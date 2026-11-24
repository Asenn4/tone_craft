#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace pedalboard {

// Official TONE3000 REST API v1 + OAuth 2.0 PKCE (S256) native C++20/JUCE client.
// Uses system browser + RFC 8252 ephemeral 127.0.0.1 loopback redirect (zero WebView!)
// and Windows DPAPI encrypted local token storage.
namespace t3k {

struct UserInfo {
    int id = 0;
    juce::String username;
    juce::String displayName;
    bool isVerified = false;

    juce::String bestName() const {
        return displayName.isNotEmpty() ? displayName : username;
    }
};

struct ToneItem {
    int id = 0;
    juce::String title;
    juce::String description;
    juce::String gear;      // "amp", "amp-cab", "pedal", "cab", "outboard", "space", "experimental"
    juce::String format;    // "nam", "ir", etc.
    juce::String license;
    juce::String url;
    juce::String publishedAt;
    UserInfo creator;
    juce::StringArray makes;
    juce::StringArray tags;
    int modelsCount = 0;
    int a1ModelsCount = 0;
    int a2ModelsCount = 0;
    int customModelsCount = 0;
    int irsCount = 0;
    int downloadsCount = 0;
    int favoritesCount = 0;
    bool isFavorite = false;

    bool isIrTone() const noexcept {
        return format.equalsIgnoreCase("ir") || gear.equalsIgnoreCase("cab") || gear.equalsIgnoreCase("ir");
    }

    bool isSupportedByPedalBoard() const noexcept {
        return format.isEmpty() || format.equalsIgnoreCase("nam") || format.equalsIgnoreCase("ir");
    }

    bool isPedalTone() const noexcept {
        return gear.equalsIgnoreCase("pedal");
    }

    juce::String gearDisplayLabel() const {
        if (gear == "amp")          return "AMP HEAD";
        if (gear == "amp-cab" || gear == "full-rig") return "AMP + CAB";
        if (gear == "pedal")        return "PEDAL";
        if (gear == "cab" || gear == "ir") return "CAB IR";
        if (gear == "outboard")     return "OUTBOARD";
        if (gear == "space")        return "SPACE";
        if (gear == "experimental") return "FX";
        return gear.toUpperCase();
    }

    juce::String countDisplayLabel() const {
        if (format.equalsIgnoreCase("ir")) {
            return juce::String(irsCount) + (irsCount == 1 ? " IR" : " IRs");
        }
        const int totalNam = std::max(modelsCount, a1ModelsCount + a2ModelsCount + customModelsCount);
        return juce::String(totalNam) + (totalNam == 1 ? " capture" : " captures");
    }
};

struct ModelItem {
    int id = 0;
    int toneId = 0;
    juce::String name;
    juce::String modelUrl;
    juce::String size;
    juce::String architectureVersion; // "1", "2", "custom", or empty

    juce::String fileExtension() const {
        const auto sub = juce::URL(modelUrl).getSubPath();
        const auto leaf = sub.fromLastOccurrenceOf("/", false, false);
        if (leaf.contains(".")) {
            return "." + leaf.fromLastOccurrenceOf(".", false, false).toLowerCase();
        }
        return ".nam";
    }

    bool isIrFile() const {
        const auto ext = fileExtension();
        return ext == ".wav" || ext == ".aif" || ext == ".aiff";
    }
};

struct TonePage {
    std::vector<ToneItem> items;
    int page = 1;
    int pageSize = 20;
    int total = 0;
    int totalPages = 1;
};

struct CachedLocalFile {
    juce::File file;
    juce::String displayName;
    bool isIr = false;
    juce::int64 sizeBytes = 0;
    juce::Time modifiedTime;
};

struct ApiError {
    int status = 0;
    juce::String message;

    juce::String friendlyMessage(const juce::String& context = {}) const {
        if (status == 429) return "Rate limit reached (TONE3000). Wait a moment and retry.";
        if (status == 401) return "TONE3000 session expired. Click CONNECT to sign in again.";
        if (status == 403) return "Access denied (HTTP 403). Use 'Browse on TONE3000' or verify your API key tier.";
        if (status == 0)   return message.isNotEmpty() ? message : "Could not reach tone3000.com. Check internet connection.";
        const auto prefix = context.isNotEmpty() ? context : juce::String("TONE3000 request");
        return prefix + " failed (HTTP " + juce::String(status) + ")" + (message.isNotEmpty() ? ": " + message : "");
    }
};

template <typename T>
struct ApiResult {
    std::optional<T> value;
    ApiError error;

    bool ok() const noexcept { return value.has_value(); }

    static ApiResult success(T v) {
        ApiResult r;
        r.value = std::move(v);
        return r;
    }

    static ApiResult failure(ApiError e) {
        ApiResult r;
        r.error = std::move(e);
        return r;
    }
};

struct SearchQuery {
    juce::String query;
    juce::String sort { "trending" }; // "best-match", "trending", "newest", "downloads-all-time"
    juce::String gear;                // empty = all, or "amp", "amp-cab", "pedal", "cab", "outboard"
    juce::String format;              // empty = nam+ir, or "nam", "ir"
    int architecture = 0;             // 0 = A1/Standard (omitted), 2 = A2 Fast
    int page = 1;
    int pageSize = 24;
};

struct OAuthFlowResult {
    bool connected = false;
    bool canceled = false;
    int selectedToneId = 0;
    juce::String errorMessage;
};

class Tone3000Client {
public:
    Tone3000Client();
    ~Tone3000Client();

    // Persistent API key (TONE3000 Publishable Key: t3k_pub_...)
    juce::String getPublishableKey() const;
    void setPublishableKey(const juce::String& newKey);
    bool hasPublishableKey() const;

    bool isConnected() const;
    UserInfo getCachedUser() const;
    void disconnect();

    // Persistent cache directory for downloaded .nam and .wav captures
    static juce::File getCacheDirectory();
    static juce::File getCachedFilePathForModel(const ModelItem& model);
    static bool isModelCached(const ModelItem& model);
    static std::vector<CachedLocalFile> scanLocalCache();

    // OAuth 2.0 PKCE + Loopback Flows (System browser fallback, non-blocking, callback on Message Thread)
    void startConnectFlow(std::function<void(OAuthFlowResult)> onComplete);
    void startSelectToneFlow(const juce::String& gears,
                             const juce::String& format,
                             int architecture,
                             std::function<void(OAuthFlowResult)> onComplete);
    void cancelActiveAuthFlow();
    bool isAuthFlowActive() const noexcept { return authFlowActive.load(std::memory_order_acquire); }

    // Embedded WebView2 OAuth 2.0 PKCE helpers (intercepted inside popup WebView via pageAboutToLoad)
    juce::URL prepareEmbeddedOAuthUrl(const juce::String& redirectUri,
                                      const juce::String& prompt,
                                      const juce::String& gears,
                                      const juce::String& format,
                                      int architecture,
                                      bool showMenubar = false);
    void completeEmbeddedOAuthCallback(const juce::String& query,
                                       const juce::String& redirectUri,
                                       std::function<void(OAuthFlowResult)> onComplete);

    // REST API v1 Calls (all HTTP off-thread, callbacks on Message Thread)
    void fetchCurrentUser(std::function<void(ApiResult<UserInfo>)> done);
    void fetchTrending(const juce::String& gear, std::function<void(ApiResult<TonePage>)> done);
    void fetchLatest(std::function<void(ApiResult<TonePage>)> done);
    void searchTones(const SearchQuery& q, std::function<void(ApiResult<TonePage>)> done);
    void fetchLibrary(const juce::String& endpoint, // "favorited", "downloaded", "created"
                      const juce::String& gear,
                      const juce::String& query,
                      int page,
                      std::function<void(ApiResult<TonePage>)> done);
    void fetchToneWithModels(int toneId,
                             int architecture,
                             std::function<void(ApiResult<std::pair<ToneItem, std::vector<ModelItem>>>)> done);
    void fetchModelsForTone(int toneId,
                            int architecture,
                            std::function<void(ApiResult<std::vector<ModelItem>>)> done);
    void setToneFavorite(int toneId, bool favorite, std::function<void(ApiResult<bool>)> done);
    void downloadModelFile(const ModelItem& model, std::function<void(ApiResult<juce::File>)> done);

    // Cancel pending callbacks cleanly on destruction
    std::shared_ptr<std::atomic<bool>> getAliveToken() const noexcept { return aliveFlag; }

private:
    struct Tokens {
        juce::String accessToken;
        juce::String refreshToken;
        juce::int64 expiresAtMs = 0;
        bool isValid() const { return accessToken.isNotEmpty() && refreshToken.isNotEmpty(); }
    };

    struct HttpRes {
        int status = 0;
        juce::MemoryBlock body;
        juce::String error;
        bool ok() const noexcept { return status >= 200 && status < 300; }
        juce::String text() const { return body.toString(); }
        juce::var json() const { return juce::JSON::parse(text()); }
    };

    class LoopbackReceiver;

    static HttpRes sendHttpSync(const juce::URL& url,
                                const juce::String& method,
                                const juce::StringPairArray& headers,
                                const juce::String& body = {},
                                int timeoutMs = 25000);

    static juce::String sha256Base64Url(const juce::String& input);
    static juce::String generateRandomBase64Url(int numBytes);
    static juce::String formEncode(const std::vector<std::pair<juce::String, juce::String>>& fields);
    static juce::String sanitizeFilename(const juce::String& raw);

    static UserInfo parseUser(const juce::var& v);
    static ToneItem parseTone(const juce::var& v);
    static ModelItem parseModel(const juce::var& v);
    static TonePage parseTonePage(const juce::var& v);
    static juce::String parseErrorMessage(const HttpRes& res);

    // Secure persistent storage (Windows DPAPI + JUCE PropertiesFile)
    void loadStoredState();
    void saveStoredKey(const juce::String& k);
    void saveStoredTokens(const Tokens& t);
    void clearStoredTokens();

    Tokens getTokensCopy() const;
    juce::String getValidAccessToken(bool forceRefresh);
    bool refreshTokensSync();
    HttpRes authenticatedRequestSync(const juce::String& method,
                                     const juce::String& pathAndQuery,
                                     const juce::String& body,
                                     ApiError& outErr);

    void beginOAuthFlow(const juce::StringPairArray& extraParams,
                        std::function<void(OAuthFlowResult)> onComplete);
    void handleLoopbackQuery(const juce::String& query, const juce::String& redirectUri);
    void finishAuthFlow(OAuthFlowResult result);

    juce::String apiOrigin { "https://www.tone3000.com" };

    mutable std::mutex stateMutex;
    juce::String publishableKey;
    Tokens tokens;
    UserInfo cachedUser;
    std::mutex refreshMutex;

    std::unique_ptr<LoopbackReceiver> loopback;
    std::atomic<bool> authFlowActive { false };
    juce::String pendingCodeVerifier;
    juce::String pendingState;
    std::function<void(OAuthFlowResult)> pendingAuthCallback;

    std::shared_ptr<std::atomic<bool>> aliveFlag { std::make_shared<std::atomic<bool>>(true) };
    juce::ThreadPool workerPool { 4 };
};

} // namespace t3k
} // namespace pedalboard
