#pragma once

#include "../cloud/Tone3000Client.h"
#include "../engine/ModuleId.h"
#include "../plugin/PluginProcessor.h"

#include <array>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace pedalboard {

enum class CloudLoadTargetKind : int {
    Auto     = 0, // Smart routing by gear/format: Pedal -> NAM Pedal, Amp -> Amp A, IR -> Cab A
    NamPedal = 1, // Load .nam into targeted NAM Pedal slot (or create new NAM Pedal slot)
    AmpRig   = 2, // Load .nam into Amp A / B / C (auto-switches Amp to NAM Capture mode)
    CabRig   = 3  // Load .wav/.aiff into Cab A / B / C (auto-switches Cab to User IR mode)
};

struct CloudLoadRequest {
    juce::File file;
    bool isIr = false;
    bool isPedal = false;
    bool isFullRig = false;
    CloudLoadTargetKind targetKind = CloudLoadTargetKind::Auto;
    int targetRigIndex = 0;  // 0 = A, 1 = B, 2 = C
    int targetSlotId   = -1; // >= 0 when targeting a specific NAM Pedal slot
    juce::String displayTitle;
};

// Native C++20/JUCE 8 Matte Hardware Cloud Browser for TONE3000 (.nam Amp/Pedal & .wav IR)
// Zero WebView (JUCE_WEB_BROWSER=0) — uses official TONE3000 REST API v1 + OAuth 2.0 PKCE loopback!
class Tone3000BrowserOverlay : public juce::Component {
public:
    using LoadFileCallback = std::function<void(const CloudLoadRequest&)>;

    Tone3000BrowserOverlay(PedalBoardAudioProcessor& proc,
                           t3k::Tone3000Client& cloudClient,
                           LoadFileCallback onLoadFile,
                           std::function<void()> onClose)
        : processor(proc),
          client(cloudClient),
          loadFileCb(std::move(onLoadFile)),
          closeCb(std::move(onClose)),
          toneListModel(*this),
          modelListModel(*this) {
        // Header action buttons
        keyToggleBtn.setButtonText("API KEY");
        keyToggleBtn.setTooltip("Configure your free TONE3000 Publishable Key (t3k_pub_...)");
        keyToggleBtn.onClick = [this] {
            showSetupBar = !showSetupBar;
            resized();
            repaint();
        };
        addAndMakeVisible(keyToggleBtn);

        connectBtn.setButtonText("CONNECT");
        connectBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7));
        connectBtn.onClick = [this] { handleConnectButtonClick(); };
        addAndMakeVisible(connectBtn);

        browseWebBtn.setButtonText("BROWSE WEB");
        browseWebBtn.setTooltip("Open official TONE3000 catalog in browser & auto-return picked tone (Free Tier supported!)");
        browseWebBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0f766e));
        browseWebBtn.onClick = [this] { handleSelectToneFlow(); };
        addAndMakeVisible(browseWebBtn);

        closeBtn.setButtonText("CLOSE");
        closeBtn.onClick = [this] { if (closeCb) closeCb(); };
        addAndMakeVisible(closeBtn);

        // Collapsible Setup / API Key Bar
        apiKeyEditor.setTextToShowWhenEmpty("Paste TONE3000 Publishable Key (t3k_pub_...) from tone3000.com/settings", juce::Colour(0xff64748b));
        apiKeyEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff090d14));
        apiKeyEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2c384e));
        apiKeyEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff06b6d4));
        apiKeyEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe2e8f0));
        apiKeyEditor.setText(client.getPublishableKey(), false);
        apiKeyEditor.onTextChange = [this] { client.setPublishableKey(apiKeyEditor.getText()); };
        apiKeyEditor.onReturnKey = [this] { handleConnectButtonClick(); };
        addChildComponent(apiKeyEditor);

        getKeyBtn.setButtonText("GET FREE KEY");
        getKeyBtn.setTooltip("Open tone3000.com/settings -> API Keys to generate a free t3k_pub_... key");
        getKeyBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
        getKeyBtn.onClick = [] { juce::URL("https://www.tone3000.com/settings").launchInDefaultBrowser(); };
        addChildComponent(getKeyBtn);

        setupConnectBtn.setButtonText("SIGN IN WITH TONE3000");
        setupConnectBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0891b2));
        setupConnectBtn.onClick = [this] { handleConnectButtonClick(); };
        addChildComponent(setupConnectBtn);

        // Feed Tabs: Trending | Latest | Search | Favorites | Downloads | Created | Cache
        const char* feedNames[kNumFeeds] = {
            "TRENDING", "LATEST", "SEARCH", "FAVORITES", "DOWNLOADS", "CREATED", "LOCAL CACHE"
        };
        for (int i = 0; i < kNumFeeds; ++i) {
            auto& b = feedBtns[static_cast<size_t>(i)];
            b.setButtonText(feedNames[i]);
            b.onClick = [this, i] { selectFeed(static_cast<FeedTab>(i)); };
            addAndMakeVisible(b);
        }

        // Search input + action button
        searchBox.setTextToShowWhenEmpty("Search TONE3000 (e.g. Tube Screamer, Klon, JCM800, 5150, V30, Dual Rec)...", juce::Colour(0xff64748b));
        searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0b0f17));
        searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2c3648));
        searchBox.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff06b6d4));
        searchBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        searchBox.onReturnKey = [this] { triggerSearch(); };
        addAndMakeVisible(searchBox);

        searchBtn.setButtonText("SEARCH");
        searchBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7));
        searchBtn.onClick = [this] { triggerSearch(); };
        addAndMakeVisible(searchBtn);

        // Gear filter pills
        const char* gearLabels[kNumGearFilters] = { "ALL", "AMP", "AMP+CAB", "PEDAL", "CAB IR", "OUTBOARD" };
        for (int i = 0; i < kNumGearFilters; ++i) {
            auto& b = gearBtns[static_cast<size_t>(i)];
            b.setButtonText(gearLabels[i]);
            b.onClick = [this, i] {
                activeGearFilter = i;
                currentPage = 1;
                updateToolbarStyles();
                reloadActiveFeed();
            };
            addAndMakeVisible(b);
        }

        // Sort ComboBox
        sortBox.addItem("Sort: Trending", 1);
        sortBox.addItem("Sort: Best Match", 2);
        sortBox.addItem("Sort: Newest", 3);
        sortBox.addItem("Sort: Most Downloaded", 4);
        sortBox.setSelectedId(1, juce::dontSendNotification);
        sortBox.onChange = [this] {
            if (activeFeed == FeedTab::Search) {
                currentPage = 1;
                reloadActiveFeed();
            }
        };
        addAndMakeVisible(sortBox);

        // NAM Architecture toggle (A1+Std vs A2 Fast)
        archBtn.setButtonText("NAM: A1+STD");
        archBtn.setTooltip("Toggle NAM capture architecture filter (A1/Standard covers the largest library; A2 Fast targets NAM v2 models)");
        archBtn.onClick = [this] {
            useA2Arch = !useA2Arch;
            archBtn.setButtonText(useA2Arch ? "NAM: A2 FAST" : "NAM: A1+STD");
            updateToolbarStyles();
            if (activeFeed == FeedTab::Search) {
                currentPage = 1;
                reloadActiveFeed();
            }
            if (selectedTone.id > 0) {
                loadModelsForSelectedTone();
            }
        };
        addAndMakeVisible(archBtn);

        // Left Pane: Tone List + Pagination
        toneList.setModel(&toneListModel);
        toneList.setRowHeight(60);
        toneList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0b0f17));
        toneList.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff222b3c));
        toneList.setOutlineThickness(1);
        addAndMakeVisible(toneList);

        prevPageBtn.setButtonText("< PREV");
        prevPageBtn.onClick = [this] {
            if (currentPage > 1) {
                --currentPage;
                reloadActiveFeed();
            }
        };
        addAndMakeVisible(prevPageBtn);

        nextPageBtn.setButtonText("NEXT >");
        nextPageBtn.onClick = [this] {
            if (currentPage < totalPages) {
                ++currentPage;
                reloadActiveFeed();
            }
        };
        addAndMakeVisible(nextPageBtn);

        refreshBtn.setButtonText("REFRESH");
        refreshBtn.onClick = [this] { reloadActiveFeed(); };
        addAndMakeVisible(refreshBtn);

        openCacheBtn.setButtonText("OPEN CACHE FOLDER");
        openCacheBtn.onClick = [] { t3k::Tone3000Client::getCacheDirectory().revealToUser(); };
        addAndMakeVisible(openCacheBtn);

        // Right Pane: Selected Tone Detail + Destination + Capture Models List
        targetBox.onChange = [this] { syncTargetFromCombo(); };
        addAndMakeVisible(targetBox);

        favBtn.setButtonText("FAV");
        favBtn.setTooltip("Favorite / Unfavorite this tone on your TONE3000 account");
        favBtn.onClick = [this] { handleToggleFavorite(); };
        addAndMakeVisible(favBtn);

        openWebToneBtn.setButtonText("OPEN ON TONE3000");
        openWebToneBtn.setTooltip("View creator page & notes on tone3000.com");
        openWebToneBtn.onClick = [this] {
            if (selectedTone.url.isNotEmpty()) {
                juce::URL(selectedTone.url).launchInDefaultBrowser();
            } else if (selectedTone.id > 0) {
                juce::URL("https://www.tone3000.com/tones/" + juce::String(selectedTone.id)).launchInDefaultBrowser();
            }
        };
        addAndMakeVisible(openWebToneBtn);

        modelList.setModel(&modelListModel);
        modelList.setRowHeight(44);
        modelList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff090d14));
        modelList.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff222b3c));
        modelList.setOutlineThickness(1);
        addAndMakeVisible(modelList);

        rebuildTargetCombo();
        updateAuthUiState();
        updateToolbarStyles();
    }

    ~Tone3000BrowserOverlay() override {
        toneList.setModel(nullptr);
        modelList.setModel(nullptr);
    }

    void setEmbeddedInPopup(bool embedded) noexcept {
        embeddedInPopup = embedded;
        closeBtn.setVisible(!embedded);
        resized();
        repaint();
    }

    // Open overlay pre-configured for a specific destination (Amp A/B/C, Cab A/B/C, NAM Pedal Slot, or Auto)
    void openForTarget(CloudLoadTargetKind kind, int rigOrSlotIndex = 0, const juce::String& initialQuery = {}) {
        activeTargetKind = kind;
        if (kind == CloudLoadTargetKind::NamPedal) {
            activeTargetSlotId = rigOrSlotIndex;
            activeGearFilter = 3; // PEDAL
        } else if (kind == CloudLoadTargetKind::AmpRig) {
            activeTargetRigIndex = std::clamp(rigOrSlotIndex, 0, 2);
            activeGearFilter = 1; // AMP
        } else if (kind == CloudLoadTargetKind::CabRig) {
            activeTargetRigIndex = std::clamp(rigOrSlotIndex, 0, 2);
            activeGearFilter = 4; // CAB IR
        } else {
            activeGearFilter = 0; // ALL
        }

        if (initialQuery.trim().isNotEmpty()) {
            searchBox.setText(initialQuery.trim(), false);
            currentPage = 1;
        }

        rebuildTargetCombo();
        apiKeyEditor.setText(client.getPublishableKey(), false);
        showSetupBar = !client.isConnected();
        updateAuthUiState();
        updateToolbarStyles();

        if (client.isConnected()) {
            if (client.getCachedUser().username.isEmpty()) {
                fetchUserProfile();
            }
            if (initialQuery.trim().isNotEmpty()) {
                activeFeed = FeedTab::Search;
                updateToolbarStyles();
            } else if (activeFeed == FeedTab::LocalCache) {
                activeFeed = FeedTab::Trending;
                updateToolbarStyles();
            }
            reloadActiveFeed();
        } else {
            localCacheFiles = t3k::Tone3000Client::scanLocalCache();
            if (!localCacheFiles.empty() && !client.hasPublishableKey()) {
                statusMessage = "Sign in with your free TONE3000 key to browse cloud tones, or switch to LOCAL CACHE (" +
                                juce::String(localCacheFiles.size()) + " downloaded files).";
            } else {
                statusMessage = "Enter your free TONE3000 Publishable Key (t3k_pub_...) and click SIGN IN to browse & load captures.";
            }
            toneList.updateContent();
        }

        resized();
        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override {
        if (!embeddedInPopup && !drawerRect.contains(e.getPosition())) {
            if (closeCb) closeCb();
        }
    }

    void paint(juce::Graphics& g) override {
        if (embeddedInPopup) {
            g.fillAll(juce::Colour(0xff121722));
        } else {
            g.setColour(juce::Colours::black.withAlpha(0.62f));
            g.fillRect(getLocalBounds());
        }

        const auto d = drawerRect.toFloat();
        g.setColour(juce::Colour(0xff121722));
        g.fillRoundedRectangle(d, embeddedInPopup ? 0.0f : 9.0f);
        if (!embeddedInPopup) {
            g.setColour(juce::Colour(0xff2b374c));
            g.drawRoundedRectangle(d, 9.0f, 1.4f);
        }

        // Top Header Band
        auto hdr = juce::Rectangle<float>(d.getX(), d.getY(), d.getWidth(), 44.0f);
        g.setColour(juce::Colour(0xff0d111a));
        g.fillRoundedRectangle(hdr.withBottom(hdr.getBottom() + 4.0f), 9.0f);
        g.fillRect(hdr.withTop(hdr.getBottom() - 10.0f));
        g.setColour(juce::Colour(0xff232d40));
        g.drawHorizontalLine(static_cast<int>(hdr.getBottom()), d.getX(), d.getRight());

        // TONE3000 Emblem Dot + Title
        const float dotX = d.getX() + 18.0f;
        const float dotY = hdr.getCentreY();
        const bool connected = client.isConnected();
        g.setColour(connected ? juce::Colour(0xff22d3ee) : juce::Colour(0xff64748b));
        g.fillEllipse(dotX - 4.5f, dotY - 4.5f, 9.0f, 9.0f);

        g.setColour(juce::Colour(0xfff8fafc));
        g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
        g.drawText("TONE3000 CLOUD BROWSER",
                   juce::Rectangle<float>(d.getX() + 30.0f, d.getY() + 6.0f, 226.0f, 20.0f),
                   juce::Justification::centredLeft);

        g.setColour(connected ? juce::Colour(0xff67e8f9) : juce::Colour(0xff94a3b8));
        g.setFont(juce::Font(juce::FontOptions(9.4f, juce::Font::plain)));
        const auto user = client.getCachedUser();
        const juce::String sub = connected
            ? ("Connected" + (user.bestName().isNotEmpty() ? (" as @" + user.username) : "") + "  |  NAM & IR Cloud")
            : "Neural Amp Modeler (.nam) & Cabinet IR (.wav) Cloud Library";
        g.drawText(sub,
                   juce::Rectangle<float>(d.getX() + 30.0f, d.getY() + 24.0f, 340.0f, 15.0f),
                   juce::Justification::centredLeft, true);

        // Setup Banner Panel (when visible)
        if (showSetupBar && !setupPanelRect.isEmpty()) {
            auto sr = setupPanelRect.toFloat();
            g.setColour(juce::Colour(0xff0b1320));
            g.fillRoundedRectangle(sr, 6.0f);
            g.setColour(juce::Colour(0xff0891b2).withAlpha(0.55f));
            g.drawRoundedRectangle(sr, 6.0f, 1.0f);

            g.setColour(juce::Colour(0xffcbd5e1));
            g.setFont(juce::Font(juce::FontOptions(9.8f, juce::Font::bold)));
            g.drawText("1. Create a free Publishable Key (t3k_pub_...) at tone3000.com/settings -> API Keys   2. Paste below & click Sign In (OAuth PKCE via browser)",
                       sr.reduced(10.0f, 4.0f).withHeight(16.0f), juce::Justification::centredLeft, true);
        }

        // Right Pane Detail Card Header Backdrop
        if (!detailHeaderRect.isEmpty()) {
            auto dr = detailHeaderRect.toFloat();
            g.setColour(juce::Colour(0xff0c1018));
            g.fillRoundedRectangle(dr, 6.0f);
            g.setColour(juce::Colour(0xff242e40));
            g.drawRoundedRectangle(dr, 6.0f, 1.0f);

            auto inner = dr.reduced(10.0f, 8.0f);
            if (activeFeed == FeedTab::LocalCache) {
                g.setColour(juce::Colour(0xff38bdf8));
                g.setFont(juce::Font(juce::FontOptions(12.5f, juce::Font::bold)));
                g.drawText("LOCAL TONE3000 CACHE", inner.removeFromTop(18.0f), juce::Justification::centredLeft);
                g.setColour(juce::Colour(0xff94a3b8));
                g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::plain)));
                g.drawFittedText("All captures downloaded from TONE3000 are saved locally for instant offline loading and DAW project recall. Select any file on the left to load it straight into your chosen destination.",
                                 inner.toNearestInt(), juce::Justification::topLeft, 4);
            } else if (selectedTone.id > 0) {
                auto titleRow = inner.removeFromTop(20.0f);
                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(juce::FontOptions(13.5f, juce::Font::bold)));
                g.drawText(selectedTone.title, titleRow.withTrimmedRight(138.0f), juce::Justification::centredLeft, true);

                auto metaRow = inner.removeFromTop(16.0f);
                g.setColour(juce::Colour(0xff94a3b8));
                g.setFont(juce::Font(juce::FontOptions(9.8f, juce::Font::plain)));
                const juce::String meta = "by " + selectedTone.creator.bestName()
                    + (selectedTone.creator.isVerified ? " [Verified]" : "")
                    + "  |  " + selectedTone.gearDisplayLabel()
                    + "  |  " + selectedTone.format.toUpperCase()
                    + "  |  " + selectedTone.countDisplayLabel();
                g.drawText(meta, metaRow, juce::Justification::centredLeft, true);

                if (!selectedTone.makes.isEmpty() || !selectedTone.tags.isEmpty()) {
                    juce::StringArray combined = selectedTone.makes;
                    for (int t = 0; t < std::min(5, selectedTone.tags.size()); ++t) {
                        combined.add("#" + selectedTone.tags[t]);
                    }
                    g.setColour(juce::Colour(0xff38bdf8));
                    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
                    g.drawText(combined.joinIntoString("  "), inner.removeFromTop(15.0f), juce::Justification::centredLeft, true);
                }

                if (selectedTone.description.isNotEmpty()) {
                    g.setColour(juce::Colour(0xff8a96ab));
                    g.setFont(juce::Font(juce::FontOptions(9.6f, juce::Font::plain)));
                    g.drawFittedText(selectedTone.description.replace("\n", " "),
                                     inner.withTrimmedBottom(26.0f).toNearestInt(),
                                     juce::Justification::topLeft, 3);
                }
            } else {
                g.setColour(juce::Colour(0xff64748b));
                g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::italic)));
                g.drawText("Select a tone on the left (or click BROWSE WEB) to inspect & load captures.",
                           inner, juce::Justification::centred, true);
            }
        }

        // Target label beside combo box
        if (!targetLabelRect.isEmpty()) {
            g.setColour(juce::Colour(0xff94a3b8));
            g.setFont(juce::Font(juce::FontOptions(9.6f, juce::Font::bold)));
            g.drawText("LOAD INTO:", targetLabelRect, juce::Justification::centredLeft);
        }

        // Page indicator in left footer
        if (!pageLabelRect.isEmpty()) {
            g.setColour(juce::Colour(0xff94a3b8));
            g.setFont(juce::Font(juce::FontOptions(9.8f, juce::Font::bold)));
            const juce::String pgText = (activeFeed == FeedTab::LocalCache)
                ? (juce::String(localCacheFiles.size()) + " cached files")
                : ("Page " + juce::String(currentPage) + " / " + juce::String(std::max(1, totalPages))
                   + " (" + juce::String(currentTones.size()) + " shown)");
            g.drawText(pgText, pageLabelRect, juce::Justification::centred, true);
        }

        // Bottom Status Footer Strip
        if (!statusBarRect.isEmpty()) {
            auto sb = statusBarRect.toFloat();
            g.setColour(juce::Colour(0xff0b0f17));
            g.fillRoundedRectangle(sb, 4.0f);
            g.setColour(statusIsError ? juce::Colour(0xffdc2626).withAlpha(0.70f)
                                      : juce::Colour(0xff243044));
            g.drawRoundedRectangle(sb, 4.0f, 0.9f);

            g.setColour(statusIsError ? juce::Colour(0xfffca5a5)
                                      : (isBusy ? juce::Colour(0xff38bdf8) : juce::Colour(0xffcbd5e1)));
            g.setFont(juce::Font(juce::FontOptions(10.2f, isBusy ? juce::Font::bold : juce::Font::plain)));
            g.drawText(statusMessage.isNotEmpty() ? statusMessage : "Ready.",
                       sb.reduced(10.0f, 0.0f), juce::Justification::centredLeft, true);
        }
    }

    void resized() override {
        auto full = getLocalBounds();
        if (embeddedInPopup) {
            drawerRect = full;
        } else {
            const int dw = std::min(1120, full.getWidth() - 40);
            const int dh = std::min(680, full.getHeight() - 36);
            drawerRect = full.withSizeKeepingCentre(dw, dh);
        }

        auto area = drawerRect;

        // 1. Header Bar (44px)
        auto hdr = area.removeFromTop(44).reduced(12, 8);
        if (!embeddedInPopup) {
            closeBtn.setBounds(hdr.removeFromRight(66));
            hdr.removeFromRight(6);
        } else {
            closeBtn.setBounds(0, 0, 0, 0);
        }
        browseWebBtn.setBounds(hdr.removeFromRight(102));
        hdr.removeFromRight(6);
        connectBtn.setBounds(hdr.removeFromRight(104));
        hdr.removeFromRight(6);
        keyToggleBtn.setBounds(hdr.removeFromRight(68));

        auto body = area.reduced(12, 8);

        // 2. Optional Collapsible Setup / API Key Bar
        apiKeyEditor.setVisible(showSetupBar);
        getKeyBtn.setVisible(showSetupBar);
        setupConnectBtn.setVisible(showSetupBar);
        if (showSetupBar) {
            setupPanelRect = body.removeFromTop(58);
            auto sInner = setupPanelRect.reduced(10, 6);
            sInner.removeFromTop(18);
            setupConnectBtn.setBounds(sInner.removeFromRight(162));
            sInner.removeFromRight(6);
            getKeyBtn.setBounds(sInner.removeFromRight(104));
            sInner.removeFromRight(6);
            apiKeyEditor.setBounds(sInner);
            body.removeFromTop(7);
        } else {
            setupPanelRect = {};
        }

        // 3. Feed Tabs Row
        auto feedRow = body.removeFromTop(24);
        const int feedGap = 4;
        const int feedW = (feedRow.getWidth() - feedGap * (kNumFeeds - 1)) / kNumFeeds;
        for (int i = 0; i < kNumFeeds; ++i) {
            auto r = (i + 1 == kNumFeeds) ? feedRow : feedRow.removeFromLeft(feedW);
            feedBtns[static_cast<size_t>(i)].setBounds(r);
            if (i + 1 < kNumFeeds) feedRow.removeFromLeft(feedGap);
        }
        body.removeFromTop(6);

        // 4. Search + Gear Filter + Sort/Arch Row
        auto searchRow = body.removeFromTop(26);
        archBtn.setBounds(searchRow.removeFromRight(98));
        searchRow.removeFromRight(5);
        sortBox.setBounds(searchRow.removeFromRight(146));
        searchRow.removeFromRight(5);
        searchBtn.setBounds(searchRow.removeFromRight(76));
        searchRow.removeFromRight(5);
        searchBox.setBounds(searchRow);
        body.removeFromTop(5);

        auto gearRow = body.removeFromTop(22);
        const int gearW = 74;
        for (int i = 0; i < kNumGearFilters; ++i) {
            gearBtns[static_cast<size_t>(i)].setBounds(gearRow.removeFromLeft(gearW));
            gearRow.removeFromLeft(4);
        }
        body.removeFromTop(7);

        // 5. Bottom Status Bar
        statusBarRect = body.removeFromBottom(24);
        body.removeFromBottom(6);

        // 6. Split 2-Column Content Area (Left: Tones List, Right: Tone Detail + Models)
        const int leftW = body.getWidth() * 55 / 100;
        auto leftPane = body.removeFromLeft(leftW);
        body.removeFromLeft(10);
        auto rightPane = body;

        // Left Pane footer controls
        auto leftFooter = leftPane.removeFromBottom(24);
        leftPane.removeFromBottom(5);
        toneList.setBounds(leftPane);

        const bool isCache = (activeFeed == FeedTab::LocalCache);
        prevPageBtn.setVisible(!isCache);
        nextPageBtn.setVisible(!isCache);
        openCacheBtn.setVisible(isCache);

        if (isCache) {
            openCacheBtn.setBounds(leftFooter.removeFromRight(144));
            leftFooter.removeFromRight(6);
            refreshBtn.setBounds(leftFooter.removeFromLeft(74));
            pageLabelRect = leftFooter;
        } else {
            refreshBtn.setBounds(leftFooter.removeFromRight(74));
            leftFooter.removeFromRight(6);
            nextPageBtn.setBounds(leftFooter.removeFromRight(64));
            leftFooter.removeFromRight(4);
            prevPageBtn.setBounds(leftFooter.removeFromLeft(64));
            pageLabelRect = leftFooter;
        }

        // Right Pane: Detail Header + Target Selector + Captures List
        const int detailH = std::clamp(rightPane.getHeight() * 34 / 100, 114, 150);
        detailHeaderRect = rightPane.removeFromTop(detailH);

        auto dhBtns = detailHeaderRect.reduced(10, 8);
        auto topRightBtns = dhBtns.removeFromTop(22);
        favBtn.setVisible(!isCache && selectedTone.id > 0);
        openWebToneBtn.setVisible(!isCache && selectedTone.id > 0);
        if (favBtn.isVisible()) {
            favBtn.setBounds(topRightBtns.removeFromRight(52));
            topRightBtns.removeFromRight(4);
            openWebToneBtn.setBounds(dhBtns.removeFromBottom(22).removeFromRight(130));
        }

        rightPane.removeFromTop(6);
        auto targetRow = rightPane.removeFromTop(24);
        targetLabelRect = targetRow.removeFromLeft(66);
        targetBox.setBounds(targetRow);
        rightPane.removeFromTop(6);

        modelList.setVisible(!isCache);
        if (!isCache) {
            modelList.setBounds(rightPane);
        }
    }

private:
    enum class FeedTab : int {
        Trending = 0,
        Latest,
        Search,
        Favorites,
        Downloads,
        Created,
        LocalCache
    };

    static constexpr int kNumFeeds = 7;
    static constexpr int kNumGearFilters = 6;

    // -------------------------------------------------------------------------
    // Left Pane ListBoxModel (Cloud Tones OR Local Cached Files)
    // -------------------------------------------------------------------------
    class ToneListModel : public juce::ListBoxModel {
    public:
        explicit ToneListModel(Tone3000BrowserOverlay& o) : owner(o) {}

        int getNumRows() override {
            if (owner.activeFeed == FeedTab::LocalCache) {
                return static_cast<int>(owner.localCacheFiles.size());
            }
            return static_cast<int>(owner.currentTones.size());
        }

        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override {
            auto r = juce::Rectangle<float>(3.0f, 2.0f, static_cast<float>(w - 6), static_cast<float>(h - 4));
            if (owner.activeFeed == FeedTab::LocalCache) {
                if (row < 0 || row >= static_cast<int>(owner.localCacheFiles.size())) return;
                const auto& cf = owner.localCacheFiles[static_cast<size_t>(row)];
                g.setColour(selected ? juce::Colour(0xff192538) : juce::Colour(0xff111621));
                g.fillRoundedRectangle(r, 6.0f);
                g.setColour(selected ? juce::Colour(0xff06b6d4) : juce::Colour(0xff232d3f));
                g.drawRoundedRectangle(r, 6.0f, selected ? 1.3f : 0.8f);

                auto inner = r.reduced(10.0f, 6.0f);
                auto loadPill = inner.removeFromRight(62.0f).reduced(2.0f, 10.0f);
                g.setColour(juce::Colour(0xff0284c7));
                g.fillRoundedRectangle(loadPill, 4.0f);
                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
                g.drawText("LOAD", loadPill, juce::Justification::centred);

                auto top = inner.removeFromTop(20.0f);
                const auto pillCol = cf.isIr ? juce::Colour(0xff22c55e) : juce::Colour(0xff06b6d4);
                auto badge = top.removeFromLeft(44.0f).reduced(0.0f, 2.0f);
                g.setColour(pillCol.withAlpha(0.22f));
                g.fillRoundedRectangle(badge, 3.5f);
                g.setColour(pillCol);
                g.drawRoundedRectangle(badge, 3.5f, 0.8f);
                g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
                g.drawText(cf.isIr ? "IR" : "NAM", badge, juce::Justification::centred);

                top.removeFromLeft(8.0f);
                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(juce::FontOptions(12.2f, juce::Font::bold)));
                g.drawText(cf.displayName, top, juce::Justification::centredLeft, true);

                g.setColour(juce::Colour(0xff8491a6));
                g.setFont(juce::Font(juce::FontOptions(9.4f, juce::Font::plain)));
                const juce::String meta = cf.file.getFileName() + "  |  " + juce::String(cf.sizeBytes / 1024) + " KB";
                g.drawText(meta, inner, juce::Justification::centredLeft, true);
                return;
            }

            if (row < 0 || row >= static_cast<int>(owner.currentTones.size())) return;
            const auto& t = owner.currentTones[static_cast<size_t>(row)];
            const bool isActive = (t.id == owner.selectedTone.id);

            g.setColour(isActive ? juce::Colour(0xff172336) : (selected ? juce::Colour(0xff141c2b) : juce::Colour(0xff101520)));
            g.fillRoundedRectangle(r, 6.0f);
            g.setColour(isActive ? juce::Colour(0xff06b6d4) : juce::Colour(0xff232c3d));
            g.drawRoundedRectangle(r, 6.0f, isActive ? 1.4f : 0.8f);

            auto inner = r.reduced(10.0f, 5.0f);
            auto topRow = inner.removeFromTop(21.0f);

            auto drawPill = [&](const juce::String& txt, juce::Colour col) {
                const float pw = std::max(38.0f, static_cast<float>(txt.length()) * 5.8f + 10.0f);
                auto pr = topRow.removeFromRight(pw).reduced(1.5f, 2.0f);
                g.setColour(col.withAlpha(0.20f));
                g.fillRoundedRectangle(pr, 3.5f);
                g.setColour(col);
                g.drawRoundedRectangle(pr, 3.5f, 0.8f);
                g.setFont(juce::Font(juce::FontOptions(8.2f, juce::Font::bold)));
                g.drawText(txt, pr, juce::Justification::centred);
            };

            drawPill(t.format.toUpperCase(), t.isIrTone() ? juce::Colour(0xff22c55e) : juce::Colour(0xff38bdf8));
            drawPill(t.gearDisplayLabel(), gearBadgeColour(t.gear));

            g.setColour(isActive ? juce::Colour(0xff67e8f9) : juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(12.2f, juce::Font::bold)));
            g.drawText(t.title, topRow.withTrimmedRight(6.0f), juce::Justification::centredLeft, true);

            auto botRow = inner;
            const juce::String stats = t.countDisplayLabel()
                + "  |  " + juce::String(t.downloadsCount) + " dl"
                + (t.isFavorite ? "  |  FAV" : "");
            g.setColour(juce::Colour(0xff64748b));
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.drawText(stats, botRow.removeFromRight(156.0f), juce::Justification::centredRight, true);

            g.setColour(juce::Colour(0xff94a3b8));
            g.setFont(juce::Font(juce::FontOptions(9.6f, juce::Font::plain)));
            const juce::String byline = "by " + t.creator.bestName()
                + (t.makes.isEmpty() ? "" : ("  |  " + t.makes.joinIntoString(", ")));
            g.drawText(byline, botRow, juce::Justification::centredLeft, true);
        }

        void listBoxItemClicked(int row, const juce::MouseEvent&) override {
            owner.handleToneListRowClick(row);
        }

        void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override {
            owner.handleToneListRowClick(row);
        }

    private:
        static juce::Colour gearBadgeColour(const juce::String& g) noexcept {
            if (g == "pedal")    return juce::Colour(0xff06b6d4);
            if (g == "amp")      return juce::Colour(0xfff59e0b);
            if (g == "amp-cab" || g == "full-rig") return juce::Colour(0xfff97316);
            if (g == "cab" || g == "ir") return juce::Colour(0xff22c55e);
            if (g == "outboard") return juce::Colour(0xffa855f7);
            return juce::Colour(0xff64748b);
        }

        Tone3000BrowserOverlay& owner;
    };

    // -------------------------------------------------------------------------
    // Right Pane ListBoxModel (Captures / Models inside Selected Tone)
    // -------------------------------------------------------------------------
    class ModelListModel : public juce::ListBoxModel {
    public:
        explicit ModelListModel(Tone3000BrowserOverlay& o) : owner(o) {}

        int getNumRows() override {
            return static_cast<int>(owner.selectedToneModels.size());
        }

        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override {
            if (row < 0 || row >= static_cast<int>(owner.selectedToneModels.size())) return;
            const auto& m = owner.selectedToneModels[static_cast<size_t>(row)];
            const bool cached = t3k::Tone3000Client::isModelCached(m);
            const bool downloading = (owner.downloadingModelId == m.id);

            auto r = juce::Rectangle<float>(3.0f, 2.0f, static_cast<float>(w - 6), static_cast<float>(h - 4));
            g.setColour(selected ? juce::Colour(0xff172336) : juce::Colour(0xff0f141e));
            g.fillRoundedRectangle(r, 5.0f);
            g.setColour(downloading ? juce::Colour(0xfff59e0b)
                                    : (selected ? juce::Colour(0xff06b6d4) : juce::Colour(0xff232c3d)));
            g.drawRoundedRectangle(r, 5.0f, downloading ? 1.3f : 0.8f);

            auto inner = r.reduced(8.0f, 4.0f);
            auto loadBtnRect = inner.removeFromRight(72.0f).reduced(1.0f, 4.0f);
            g.setColour(downloading ? juce::Colour(0xffb45309)
                                    : (cached ? juce::Colour(0xff0f766e) : juce::Colour(0xff0284c7)));
            g.fillRoundedRectangle(loadBtnRect, 4.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(9.2f, juce::Font::bold)));
            g.drawText(downloading ? "WAIT..." : (cached ? "LOAD" : "GET & LOAD"), loadBtnRect, juce::Justification::centred);

            inner.removeFromRight(6.0f);
            auto top = inner.removeFromTop(18.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(11.2f, juce::Font::bold)));
            g.drawText(m.name, top, juce::Justification::centredLeft, true);

            juce::String meta = m.fileExtension().toUpperCase();
            if (m.architectureVersion.isNotEmpty()) meta += "  |  Arch " + m.architectureVersion;
            if (m.size.isNotEmpty())                meta += "  |  " + m.size;
            if (cached)                             meta += "  |  CACHED";

            g.setColour(cached ? juce::Colour(0xff34d399) : juce::Colour(0xff8491a6));
            g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::plain)));
            g.drawText(meta, inner, juce::Justification::centredLeft, true);
        }

        void listBoxItemClicked(int row, const juce::MouseEvent&) override {
            owner.handleModelRowLoad(row);
        }

        void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override {
            owner.handleModelRowLoad(row);
        }

    private:
        Tone3000BrowserOverlay& owner;
    };

    // -------------------------------------------------------------------------
    // Actions & State Sync
    // -------------------------------------------------------------------------
    void rebuildTargetCombo() {
        const auto mode = processor.getRoutingMode();
        const int rigCount = (mode == RoutingMode::TripleParallel) ? 3 : (isParallelRouting(mode) ? 2 : 1);

        targetBox.clear(juce::dontSendNotification);
        targetBox.addItem("Auto (Pedal -> NAM Pedal, Amp -> Amp A, IR -> Cab A)", 1);
        const juce::String pedalLabel = (activeTargetSlotId >= 0)
            ? ("NAM Pedal (Slot #" + juce::String(activeTargetSlotId + 1) + ")")
            : "NAM Pedal (Add / Active Slot)";
        targetBox.addItem(pedalLabel, 2);
        targetBox.addItem("Amp A (NAM Capture)", 10);
        if (rigCount >= 2) targetBox.addItem("Amp B (NAM Capture)", 11);
        if (rigCount >= 3) targetBox.addItem("Amp C (NAM Capture)", 12);
        targetBox.addItem("Cab A (User IR)", 20);
        if (rigCount >= 2) targetBox.addItem("Cab B (User IR)", 21);
        if (rigCount >= 3) targetBox.addItem("Cab C (User IR)", 22);

        int selId = 1;
        if (activeTargetKind == CloudLoadTargetKind::NamPedal) {
            selId = 2;
        } else if (activeTargetKind == CloudLoadTargetKind::AmpRig) {
            selId = 10 + std::clamp(activeTargetRigIndex, 0, rigCount - 1);
        } else if (activeTargetKind == CloudLoadTargetKind::CabRig) {
            selId = 20 + std::clamp(activeTargetRigIndex, 0, rigCount - 1);
        }
        targetBox.setSelectedId(selId, juce::dontSendNotification);
    }

    void syncTargetFromCombo() {
        const int id = targetBox.getSelectedId();
        if (id == 1) {
            activeTargetKind = CloudLoadTargetKind::Auto;
        } else if (id == 2) {
            activeTargetKind = CloudLoadTargetKind::NamPedal;
        } else if (id >= 10 && id <= 12) {
            activeTargetKind = CloudLoadTargetKind::AmpRig;
            activeTargetRigIndex = id - 10;
        } else if (id >= 20 && id <= 22) {
            activeTargetKind = CloudLoadTargetKind::CabRig;
            activeTargetRigIndex = id - 20;
        }
    }

    juce::String currentGearApiValue() const {
        switch (activeGearFilter) {
            case 1:  return "amp";
            case 2:  return "amp-cab";
            case 3:  return "pedal";
            case 4:  return "cab";
            case 5:  return "outboard";
            default: return {};
        }
    }

    juce::String currentFormatApiValue() const {
        if (activeGearFilter == 4) return "ir";
        if (activeGearFilter >= 1 && activeGearFilter <= 3) return "nam";
        return {};
    }

    juce::String currentSortApiValue() const {
        switch (sortBox.getSelectedId()) {
            case 2:  return "best-match";
            case 3:  return "newest";
            case 4:  return "downloads-all-time";
            default: return "trending";
        }
    }

    void setStatus(const juce::String& msg, bool err = false, bool busy = false) {
        statusMessage = msg;
        statusIsError = err;
        isBusy = busy;
        repaint();
    }

    void updateAuthUiState() {
        const bool connected = client.isConnected();
        const bool authActive = client.isAuthFlowActive();

        if (authActive) {
            connectBtn.setButtonText("CANCEL AUTH");
            connectBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb45309));
            setupConnectBtn.setButtonText("CANCEL SIGN-IN");
        } else if (connected) {
            connectBtn.setButtonText("DISCONNECT");
            connectBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2c1a1f));
            setupConnectBtn.setButtonText("RECONNECT ACCOUNT");
        } else {
            connectBtn.setButtonText("CONNECT");
            connectBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7));
            setupConnectBtn.setButtonText("SIGN IN WITH TONE3000");
        }
        repaint();
    }

    void updateToolbarStyles() {
        for (int i = 0; i < kNumFeeds; ++i) {
            const bool sel = (static_cast<int>(activeFeed) == i);
            feedBtns[static_cast<size_t>(i)].setColour(juce::TextButton::buttonColourId,
                                                       sel ? juce::Colour(0xff0891b2) : juce::Colour(0xff19202e));
            feedBtns[static_cast<size_t>(i)].setColour(juce::TextButton::textColourOffId,
                                                       sel ? juce::Colours::white : juce::Colour(0xff94a3b8));
            feedBtns[static_cast<size_t>(i)].repaint();
        }
        for (int i = 0; i < kNumGearFilters; ++i) {
            const bool sel = (activeGearFilter == i);
            gearBtns[static_cast<size_t>(i)].setColour(juce::TextButton::buttonColourId,
                                                       sel ? juce::Colour(0xff2563eb) : juce::Colour(0xff171d29));
            gearBtns[static_cast<size_t>(i)].setColour(juce::TextButton::textColourOffId,
                                                       sel ? juce::Colours::white : juce::Colour(0xff94a3b8));
            gearBtns[static_cast<size_t>(i)].repaint();
        }
        archBtn.setColour(juce::TextButton::buttonColourId,
                          useA2Arch ? juce::Colour(0xff0f766e) : juce::Colour(0xff1b2232));
        favBtn.setColour(juce::TextButton::buttonColourId,
                         selectedTone.isFavorite ? juce::Colour(0xffbe123c) : juce::Colour(0xff1c2333));
        favBtn.setButtonText(selectedTone.isFavorite ? "FAVED" : "FAV");
    }

    void handleConnectButtonClick() {
        if (client.isAuthFlowActive()) {
            client.cancelActiveAuthFlow();
            updateAuthUiState();
            setStatus("Sign-in canceled.");
            return;
        }
        if (client.isConnected() && !showSetupBar) {
            client.disconnect();
            currentTones.clear();
            selectedTone = {};
            selectedToneModels.clear();
            toneList.updateContent();
            modelList.updateContent();
            showSetupBar = true;
            updateAuthUiState();
            resized();
            setStatus("Disconnected from TONE3000.");
            return;
        }

        const auto key = apiKeyEditor.getText().trim();
        if (key.isEmpty()) {
            showSetupBar = true;
            resized();
            setStatus("Paste your free TONE3000 Publishable Key (t3k_pub_...) first, or click GET FREE KEY.", true);
            return;
        }
        client.setPublishableKey(key);
        setStatus("Opening TONE3000 sign-in in your browser...", false, true);

        juce::Component::SafePointer<Tone3000BrowserOverlay> safeThis(this);
        client.startConnectFlow([safeThis](t3k::OAuthFlowResult res) {
            if (safeThis == nullptr) return;
            safeThis->updateAuthUiState();
            if (res.connected) {
                safeThis->showSetupBar = false;
                safeThis->resized();
                safeThis->setStatus("Connected to TONE3000! Loading tones...", false, true);
                safeThis->fetchUserProfile();
                safeThis->reloadActiveFeed();
            } else if (res.canceled) {
                safeThis->setStatus("Sign-in canceled.");
            } else {
                safeThis->setStatus(res.errorMessage.isNotEmpty() ? res.errorMessage : "Could not connect to TONE3000.", true);
            }
        });
        updateAuthUiState();
    }

    void handleSelectToneFlow() {
        if (client.isAuthFlowActive()) {
            client.cancelActiveAuthFlow();
            updateAuthUiState();
            setStatus("Canceled web browse.");
            return;
        }
        const auto key = apiKeyEditor.getText().trim();
        if (key.isNotEmpty()) {
            client.setPublishableKey(key);
        }
        if (!client.hasPublishableKey()) {
            showSetupBar = true;
            resized();
            setStatus("Enter your free TONE3000 Publishable Key (t3k_pub_...) first to use Browse on TONE3000.", true);
            return;
        }

        juce::String gears;
        juce::String fmt;
        if (activeGearFilter == 1)      { gears = "amp"; fmt = "nam"; }
        else if (activeGearFilter == 2) { gears = "amp-cab"; fmt = "nam"; }
        else if (activeGearFilter == 3) { gears = "pedal"; fmt = "nam"; }
        else if (activeGearFilter == 4) { gears = "cab"; fmt = "ir"; }
        else if (activeGearFilter == 5) { gears = "outboard"; fmt = "nam"; }

        setStatus("Opened TONE3000 catalog in browser — pick any tone to load it here...", false, true);
        juce::Component::SafePointer<Tone3000BrowserOverlay> safeThis(this);
        client.startSelectToneFlow(gears, fmt, useA2Arch ? 2 : 0, [safeThis](t3k::OAuthFlowResult res) {
            if (safeThis == nullptr) return;
            safeThis->updateAuthUiState();
            if (res.connected) {
                safeThis->showSetupBar = false;
                safeThis->resized();
                safeThis->fetchUserProfile();
                if (res.selectedToneId > 0) {
                    safeThis->fetchPickedToneById(res.selectedToneId);
                } else if (res.canceled) {
                    safeThis->setStatus("Connected to TONE3000 (no tone picked).");
                    safeThis->reloadActiveFeed();
                }
            } else if (res.canceled) {
                safeThis->setStatus("Browse canceled.");
            } else {
                safeThis->setStatus(res.errorMessage.isNotEmpty() ? res.errorMessage : "Browse flow failed.", true);
            }
        });
        updateAuthUiState();
    }

    void fetchUserProfile() {
        juce::Component::SafePointer<Tone3000BrowserOverlay> safeThis(this);
        client.fetchCurrentUser([safeThis](t3k::ApiResult<t3k::UserInfo> r) {
            if (safeThis == nullptr) return;
            if (r.ok()) safeThis->repaint();
        });
    }

    void fetchPickedToneById(int toneId) {
        setStatus("Fetching selected TONE3000 tone #" + juce::String(toneId) + "...", false, true);
        juce::Component::SafePointer<Tone3000BrowserOverlay> safeThis(this);
        client.fetchToneWithModels(toneId, useA2Arch ? 2 : 0,
                                   [safeThis](t3k::ApiResult<std::pair<t3k::ToneItem, std::vector<t3k::ModelItem>>> r) {
            if (safeThis == nullptr) return;
            if (!r.ok()) {
                safeThis->setStatus(r.error.friendlyMessage("Load tone"), true);
                return;
            }
            safeThis->selectedTone = r.value->first;
            safeThis->selectedToneModels = r.value->second;
            // Prepend to left list if not already present so user can see it immediately
            bool found = false;
            for (const auto& t : safeThis->currentTones) {
                if (t.id == safeThis->selectedTone.id) { found = true; break; }
            }
            if (!found) {
                safeThis->currentTones.insert(safeThis->currentTones.begin(), safeThis->selectedTone);
                safeThis->toneList.updateContent();
                safeThis->toneList.selectRow(0);
            }
            safeThis->modelList.updateContent();
            safeThis->updateToolbarStyles();
            safeThis->resized();
            safeThis->setStatus("Loaded \"" + safeThis->selectedTone.title + "\" (" +
                                juce::String(safeThis->selectedToneModels.size()) + " captures). Click any capture on the right to load!");
        });
    }

    void selectFeed(FeedTab tab) {
        activeFeed = tab;
        currentPage = 1;
        updateToolbarStyles();
        resized();
        reloadActiveFeed();
    }

    void triggerSearch() {
        if (searchBox.getText().trim().isNotEmpty()) {
            activeFeed = FeedTab::Search;
            sortBox.setSelectedId(2, juce::dontSendNotification); // Best Match
        }
        currentPage = 1;
        updateToolbarStyles();
        resized();
        reloadActiveFeed();
    }

    void reloadActiveFeed() {
        if (activeFeed == FeedTab::LocalCache) {
            localCacheFiles = t3k::Tone3000Client::scanLocalCache();
            toneList.updateContent();
            setStatus("Local cache: " + juce::String(localCacheFiles.size()) + " downloaded .nam / .wav captures.");
            return;
        }

        if (!client.isConnected()) {
            showSetupBar = true;
            resized();
            setStatus("Connect your TONE3000 account above to browse cloud tones (or open LOCAL CACHE).", false);
            return;
        }

        const int reqGen = ++feedRequestGen;
        juce::Component::SafePointer<Tone3000BrowserOverlay> safeThis(this);
        auto onPage = [safeThis, reqGen](t3k::ApiResult<t3k::TonePage> r) {
            if (safeThis == nullptr || reqGen != safeThis->feedRequestGen) return;
            if (!r.ok()) {
                if (r.error.status == 401) {
                    safeThis->updateAuthUiState();
                }
                safeThis->setStatus(r.error.friendlyMessage("Browse"), true);
                return;
            }
            safeThis->currentTones = std::move(r.value->items);
            safeThis->totalPages   = std::max(1, r.value->totalPages);
            safeThis->toneList.updateContent();
            safeThis->toneList.deselectAllRows();
            if (!safeThis->currentTones.empty()) {
                safeThis->toneList.selectRow(0);
                safeThis->handleToneListRowClick(0);
            } else {
                safeThis->selectedTone = {};
                safeThis->selectedToneModels.clear();
                safeThis->modelList.updateContent();
                safeThis->resized();
            }
            safeThis->setStatus("Found " + juce::String(safeThis->currentTones.size()) + " tones (page " +
                                juce::String(safeThis->currentPage) + "/" + juce::String(safeThis->totalPages) + ").");
        };

        const auto gear = currentGearApiValue();
        const auto query = searchBox.getText().trim();

        switch (activeFeed) {
            case FeedTab::Trending:
                setStatus("Loading Trending tones from TONE3000...", false, true);
                client.fetchTrending(gear, std::move(onPage));
                break;
            case FeedTab::Latest:
                setStatus("Loading Latest tones from TONE3000...", false, true);
                client.fetchLatest(std::move(onPage));
                break;
            case FeedTab::Search: {
                t3k::SearchQuery sq;
                sq.query        = query;
                sq.gear         = gear;
                sq.format       = currentFormatApiValue();
                sq.sort         = currentSortApiValue();
                sq.architecture = useA2Arch ? 2 : 0;
                sq.page         = currentPage;
                sq.pageSize     = 24;
                setStatus(query.isNotEmpty() ? ("Searching TONE3000 for \"" + query + "\"...") : "Searching TONE3000...", false, true);
                client.searchTones(sq, std::move(onPage));
                break;
            }
            case FeedTab::Favorites:
                setStatus("Loading your Favorited tones...", false, true);
                client.fetchLibrary("favorited", gear, query, currentPage, std::move(onPage));
                break;
            case FeedTab::Downloads:
                setStatus("Loading your Downloaded tones...", false, true);
                client.fetchLibrary("downloaded", gear, query, currentPage, std::move(onPage));
                break;
            case FeedTab::Created:
                setStatus("Loading your Created tones...", false, true);
                client.fetchLibrary("created", gear, query, currentPage, std::move(onPage));
                break;
            case FeedTab::LocalCache:
                break;
        }
    }

    void handleToneListRowClick(int row) {
        if (activeFeed == FeedTab::LocalCache) {
            if (row < 0 || row >= static_cast<int>(localCacheFiles.size())) return;
            const auto& cf = localCacheFiles[static_cast<size_t>(row)];
            CloudLoadRequest req;
            req.file = cf.file;
            req.isIr = cf.isIr;
            req.isPedal = (activeTargetKind == CloudLoadTargetKind::NamPedal);
            req.targetKind = activeTargetKind;
            req.targetRigIndex = activeTargetRigIndex;
            req.targetSlotId = activeTargetSlotId;
            req.displayTitle = cf.displayName;
            if (loadFileCb) loadFileCb(req);
            setStatus("Loaded cached capture: " + cf.displayName);
            return;
        }

        if (row < 0 || row >= static_cast<int>(currentTones.size())) return;
        selectedTone = currentTones[static_cast<size_t>(row)];
        selectedToneModels.clear();
        modelList.updateContent();
        updateToolbarStyles();
        resized();
        loadModelsForSelectedTone();
    }

    void loadModelsForSelectedTone() {
        if (selectedTone.id <= 0) return;
        const int toneId = selectedTone.id;
        setStatus("Loading captures for \"" + selectedTone.title + "\"...", false, true);

        juce::Component::SafePointer<Tone3000BrowserOverlay> safeThis(this);
        client.fetchModelsForTone(toneId, useA2Arch ? 2 : 0,
                                  [safeThis, toneId](t3k::ApiResult<std::vector<t3k::ModelItem>> r) {
            if (safeThis == nullptr || safeThis->selectedTone.id != toneId) return;
            if (!r.ok()) {
                safeThis->setStatus(r.error.friendlyMessage("Load captures"), true);
                return;
            }
            safeThis->selectedToneModels = std::move(*r.value);
            safeThis->modelList.updateContent();
            safeThis->setStatus("Select any capture below (" + juce::String(safeThis->selectedToneModels.size()) +
                                " available) to download & load instantly.");
        });
    }

    void handleToggleFavorite() {
        if (selectedTone.id <= 0 || !client.isConnected()) return;
        const int toneId = selectedTone.id;
        const bool nextFav = !selectedTone.isFavorite;
        juce::Component::SafePointer<Tone3000BrowserOverlay> safeThis(this);
        client.setToneFavorite(toneId, nextFav, [safeThis, toneId](t3k::ApiResult<bool> r) {
            if (safeThis == nullptr) return;
            if (!r.ok()) {
                safeThis->setStatus(r.error.friendlyMessage("Favorite"), true);
                return;
            }
            const bool favNow = *r.value;
            if (safeThis->selectedTone.id == toneId) {
                safeThis->selectedTone.isFavorite = favNow;
            }
            for (auto& t : safeThis->currentTones) {
                if (t.id == toneId) t.isFavorite = favNow;
            }
            safeThis->updateToolbarStyles();
            safeThis->toneList.repaint();
            safeThis->setStatus(favNow ? "Added to your TONE3000 Favorites." : "Removed from Favorites.");
        });
    }

    void handleModelRowLoad(int row) {
        if (row < 0 || row >= static_cast<int>(selectedToneModels.size())) return;
        const auto model = selectedToneModels[static_cast<size_t>(row)];
        const auto toneSnapshot = selectedTone;

        downloadingModelId = model.id;
        modelList.repaint();
        setStatus("Downloading \"" + model.name + "\" from TONE3000...", false, true);

        juce::Component::SafePointer<Tone3000BrowserOverlay> safeThis(this);
        client.downloadModelFile(model, [safeThis, model, toneSnapshot](t3k::ApiResult<juce::File> r) {
            if (safeThis == nullptr) return;
            safeThis->downloadingModelId = 0;
            safeThis->modelList.repaint();
            if (!r.ok()) {
                safeThis->setStatus(r.error.friendlyMessage("Download"), true);
                return;
            }

            CloudLoadRequest req;
            req.file           = *r.value;
            req.isIr           = model.isIrFile() || toneSnapshot.isIrTone();
            req.isPedal        = toneSnapshot.isPedalTone();
            req.isFullRig      = (toneSnapshot.gear == "amp-cab" || toneSnapshot.gear == "full-rig");
            req.targetKind     = safeThis->activeTargetKind;
            req.targetRigIndex = safeThis->activeTargetRigIndex;
            req.targetSlotId   = safeThis->activeTargetSlotId;
            req.displayTitle   = model.name;

            if (safeThis->loadFileCb) {
                safeThis->loadFileCb(req);
            }

            juce::String where = req.isIr ? "Cabinet User IR" : (req.isPedal ? "NAM Pedal" : "Amp NAM Capture");
            if (req.targetKind == CloudLoadTargetKind::NamPedal) where = "NAM Pedal";
            else if (req.targetKind == CloudLoadTargetKind::AmpRig) where = "Amp " + juce::String(req.targetRigIndex == 2 ? "C" : (req.targetRigIndex == 1 ? "B" : "A")) + " (NAM)";
            else if (req.targetKind == CloudLoadTargetKind::CabRig) where = "Cab " + juce::String(req.targetRigIndex == 2 ? "C" : (req.targetRigIndex == 1 ? "B" : "A")) + " (User IR)";

            juce::String tip = req.isFullRig ? "  (Tip: Bypass CAB for Amp+Cab full-rig captures)" : "";
            safeThis->setStatus("Loaded \"" + model.name + "\" -> " + where + "!" + tip);
        });
    }

    PedalBoardAudioProcessor& processor;
    t3k::Tone3000Client& client;
    LoadFileCallback loadFileCb;
    std::function<void()> closeCb;

    ToneListModel toneListModel;
    ModelListModel modelListModel;

    FeedTab activeFeed = FeedTab::Trending;
    int activeGearFilter = 0; // 0=All, 1=Amp, 2=Amp+Cab, 3=Pedal, 4=Cab IR, 5=Outboard
    bool useA2Arch = false;
    bool showSetupBar = true;
    bool embeddedInPopup = false;
    int currentPage = 1;
    int totalPages = 1;
    int feedRequestGen = 0;
    int downloadingModelId = 0;

    CloudLoadTargetKind activeTargetKind = CloudLoadTargetKind::Auto;
    int activeTargetRigIndex = 0;
    int activeTargetSlotId = -1;

    std::vector<t3k::ToneItem> currentTones;
    t3k::ToneItem selectedTone;
    std::vector<t3k::ModelItem> selectedToneModels;
    std::vector<t3k::CachedLocalFile> localCacheFiles;

    juce::String statusMessage;
    bool statusIsError = false;
    bool isBusy = false;

    juce::Rectangle<int> drawerRect;
    juce::Rectangle<int> setupPanelRect;
    juce::Rectangle<int> detailHeaderRect;
    juce::Rectangle<int> targetLabelRect;
    juce::Rectangle<int> pageLabelRect;
    juce::Rectangle<int> statusBarRect;

    // Header controls
    juce::TextButton keyToggleBtn, connectBtn, browseWebBtn, closeBtn;
    // Setup bar controls
    juce::TextEditor apiKeyEditor;
    juce::TextButton getKeyBtn, setupConnectBtn;
    // Filter toolbar controls
    std::array<juce::TextButton, kNumFeeds> feedBtns;
    juce::TextEditor searchBox;
    juce::TextButton searchBtn, archBtn;
    juce::ComboBox sortBox;
    std::array<juce::TextButton, kNumGearFilters> gearBtns;
    // Left pane controls
    juce::ListBox toneList;
    juce::TextButton prevPageBtn, nextPageBtn, refreshBtn, openCacheBtn;
    // Right pane controls
    juce::ComboBox targetBox;
    juce::TextButton favBtn, openWebToneBtn;
    juce::ListBox modelList;
};

} // namespace pedalboard
