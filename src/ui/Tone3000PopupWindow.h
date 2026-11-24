#pragma once

#include "../cloud/Tone3000Client.h"
#include "../plugin/PluginProcessor.h"
#include "Tone3000BrowserOverlay.h"

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include <vector>

namespace pedalboard {

// ==============================================================================
// Floating Resizable TONE3000 Popup Window with Embedded Microsoft Edge WebView2
// - Tab 1: TONE3000 WEB (live tone3000.com + official OAuth PKCE select_tone flow
//          intercepted directly inside the popup -> instant capture drawer & 1-click Load!)
// - Tab 2: LIBRARY & CACHE (native fast Trending/Search/Favorites/Downloads/Local Cache)
// ==============================================================================
class Tone3000PopupContent : public juce::Component {
public:
    static constexpr const char* kEmbeddedRedirectUri = "http://127.0.0.1/callback";

    Tone3000PopupContent(PedalBoardAudioProcessor& proc,
                         t3k::Tone3000Client& cloudClient,
                         Tone3000BrowserOverlay::LoadFileCallback onLoadFile)
        : processor(proc),
          client(cloudClient),
          loadFileCb(std::move(onLoadFile)),
          pickedModelListModel(*this) {
        // Mode switch buttons
        tabWebBtn.setButtonText("TONE3000 WEB");
        tabWebBtn.onClick = [this] { setMode(Mode::WebCatalog); };
        addAndMakeVisible(tabWebBtn);

        tabLibBtn.setButtonText("LIBRARY & CACHE");
        tabLibBtn.onClick = [this] { setMode(Mode::NativeLibrary); };
        addAndMakeVisible(tabLibBtn);

        // Web navigation helpers
        navBackBtn.setButtonText("<");
        navBackBtn.setTooltip("Go Back");
        navBackBtn.onClick = [this] {
#if JUCE_WEB_BROWSER
            if (webView != nullptr) webView->goBack();
#endif
        };
        addAndMakeVisible(navBackBtn);

        navReloadBtn.setButtonText("R");
        navReloadBtn.setTooltip("Reload Page");
        navReloadBtn.onClick = [this] {
#if JUCE_WEB_BROWSER
            if (webView != nullptr) webView->refresh();
#endif
        };
        addAndMakeVisible(navReloadBtn);

        selectCatalogBtn.setButtonText("SELECT TONE");
        selectCatalogBtn.setTooltip("Open official TONE3000 Select Catalog inside this popup (returns picked tone for 1-click Load!)");
        selectCatalogBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0f766e));
        selectCatalogBtn.onClick = [this] { launchEmbeddedCatalogOrConnect(true); };
        addAndMakeVisible(selectCatalogBtn);

        // Destination combo
        targetBox.onChange = [this] { syncTargetFromCombo(); };
        addAndMakeVisible(targetBox);

        // Key / Account controls
        keyBtn.setButtonText("API KEY");
        keyBtn.setTooltip("Set or change your free TONE3000 Publishable Key (t3k_pub_...)");
        keyBtn.onClick = [this] {
            showKeyBar = !showKeyBar;
            resized();
            repaint();
        };
        addAndMakeVisible(keyBtn);

        authBtn.setButtonText("SIGN IN");
        authBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0284c7));
        authBtn.onClick = [this] { handleAuthBtnClick(); };
        addAndMakeVisible(authBtn);

        // Slim collapsible API key strip
        apiKeyEditor.setTextToShowWhenEmpty("Paste free TONE3000 Publishable Key (t3k_pub_...) for 1-click Auto-Load into PedalBoard", juce::Colour(0xff64748b));
        apiKeyEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff090d14));
        apiKeyEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2b374c));
        apiKeyEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff06b6d4));
        apiKeyEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe2e8f0));
        apiKeyEditor.setText(client.getPublishableKey(), false);
        apiKeyEditor.onTextChange = [this] { client.setPublishableKey(apiKeyEditor.getText()); };
        apiKeyEditor.onReturnKey = [this] { launchEmbeddedCatalogOrConnect(true); };
        addChildComponent(apiKeyEditor);

        getKeyInPopupBtn.setButtonText("GET FREE KEY");
        getKeyInPopupBtn.setTooltip("Open tone3000.com/settings directly inside this popup to create a free t3k_pub_... key");
        getKeyInPopupBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e293b));
        getKeyInPopupBtn.onClick = [this] { openUrlInEmbeddedWeb("https://www.tone3000.com/settings"); };
        addChildComponent(getKeyInPopupBtn);

        connectKeyBtn.setButtonText("CONNECT & BROWSE");
        connectKeyBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0891b2));
        connectKeyBtn.onClick = [this] { launchEmbeddedCatalogOrConnect(true); };
        addChildComponent(connectKeyBtn);

        // Right-side Picked Tone Captures Drawer (slides open when a tone is picked in WebView!)
        drawerFavBtn.setButtonText("FAV");
        drawerFavBtn.onClick = [this] { togglePickedToneFavorite(); };
        addChildComponent(drawerFavBtn);

        drawerCloseBtn.setButtonText("X");
        drawerCloseBtn.setTooltip("Hide Captures Drawer");
        drawerCloseBtn.onClick = [this] {
            showCaptureDrawer = false;
            resized();
            repaint();
        };
        addChildComponent(drawerCloseBtn);

        drawerBrowseAgainBtn.setButtonText("PICK ANOTHER TONE");
        drawerBrowseAgainBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff0f766e));
        drawerBrowseAgainBtn.onClick = [this] { launchEmbeddedCatalogOrConnect(true); };
        addChildComponent(drawerBrowseAgainBtn);

        pickedModelList.setModel(&pickedModelListModel);
        pickedModelList.setRowHeight(46);
        pickedModelList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0a0e16));
        pickedModelList.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff232d40));
        pickedModelList.setOutlineThickness(1);
        addChildComponent(pickedModelList);

        // Native Library & Cache view (Tab 2)
        nativeLibrary = std::make_unique<Tone3000BrowserOverlay>(
            processor,
            client,
            [this](const CloudLoadRequest& req) {
                if (loadFileCb) loadFileCb(req);
            },
            [] {}
        );
        nativeLibrary->setEmbeddedInPopup(true);
        addChildComponent(*nativeLibrary);

        // Create Embedded Microsoft Edge WebView2 instance (if runtime is supported)
        initEmbeddedWebView();

        rebuildTargetCombo();
        updateHeaderStyles();
    }

    ~Tone3000PopupContent() override {
        pickedModelList.setModel(nullptr);
    }

    void prepareForTarget(CloudLoadTargetKind kind, int rigOrSlotIndex) {
        activeTargetKind = kind;
        if (kind == CloudLoadTargetKind::NamPedal) {
            activeTargetSlotId = rigOrSlotIndex;
        } else if (kind == CloudLoadTargetKind::AmpRig || kind == CloudLoadTargetKind::CabRig) {
            activeTargetRigIndex = std::clamp(rigOrSlotIndex, 0, 2);
        }

        rebuildTargetCombo();
        apiKeyEditor.setText(client.getPublishableKey(), false);
        showKeyBar = !client.hasPublishableKey();
        if (nativeLibrary != nullptr) {
            nativeLibrary->openForTarget(kind, rigOrSlotIndex);
        }

        updateHeaderStyles();
        if (webViewReady) {
            setMode(Mode::WebCatalog);
            if (client.hasPublishableKey()) {
                launchEmbeddedCatalogOrConnect(true);
            } else {
                openUrlInEmbeddedWeb(publicBrowseUrlForCurrentTarget());
                setStatus("Browsing tone3000.com — click GET FREE KEY above & paste t3k_pub_... to enable 1-click Auto-Load into PedalBoard!");
            }
        } else {
            setMode(Mode::NativeLibrary);
            setStatus("Microsoft Edge WebView2 Runtime not detected — using Native Cloud Browser mode.", false);
        }

        resized();
        repaint();
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        g.fillAll(juce::Colour(0xff0e131d));

        // Top studio toolbar bar
        auto topBar = bounds.withHeight(42.0f);
        g.setColour(juce::Colour(0xff0b0f17));
        g.fillRect(topBar);
        g.setColour(juce::Colour(0xff232e42));
        g.drawHorizontalLine(static_cast<int>(topBar.getBottom()) - 1, 0.0f, bounds.getWidth());

        // Connection status indicator dot
        const bool connected = client.isConnected();
        g.setColour(connected ? juce::Colour(0xff22d3ee) : juce::Colour(0xff64748b));
        g.fillEllipse(12.0f, topBar.getCentreY() - 4.0f, 8.0f, 8.0f);

        if (!targetLabelRect.isEmpty()) {
            g.setColour(juce::Colour(0xff8491a6));
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.drawText("TARGET:", targetLabelRect, juce::Justification::centredRight);
        }

        // Collapsible API key strip
        if (showKeyBar && activeMode == Mode::WebCatalog && !keyBarRect.isEmpty()) {
            auto kr = keyBarRect.toFloat();
            g.setColour(juce::Colour(0xff0d1726));
            g.fillRect(kr);
            g.setColour(juce::Colour(0xff0891b2).withAlpha(0.55f));
            g.drawHorizontalLine(static_cast<int>(kr.getBottom()) - 1, kr.getX(), kr.getRight());
        }

        // Right-side Picked Tone Captures Drawer
        if (activeMode == Mode::WebCatalog && showCaptureDrawer && !captureDrawerRect.isEmpty()) {
            auto dr = captureDrawerRect.toFloat();
            g.setColour(juce::Colour(0xff111622));
            g.fillRect(dr);
            g.setColour(juce::Colour(0xff243044));
            g.drawVerticalLine(static_cast<int>(dr.getX()), dr.getY(), dr.getBottom());

            auto hdr = dr.reduced(10.0f, 8.0f).withHeight(64.0f);
            g.setColour(juce::Colour(0xff67e8f9));
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.drawText("PICKED TONE CAPTURES", hdr.removeFromTop(14.0f).withTrimmedRight(78.0f), juce::Justification::centredLeft);

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(12.5f, juce::Font::bold)));
            g.drawText(pickedTone.title.isNotEmpty() ? pickedTone.title : "Loading tone...",
                       hdr.removeFromTop(20.0f).withTrimmedRight(78.0f), juce::Justification::centredLeft, true);

            g.setColour(juce::Colour(0xff94a3b8));
            g.setFont(juce::Font(juce::FontOptions(9.4f, juce::Font::plain)));
            const juce::String sub = pickedTone.id > 0
                ? ("by " + pickedTone.creator.bestName() + "  |  " + pickedTone.gearDisplayLabel() + "  |  " + pickedTone.countDisplayLabel())
                : "Fetching captures from TONE3000...";
            g.drawText(sub, hdr.removeFromTop(16.0f), juce::Justification::centredLeft, true);
        }

        // Bottom status strip (in WebCatalog mode)
        if (activeMode == Mode::WebCatalog && !statusRect.isEmpty()) {
            auto sb = statusRect.toFloat();
            g.setColour(juce::Colour(0xff090d14));
            g.fillRect(sb);
            g.setColour(juce::Colour(0xff202a3c));
            g.drawHorizontalLine(static_cast<int>(sb.getY()), sb.getX(), sb.getRight());
            g.setColour(statusIsError ? juce::Colour(0xfffca5a5)
                                      : (isBusy ? juce::Colour(0xff38bdf8) : juce::Colour(0xffcbd5e1)));
            g.setFont(juce::Font(juce::FontOptions(10.0f, isBusy ? juce::Font::bold : juce::Font::plain)));
            g.drawText(statusText.isNotEmpty() ? statusText : "TONE3000 Embedded Browser ready.",
                       sb.reduced(10.0f, 0.0f), juce::Justification::centredLeft, true);
        }
    }

    void resized() override {
        auto area = getLocalBounds();
        auto topBar = area.removeFromTop(42).reduced(8, 7);
        topBar.removeFromLeft(18); // status dot space

        tabWebBtn.setBounds(topBar.removeFromLeft(108));
        topBar.removeFromLeft(4);
        tabLibBtn.setBounds(topBar.removeFromLeft(122));
        topBar.removeFromLeft(10);

        const bool isWeb = (activeMode == Mode::WebCatalog);
        navBackBtn.setVisible(isWeb && webViewReady);
        navReloadBtn.setVisible(isWeb && webViewReady);
        selectCatalogBtn.setVisible(isWeb && webViewReady);
        keyBtn.setVisible(isWeb);
        authBtn.setVisible(isWeb);
        targetBox.setVisible(isWeb);

        if (isWeb) {
            if (webViewReady) {
                navBackBtn.setBounds(topBar.removeFromLeft(26));
                topBar.removeFromLeft(3);
                navReloadBtn.setBounds(topBar.removeFromLeft(26));
                topBar.removeFromLeft(6);
                selectCatalogBtn.setBounds(topBar.removeFromLeft(96));
                topBar.removeFromLeft(8);
            }
            authBtn.setBounds(topBar.removeFromRight(88));
            topBar.removeFromRight(5);
            keyBtn.setBounds(topBar.removeFromRight(62));
            topBar.removeFromRight(8);

            const int tgtW = std::clamp(topBar.getWidth() - 56, 130, 260);
            targetBox.setBounds(topBar.removeFromRight(tgtW));
            topBar.removeFromRight(5);
            targetLabelRect = topBar.removeFromRight(48);
        } else {
            targetLabelRect = {};
        }

        // Toggle visibility between WebCatalog vs NativeLibrary
        if (nativeLibrary != nullptr) {
            nativeLibrary->setVisible(!isWeb);
            if (!isWeb) {
                nativeLibrary->setBounds(area);
            }
        }

        const bool showKey = isWeb && showKeyBar;
        apiKeyEditor.setVisible(showKey);
        getKeyInPopupBtn.setVisible(showKey);
        connectKeyBtn.setVisible(showKey);
        if (showKey) {
            keyBarRect = area.removeFromTop(36);
            auto kr = keyBarRect.reduced(10, 5);
            connectKeyBtn.setBounds(kr.removeFromRight(138));
            kr.removeFromRight(5);
            getKeyInPopupBtn.setBounds(kr.removeFromRight(100));
            kr.removeFromRight(6);
            apiKeyEditor.setBounds(kr);
        } else {
            keyBarRect = {};
        }

        if (isWeb) {
            statusRect = area.removeFromBottom(24);
        } else {
            statusRect = {};
        }

        const bool showDrawer = isWeb && showCaptureDrawer;
        drawerFavBtn.setVisible(showDrawer && pickedTone.id > 0);
        drawerCloseBtn.setVisible(showDrawer);
        drawerBrowseAgainBtn.setVisible(showDrawer);
        pickedModelList.setVisible(showDrawer);

        if (showDrawer) {
            const int drawerW = std::clamp(area.getWidth() * 31 / 100, 270, 340);
            captureDrawerRect = area.removeFromRight(drawerW);
            auto dr = captureDrawerRect.reduced(8, 8);
            auto topBtns = dr.removeFromTop(22);
            drawerCloseBtn.setBounds(topBtns.removeFromRight(24));
            topBtns.removeFromRight(4);
            drawerFavBtn.setBounds(topBtns.removeFromRight(48));
            dr.removeFromTop(42); // space for title & subtitle painted in paint()
            drawerBrowseAgainBtn.setBounds(dr.removeFromBottom(26));
            dr.removeFromBottom(6);
            pickedModelList.setBounds(dr);
        } else {
            captureDrawerRect = {};
        }

#if JUCE_WEB_BROWSER
        if (webView != nullptr) {
            webView->setVisible(isWeb && webViewReady);
            if (isWeb && webViewReady) {
                webView->setBounds(area);
            }
        }
#endif
    }

private:
    enum class Mode : int {
        WebCatalog = 0,
        NativeLibrary = 1
    };

#if JUCE_WEB_BROWSER
    class EmbeddedToneWebView : public juce::WebBrowserComponent {
    public:
        explicit EmbeddedToneWebView(const Options& opts) : juce::WebBrowserComponent(opts) {}

        std::function<bool(const juce::String&)> onBeforeNavigate;
        std::function<void(const juce::String&)> onNetworkErr;

        bool pageAboutToLoad(const juce::String& newURL) override {
            if (onBeforeNavigate) {
                return onBeforeNavigate(newURL);
            }
            return true;
        }

        void newWindowAttemptingToLoad(const juce::String& newURL) override {
            // Keep tone3000.com links inside the popup; open external links (Discord/YouTube/GitHub) in system browser
            if (newURL.containsIgnoreCase("tone3000.com")) {
                goToURL(newURL);
            } else {
                juce::URL(newURL).launchInDefaultBrowser();
            }
        }

        bool pageLoadHadNetworkError(const juce::String& errorInfo) override {
            if (onNetworkErr) onNetworkErr(errorInfo);
            return false;
        }
    };
#endif

    class PickedModelListModel : public juce::ListBoxModel {
    public:
        explicit PickedModelListModel(Tone3000PopupContent& o) : owner(o) {}

        int getNumRows() override {
            return static_cast<int>(owner.pickedModels.size());
        }

        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override {
            if (row < 0 || row >= static_cast<int>(owner.pickedModels.size())) return;
            const auto& m = owner.pickedModels[static_cast<size_t>(row)];
            const bool cached = t3k::Tone3000Client::isModelCached(m);
            const bool busy   = (owner.downloadingModelId == m.id);
            const bool loaded = (owner.lastLoadedModelId == m.id);

            auto r = juce::Rectangle<float>(3.0f, 2.0f, static_cast<float>(w - 6), static_cast<float>(h - 4));
            g.setColour(loaded ? juce::Colour(0xff132838)
                               : (selected ? juce::Colour(0xff182234) : juce::Colour(0xff0f1420)));
            g.fillRoundedRectangle(r, 5.0f);
            g.setColour(busy ? juce::Colour(0xfff59e0b)
                             : (loaded ? juce::Colour(0xff22d3ee) : juce::Colour(0xff232e42)));
            g.drawRoundedRectangle(r, 5.0f, (busy || loaded) ? 1.3f : 0.8f);

            auto inner = r.reduced(7.0f, 4.0f);
            auto pill = inner.removeFromRight(68.0f).reduced(1.0f, 4.0f);
            g.setColour(busy ? juce::Colour(0xffb45309)
                             : (loaded ? juce::Colour(0xff15803d)
                                       : (cached ? juce::Colour(0xff0f766e) : juce::Colour(0xff0284c7))));
            g.fillRoundedRectangle(pill, 4.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(8.8f, juce::Font::bold)));
            g.drawText(busy ? "WAIT..." : (loaded ? "ACTIVE" : (cached ? "LOAD" : "GET & LOAD")),
                       pill, juce::Justification::centred);

            inner.removeFromRight(5.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(10.8f, juce::Font::bold)));
            g.drawText(m.name, inner.removeFromTop(18.0f), juce::Justification::centredLeft, true);

            juce::String meta = m.fileExtension().toUpperCase();
            if (m.architectureVersion.isNotEmpty()) meta += " | A" + m.architectureVersion;
            if (m.size.isNotEmpty())                meta += " | " + m.size;
            if (cached)                             meta += " | CACHED";
            g.setColour(cached ? juce::Colour(0xff34d399) : juce::Colour(0xff8491a6));
            g.setFont(juce::Font(juce::FontOptions(8.6f, juce::Font::plain)));
            g.drawText(meta, inner, juce::Justification::centredLeft, true);
        }

        void listBoxItemClicked(int row, const juce::MouseEvent&) override {
            owner.downloadAndLoadPickedModel(row);
        }

        void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override {
            owner.downloadAndLoadPickedModel(row);
        }

    private:
        Tone3000PopupContent& owner;
    };

    void initEmbeddedWebView() {
#if JUCE_WEB_BROWSER
        auto dataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                           .getChildFile("PedalBoard")
                           .getChildFile("WebView2");
        dataDir.createDirectory();

        auto opts = juce::WebBrowserComponent::Options {}
                        .withKeepPageLoadedWhenBrowserIsHidden();
#if JUCE_WINDOWS
        opts = opts.withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
                   .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2 {}
                                               .withUserDataFolder(dataDir)
                                               .withStatusBarDisabled()
                                               .withBuiltInErrorPageDisabled()
                                               .withBackgroundColour(juce::Colour(0xff0d1118)));
#endif
        if (juce::WebBrowserComponent::areOptionsSupported(opts)) {
            webView = std::make_unique<EmbeddedToneWebView>(opts);
            webView->onBeforeNavigate = [this](const juce::String& url) {
                return handleWebNavigation(url);
            };
            webView->onNetworkErr = [this](const juce::String& info) {
                setStatus("Network error loading TONE3000: " + info, true);
            };
            addAndMakeVisible(*webView);
            webViewReady = true;
        }
#endif
    }

    bool handleWebNavigation(const juce::String& url) {
        if (url.startsWithIgnoreCase(kEmbeddedRedirectUri)) {
            const auto query = url.contains("?") ? url.fromFirstOccurrenceOf("?", false, false) : juce::String();
            setStatus("Completing TONE3000 sign-in...", false, true);

            juce::Component::SafePointer<Tone3000PopupContent> safeThis(this);
            client.completeEmbeddedOAuthCallback(query, kEmbeddedRedirectUri, [safeThis](t3k::OAuthFlowResult res) {
                if (safeThis == nullptr) return;
                safeThis->updateHeaderStyles();
                if (res.connected) {
                    safeThis->showKeyBar = false;
                    safeThis->resized();
                    safeThis->client.fetchCurrentUser([safeThis](t3k::ApiResult<t3k::UserInfo>) {
                        if (safeThis != nullptr) safeThis->updateHeaderStyles();
                    });
                    if (res.selectedToneId > 0) {
                        safeThis->fetchAndDisplayPickedTone(res.selectedToneId);
                        // Keep the catalog ready for picking another tone right after!
                        safeThis->launchEmbeddedCatalogOrConnect(true, false);
                    } else if (res.canceled) {
                        safeThis->setStatus("Connected to TONE3000! Click SELECT TONE or switch to LIBRARY & CACHE.");
                        safeThis->openUrlInEmbeddedWeb(safeThis->publicBrowseUrlForCurrentTarget());
                    } else {
                        safeThis->setStatus("Signed in to TONE3000! Opening Select Catalog...");
                        safeThis->launchEmbeddedCatalogOrConnect(true);
                    }
                } else if (res.canceled) {
                    safeThis->setStatus("TONE3000 selection canceled.");
                    safeThis->openUrlInEmbeddedWeb(safeThis->publicBrowseUrlForCurrentTarget());
                } else {
                    safeThis->setStatus(res.errorMessage.isNotEmpty() ? res.errorMessage : "TONE3000 sign-in failed.", true);
                }
            });
            return false; // Intercepted! Do not navigate WebView2 to 127.0.0.1
        }
        return true;
    }

    void openUrlInEmbeddedWeb(const juce::String& urlStr) {
#if JUCE_WEB_BROWSER
        if (webView != nullptr && webViewReady) {
            setMode(Mode::WebCatalog);
            webView->goToURL(urlStr);
            return;
        }
#endif
        juce::URL(urlStr).launchInDefaultBrowser();
    }

    juce::String publicBrowseUrlForCurrentTarget() const {
        if (activeTargetKind == CloudLoadTargetKind::NamPedal) return "https://www.tone3000.com/search?gears=pedal&format=nam";
        if (activeTargetKind == CloudLoadTargetKind::AmpRig)   return "https://www.tone3000.com/search?gears=amp_amp-cab&format=nam";
        if (activeTargetKind == CloudLoadTargetKind::CabRig)   return "https://www.tone3000.com/search?gears=cab&format=ir";
        return "https://www.tone3000.com/tones";
    }

    void launchEmbeddedCatalogOrConnect(bool selectTonePrompt, bool updateStatusMsg = true) {
        const auto key = apiKeyEditor.getText().trim();
        if (key.isNotEmpty()) {
            client.setPublishableKey(key);
        }
        if (!client.hasPublishableKey()) {
            showKeyBar = true;
            resized();
            if (updateStatusMsg) {
                setStatus("Click GET FREE KEY to copy your free t3k_pub_... key from tone3000.com/settings, then paste it above.", true);
            }
            return;
        }

        juce::String gears;
        juce::String fmt;
        if (activeTargetKind == CloudLoadTargetKind::NamPedal) { gears = "pedal"; fmt = "nam"; }
        else if (activeTargetKind == CloudLoadTargetKind::AmpRig) { gears = "amp_amp-cab"; fmt = "nam"; }
        else if (activeTargetKind == CloudLoadTargetKind::CabRig) { gears = "cab"; fmt = "ir"; }

#if JUCE_WEB_BROWSER
        if (webView != nullptr && webViewReady) {
            const auto url = client.prepareEmbeddedOAuthUrl(
                kEmbeddedRedirectUri,
                selectTonePrompt ? "select_tone" : "",
                gears,
                fmt,
                0,     // A1 + Standard (widest catalog); our API fetch also supports A2!
                false  // cleaner embedded chrome
            );
            if (!url.isEmpty()) {
                setMode(Mode::WebCatalog);
                webView->goToURL(url.toString(true));
                if (updateStatusMsg) {
                    setStatus(selectTonePrompt
                        ? "Browse TONE3000 below and click Select Tone to load directly into PedalBoard!"
                        : "Sign in to your TONE3000 account below...", false, false);
                }
                return;
            }
        }
#endif
        // Fallback if WebView2 runtime isn't installed
        setMode(Mode::NativeLibrary);
        setStatus("WebView2 unavailable — use CONNECT / BROWSE WEB in Native Library mode.");
    }

    void handleAuthBtnClick() {
        if (client.isConnected()) {
            client.disconnect();
            showKeyBar = true;
            updateHeaderStyles();
            resized();
            setStatus("Disconnected from TONE3000.");
            return;
        }
        launchEmbeddedCatalogOrConnect(false);
    }

    void fetchAndDisplayPickedTone(int toneId) {
        showCaptureDrawer = true;
        pickedTone = {};
        pickedTone.id = toneId;
        pickedModels.clear();
        lastLoadedModelId = 0;
        pickedModelList.updateContent();
        resized();
        repaint();
        setStatus("Fetching picked TONE3000 tone #" + juce::String(toneId) + "...", false, true);

        juce::Component::SafePointer<Tone3000PopupContent> safeThis(this);
        client.fetchToneWithModels(toneId, 0,
            [safeThis, toneId](t3k::ApiResult<std::pair<t3k::ToneItem, std::vector<t3k::ModelItem>>> r) {
                if (safeThis == nullptr || safeThis->pickedTone.id != toneId) return;
                if (!r.ok()) {
                    safeThis->setStatus(r.error.friendlyMessage("Load tone"), true);
                    return;
                }
                safeThis->pickedTone   = r.value->first;
                safeThis->pickedModels = r.value->second;
                safeThis->pickedModelList.updateContent();
                safeThis->updateHeaderStyles();
                safeThis->resized();
                safeThis->repaint();

                if (!safeThis->pickedTone.isSupportedByPedalBoard()) {
                    safeThis->setStatus("\"" + safeThis->pickedTone.title + "\" uses format " +
                                        safeThis->pickedTone.format.toUpperCase() +
                                        " — PedalBoard supports .nam (NAM) and .wav (IR) captures.", true);
                } else if (safeThis->pickedModels.size() == 1) {
                    safeThis->setStatus("Auto-loading \"" + safeThis->pickedModels[0].name + "\"...", false, true);
                    safeThis->downloadAndLoadPickedModel(0);
                } else if (!safeThis->pickedModels.empty()) {
                    safeThis->setStatus("Picked \"" + safeThis->pickedTone.title + "\" (" +
                                        juce::String(safeThis->pickedModels.size()) +
                                        " captures) — click any capture on the right to load immediately!");
                } else {
                    safeThis->setStatus("No downloadable .nam/.wav files returned for \"" + safeThis->pickedTone.title +
                                        "\" (format: " + safeThis->pickedTone.format.toUpperCase() +
                                        ", A1:" + juce::String(safeThis->pickedTone.a1ModelsCount) +
                                        " A2:" + juce::String(safeThis->pickedTone.a2ModelsCount) +
                                        " IR:" + juce::String(safeThis->pickedTone.irsCount) + ").", true);
                }
            });
    }

    void downloadAndLoadPickedModel(int row) {
        if (row < 0 || row >= static_cast<int>(pickedModels.size())) return;
        const auto model = pickedModels[static_cast<size_t>(row)];
        const auto toneSnap = pickedTone;

        downloadingModelId = model.id;
        pickedModelList.repaint();
        setStatus("Downloading \"" + model.name + "\" from TONE3000...", false, true);

        juce::Component::SafePointer<Tone3000PopupContent> safeThis(this);
        client.downloadModelFile(model, [safeThis, model, toneSnap](t3k::ApiResult<juce::File> r) {
            if (safeThis == nullptr) return;
            safeThis->downloadingModelId = 0;
            if (!r.ok()) {
                safeThis->pickedModelList.repaint();
                safeThis->setStatus(r.error.friendlyMessage("Download"), true);
                return;
            }

            safeThis->lastLoadedModelId = model.id;
            safeThis->pickedModelList.repaint();

            CloudLoadRequest req;
            req.file           = *r.value;
            req.isIr           = model.isIrFile() || toneSnap.isIrTone();
            req.isPedal        = toneSnap.isPedalTone();
            req.isFullRig      = (toneSnap.gear == "amp-cab" || toneSnap.gear == "full-rig");
            req.targetKind     = safeThis->activeTargetKind;
            req.targetRigIndex = safeThis->activeTargetRigIndex;
            req.targetSlotId   = safeThis->activeTargetSlotId;
            req.displayTitle   = model.name;

            if (safeThis->loadFileCb) {
                safeThis->loadFileCb(req);
            }

            const juce::String tip = req.isFullRig ? "  (Tip: Bypass CAB for Amp+Cab full-rig)" : "";
            safeThis->setStatus("Loaded \"" + model.name + "\" into PedalBoard!" + tip);
        });
    }

    void togglePickedToneFavorite() {
        if (pickedTone.id <= 0 || !client.isConnected()) return;
        const int id = pickedTone.id;
        const bool nextFav = !pickedTone.isFavorite;
        juce::Component::SafePointer<Tone3000PopupContent> safeThis(this);
        client.setToneFavorite(id, nextFav, [safeThis, id](t3k::ApiResult<bool> r) {
            if (safeThis == nullptr || safeThis->pickedTone.id != id) return;
            if (r.ok()) {
                safeThis->pickedTone.isFavorite = *r.value;
                safeThis->updateHeaderStyles();
                safeThis->setStatus(*r.value ? "Saved to your TONE3000 Favorites." : "Removed from Favorites.");
            }
        });
    }

    void setMode(Mode m) {
        if (m == Mode::WebCatalog && !webViewReady) {
            m = Mode::NativeLibrary;
        }
        activeMode = m;
        if (activeMode == Mode::NativeLibrary && nativeLibrary != nullptr) {
            nativeLibrary->openForTarget(activeTargetKind,
                                         activeTargetKind == CloudLoadTargetKind::NamPedal ? activeTargetSlotId : activeTargetRigIndex);
        }
        updateHeaderStyles();
        resized();
        repaint();
    }

    void rebuildTargetCombo() {
        const auto mode = processor.getRoutingMode();
        const int rigCount = (mode == RoutingMode::TripleParallel) ? 3 : (isParallelRouting(mode) ? 2 : 1);

        targetBox.clear(juce::dontSendNotification);
        targetBox.addItem("Auto (Pedal / Amp A / Cab A)", 1);
        const juce::String pedalTxt = (activeTargetSlotId >= 0)
            ? ("NAM Pedal (Slot #" + juce::String(activeTargetSlotId + 1) + ")")
            : "NAM Pedal (Add / Active)";
        targetBox.addItem(pedalTxt, 2);
        targetBox.addItem("Amp A (NAM Capture)", 10);
        if (rigCount >= 2) targetBox.addItem("Amp B (NAM Capture)", 11);
        if (rigCount >= 3) targetBox.addItem("Amp C (NAM Capture)", 12);
        targetBox.addItem("Cab A (User IR)", 20);
        if (rigCount >= 2) targetBox.addItem("Cab B (User IR)", 21);
        if (rigCount >= 3) targetBox.addItem("Cab C (User IR)", 22);

        int selId = 1;
        if (activeTargetKind == CloudLoadTargetKind::NamPedal) selId = 2;
        else if (activeTargetKind == CloudLoadTargetKind::AmpRig) selId = 10 + std::clamp(activeTargetRigIndex, 0, rigCount - 1);
        else if (activeTargetKind == CloudLoadTargetKind::CabRig) selId = 20 + std::clamp(activeTargetRigIndex, 0, rigCount - 1);
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

    void updateHeaderStyles() {
        const bool isWeb = (activeMode == Mode::WebCatalog);
        tabWebBtn.setColour(juce::TextButton::buttonColourId, isWeb ? juce::Colour(0xff0891b2) : juce::Colour(0xff19202e));
        tabWebBtn.setColour(juce::TextButton::textColourOffId, isWeb ? juce::Colours::white : juce::Colour(0xff94a3b8));
        tabLibBtn.setColour(juce::TextButton::buttonColourId, !isWeb ? juce::Colour(0xff0891b2) : juce::Colour(0xff19202e));
        tabLibBtn.setColour(juce::TextButton::textColourOffId, !isWeb ? juce::Colours::white : juce::Colour(0xff94a3b8));

        const bool connected = client.isConnected();
        authBtn.setButtonText(connected ? "DISCONNECT" : "SIGN IN");
        authBtn.setColour(juce::TextButton::buttonColourId,
                          connected ? juce::Colour(0xff2c1a1f) : juce::Colour(0xff0284c7));

        drawerFavBtn.setButtonText(pickedTone.isFavorite ? "FAVED" : "FAV");
        drawerFavBtn.setColour(juce::TextButton::buttonColourId,
                               pickedTone.isFavorite ? juce::Colour(0xffbe123c) : juce::Colour(0xff1c2333));
        repaint();
    }

    void setStatus(const juce::String& msg, bool err = false, bool busy = false) {
        statusText = msg;
        statusIsError = err;
        isBusy = busy;
        repaint();
    }

    PedalBoardAudioProcessor& processor;
    t3k::Tone3000Client& client;
    Tone3000BrowserOverlay::LoadFileCallback loadFileCb;

    Mode activeMode = Mode::WebCatalog;
    bool webViewReady = false;
    bool showKeyBar = false;
    bool showCaptureDrawer = false;

    CloudLoadTargetKind activeTargetKind = CloudLoadTargetKind::Auto;
    int activeTargetRigIndex = 0;
    int activeTargetSlotId = -1;

    t3k::ToneItem pickedTone;
    std::vector<t3k::ModelItem> pickedModels;
    PickedModelListModel pickedModelListModel;
    int downloadingModelId = 0;
    int lastLoadedModelId = 0;

    juce::String statusText;
    bool statusIsError = false;
    bool isBusy = false;

    juce::Rectangle<int> targetLabelRect;
    juce::Rectangle<int> keyBarRect;
    juce::Rectangle<int> captureDrawerRect;
    juce::Rectangle<int> statusRect;

    juce::TextButton tabWebBtn, tabLibBtn;
    juce::TextButton navBackBtn, navReloadBtn, selectCatalogBtn;
    juce::ComboBox targetBox;
    juce::TextButton keyBtn, authBtn;

    juce::TextEditor apiKeyEditor;
    juce::TextButton getKeyInPopupBtn, connectKeyBtn;

    juce::TextButton drawerFavBtn, drawerCloseBtn, drawerBrowseAgainBtn;
    juce::ListBox pickedModelList;

#if JUCE_WEB_BROWSER
    std::unique_ptr<EmbeddedToneWebView> webView;
#endif
    std::unique_ptr<Tone3000BrowserOverlay> nativeLibrary;
};

class Tone3000PopupWindow : public juce::DocumentWindow {
public:
    Tone3000PopupWindow(PedalBoardAudioProcessor& proc,
                        t3k::Tone3000Client& cloudClient,
                        juce::LookAndFeel* lnf,
                        Tone3000BrowserOverlay::LoadFileCallback onLoadFile,
                        std::function<void(juce::DocumentWindow&)> onApplyDarkFrame = {})
        : juce::DocumentWindow("TONE3000 Cloud Browser",
                               juce::Colour(0xff0e131d),
                               juce::DocumentWindow::minimiseButton
                                   | juce::DocumentWindow::maximiseButton
                                   | juce::DocumentWindow::closeButton,
                               false),
          applyDarkFrameCb(std::move(onApplyDarkFrame)) {
        if (lnf != nullptr) {
            setLookAndFeel(lnf);
        }
        content = new Tone3000PopupContent(proc, cloudClient, std::move(onLoadFile));
        setContentOwned(content, true);
        setUsingNativeTitleBar(true);
        setResizable(true, false);
        setResizeLimits(760, 520, 3840, 2160);
        addToDesktop();
        if (applyDarkFrameCb) {
            applyDarkFrameCb(*this);
        }
    }

    ~Tone3000PopupWindow() override {
        setLookAndFeel(nullptr);
    }

    void showForTarget(juce::Component* anchor, CloudLoadTargetKind kind, int rigOrSlotIndex) {
        if (!positionedOnce) {
            positionedOnce = true;
            int w = 1080;
            int h = 700;
            if (const auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
                w = std::clamp(disp->userArea.getWidth() - 160, 820, 1140);
                h = std::clamp(disp->userArea.getHeight() - 120, 580, 740);
            }
            if (anchor != nullptr) {
                centreAroundComponent(anchor, w, h);
            } else {
                centreWithSize(w, h);
            }
            if (applyDarkFrameCb) {
                applyDarkFrameCb(*this);
            }
        }
        if (content != nullptr) {
            content->prepareForTarget(kind, rigOrSlotIndex);
        }
        setVisible(true);
        toFront(true);
    }

    void closeButtonPressed() override {
        setVisible(false);
    }

private:
    Tone3000PopupContent* content = nullptr;
    std::function<void(juce::DocumentWindow&)> applyDarkFrameCb;
    bool positionedOnce = false;
};

} // namespace pedalboard
