#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <wincrypt.h>
#endif

#include "Tone3000Client.h"

#include <array>
#include <cstdint>
#include <cstring>

namespace pedalboard {
namespace t3k {

namespace {

constexpr juce::int64 kRefreshLeadMs = 60 * 1000;
constexpr const char* kTokenEndpoint = "/api/v1/oauth/token";

// Pure C++20 RFC 6234 SHA-256 (zero external cryptography dependency required)
std::array<uint8_t, 32> computeSha256(const uint8_t* data, size_t len) noexcept {
    static constexpr uint32_t k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };

    auto rotr = [](uint32_t x, uint32_t n) noexcept { return (x >> n) | (x << (32u - n)); };

    uint32_t h[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };

    const uint64_t bitLen = static_cast<uint64_t>(len) * 8ull;
    const size_t paddedLen = ((len + 9 + 63) / 64) * 64;
    std::vector<uint8_t> msg(paddedLen, 0u);
    if (len > 0 && data != nullptr) {
        std::memcpy(msg.data(), data, len);
    }
    msg[len] = 0x80u;
    for (int i = 0; i < 8; ++i) {
        msg[paddedLen - 1 - static_cast<size_t>(i)] = static_cast<uint8_t>((bitLen >> (i * 8)) & 0xffu);
    }

    for (size_t offset = 0; offset < paddedLen; offset += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            const size_t idx = offset + static_cast<size_t>(i) * 4;
            w[i] = (static_cast<uint32_t>(msg[idx]) << 24)
                 | (static_cast<uint32_t>(msg[idx + 1]) << 16)
                 | (static_cast<uint32_t>(msg[idx + 2]) << 8)
                 | static_cast<uint32_t>(msg[idx + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

        for (int i = 0; i < 64; ++i) {
            const uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const uint32_t ch = (e & f) ^ ((~e) & g);
            const uint32_t temp1 = hh + S1 + ch + k[i] + w[i];
            const uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t temp2 = S0 + maj;

            hh = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    std::array<uint8_t, 32> digest {};
    for (int i = 0; i < 8; ++i) {
        digest[static_cast<size_t>(i * 4)]     = static_cast<uint8_t>((h[i] >> 24) & 0xffu);
        digest[static_cast<size_t>(i * 4 + 1)] = static_cast<uint8_t>((h[i] >> 16) & 0xffu);
        digest[static_cast<size_t>(i * 4 + 2)] = static_cast<uint8_t>((h[i] >> 8) & 0xffu);
        digest[static_cast<size_t>(i * 4 + 3)] = static_cast<uint8_t>(h[i] & 0xffu);
    }
    return digest;
}

juce::String toBase64Url(const void* data, size_t size) {
    return juce::Base64::toBase64(data, size)
        .replaceCharacter('+', '-')
        .replaceCharacter('/', '_')
        .removeCharacters("=");
}

juce::String pathOfUrl(const juce::URL& url) {
    const auto full = url.toString(true);
    const auto afterScheme = full.fromFirstOccurrenceOf("://", false, false);
    return afterScheme.contains("/") ? afterScheme.fromFirstOccurrenceOf("/", true, false) : "/";
}

std::unique_ptr<juce::PropertiesFile> openTone3000Settings() {
    juce::PropertiesFile::Options opts;
    opts.applicationName = "tone3000";
    opts.filenameSuffix = ".settings";
    opts.folderName = "PedalBoard";
    opts.osxLibrarySubFolder = "Application Support";
    return std::make_unique<juce::PropertiesFile>(opts);
}

juce::String protectSecret(const juce::String& plain) {
    if (plain.isEmpty()) return {};
#if defined(_WIN32)
    DATA_BLOB in { static_cast<DWORD>(plain.getNumBytesAsUTF8()),
                   reinterpret_cast<BYTE*>(const_cast<char*>(plain.toRawUTF8())) };
    DATA_BLOB out {};
    if (CryptProtectData(&in, L"PedalBoard TONE3000", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        auto encoded = juce::Base64::toBase64(out.pbData, out.cbData);
        LocalFree(out.pbData);
        return "dpapi:" + encoded;
    }
#endif
    return juce::Base64::toBase64(plain.toRawUTF8(), plain.getNumBytesAsUTF8());
}

juce::String unprotectSecret(const juce::String& stored) {
    if (stored.isEmpty()) return {};
#if defined(_WIN32)
    if (stored.startsWith("dpapi:")) {
        juce::MemoryOutputStream raw;
        if (!juce::Base64::convertFromBase64(raw, stored.fromFirstOccurrenceOf("dpapi:", false, false))) {
            return {};
        }
        DATA_BLOB in { static_cast<DWORD>(raw.getDataSize()),
                       reinterpret_cast<BYTE*>(const_cast<void*>(raw.getData())) };
        DATA_BLOB out {};
        if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
            return {};
        }
        auto plain = juce::String::fromUTF8(reinterpret_cast<const char*>(out.pbData), static_cast<int>(out.cbData));
        LocalFree(out.pbData);
        return plain;
    }
#endif
    juce::MemoryOutputStream raw;
    if (juce::Base64::convertFromBase64(raw, stored)) {
        return raw.toString();
    }
    return {};
}

juce::String jsonStr(const juce::var& v, const char* key) {
    const auto p = v.getProperty(key, juce::var());
    return p.isVoid() ? juce::String() : p.toString();
}

int jsonInt(const juce::var& v, const char* key, int def = 0) {
    const auto p = v.getProperty(key, juce::var());
    return p.isVoid() ? def : static_cast<int>(p);
}

} // namespace

// ==============================================================================
// Ephemeral Loopback HTTP Server (127.0.0.1:<port>/callback) for OAuth PKCE
// ==============================================================================
class Tone3000Client::LoopbackReceiver : private juce::Thread {
public:
    LoopbackReceiver() : juce::Thread("PedalBoard-T3K-Loopback") {}
    ~LoopbackReceiver() override { stop(); }

    bool start(std::function<void(juce::String)> cb) {
        stop();
        callback = std::move(cb);
        if (!listener.createListener(0, "127.0.0.1")) {
            return false;
        }
        const int boundPort = listener.getBoundPort();
        if (boundPort <= 0) {
            listener.close();
            return false;
        }
        port.store(boundPort, std::memory_order_release);
        startThread();
        return true;
    }

    void stop() {
        signalThreadShouldExit();
        listener.close();
        if (isThreadRunning()) {
            stopThread(2500);
        }
        port.store(0, std::memory_order_release);
    }

    juce::String getRedirectUri() const {
        const int p = port.load(std::memory_order_acquire);
        return p > 0 ? ("http://127.0.0.1:" + juce::String(p) + "/callback") : juce::String();
    }

private:
    void run() override {
        while (!threadShouldExit()) {
            std::unique_ptr<juce::StreamingSocket> conn(listener.waitForNextConnection());
            if (conn == nullptr) continue;
            if (threadShouldExit()) break;

            juce::MemoryOutputStream head;
            char buf[1024];
            while (!head.toString().contains("\r\n\r\n") && head.getDataSize() < 16384) {
                if (conn->waitUntilReady(true, 2000) != 1) break;
                const int n = conn->read(buf, static_cast<int>(sizeof(buf)), false);
                if (n <= 0) break;
                head.write(buf, static_cast<size_t>(n));
            }

            const auto reqLine = head.toString().upToFirstOccurrenceOf("\r\n", false, false);
            const auto target  = reqLine.fromFirstOccurrenceOf(" ", false, false).upToFirstOccurrenceOf(" ", false, false);
            const auto path    = target.upToFirstOccurrenceOf("?", false, false);
            const auto query   = target.contains("?") ? target.fromFirstOccurrenceOf("?", false, false) : juce::String();

            juce::String resp;
            if (reqLine.startsWith("GET ") && (path == "/callback" || path == "/")) {
                const juce::String html =
                    "<!doctype html><html><head><meta charset=\"utf-8\"><title>PedalBoard - TONE3000 Connected</title>"
                    "<style>body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;"
                    "background:#0d1118;color:#f1f5f9;font:15px/1.6 -apple-system,Segoe UI,sans-serif}"
                    "main{text-align:center;padding:34px 42px;border:1px solid #263144;border-radius:10px;background:#141923}"
                    "h1{margin:0 0 8px;font-size:20px;color:#38bdf8}p{margin:0;color:#94a3b8}</style></head>"
                    "<body><main><h1>TONE3000 Connected</h1>"
                    "<p>You can close this browser tab and return to PedalBoard.</p></main></body></html>";
                resp << "HTTP/1.1 200 OK\r\n"
                     << "Content-Type: text/html; charset=utf-8\r\n"
                     << "Content-Length: " << static_cast<int>(html.getNumBytesAsUTF8()) << "\r\n"
                     << "Cache-Control: no-store\r\n"
                     << "Connection: close\r\n\r\n"
                     << html;

                if (callback) {
                    auto cbCopy = callback;
                    juce::MessageManager::callAsync([cbCopy = std::move(cbCopy), query] { cbCopy(query); });
                }
            } else {
                resp << "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            }

            conn->write(resp.toRawUTF8(), static_cast<int>(resp.getNumBytesAsUTF8()));
            conn->close();
        }
    }

    juce::StreamingSocket listener;
    std::atomic<int> port { 0 };
    std::function<void(juce::String)> callback;
};

// ==============================================================================
// Tone3000Client Lifecycle & Storage
// ==============================================================================
Tone3000Client::Tone3000Client()
    : loopback(std::make_unique<LoopbackReceiver>()) {
    loadStoredState();
}

Tone3000Client::~Tone3000Client() {
    aliveFlag->store(false, std::memory_order_release);
    cancelActiveAuthFlow();
    workerPool.removeAllJobs(true, 4000);
}

juce::String Tone3000Client::getPublishableKey() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return publishableKey;
}

void Tone3000Client::setPublishableKey(const juce::String& newKey) {
    const auto trimmed = newKey.trim();
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        publishableKey = trimmed;
    }
    saveStoredKey(trimmed);
}

bool Tone3000Client::hasPublishableKey() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return publishableKey.isNotEmpty();
}

bool Tone3000Client::isConnected() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return tokens.isValid();
}

UserInfo Tone3000Client::getCachedUser() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return cachedUser;
}

void Tone3000Client::disconnect() {
    cancelActiveAuthFlow();
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        tokens = {};
        cachedUser = {};
    }
    clearStoredTokens();
}

void Tone3000Client::loadStoredState() {
    auto props = openTone3000Settings();
    const auto savedKey = props->getValue("publishable_key").trim();
    const auto tokenJson = unprotectSecret(props->getValue("oauth_tokens"));

    std::lock_guard<std::mutex> lock(stateMutex);
    if (savedKey.isNotEmpty()) {
        publishableKey = savedKey;
    } else if (const auto envKey = juce::SystemStats::getEnvironmentVariable("T3K_PUBLISHABLE_KEY", {}); envKey.isNotEmpty()) {
        publishableKey = envKey.trim();
    }

    if (tokenJson.isNotEmpty()) {
        const auto v = juce::JSON::parse(tokenJson);
        tokens.accessToken  = jsonStr(v, "access_token");
        tokens.refreshToken = jsonStr(v, "refresh_token");
        tokens.expiresAtMs  = static_cast<juce::int64>(v.getProperty("expires_at", 0));
    }
}

void Tone3000Client::saveStoredKey(const juce::String& k) {
    auto props = openTone3000Settings();
    props->setValue("publishable_key", k);
    props->saveIfNeeded();
}

void Tone3000Client::saveStoredTokens(const Tokens& t) {
    auto* obj = new juce::DynamicObject();
    obj->setProperty("access_token", t.accessToken);
    obj->setProperty("refresh_token", t.refreshToken);
    obj->setProperty("expires_at", t.expiresAtMs);
    const auto json = juce::JSON::toString(juce::var(obj), true);

    auto props = openTone3000Settings();
    props->setValue("oauth_tokens", protectSecret(json));
    props->saveIfNeeded();
}

void Tone3000Client::clearStoredTokens() {
    auto props = openTone3000Settings();
    props->removeValue("oauth_tokens");
    props->saveIfNeeded();
}

juce::File Tone3000Client::getCacheDirectory() {
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("PedalBoard")
                   .getChildFile("TONE3000Cache");
    dir.createDirectory();
    return dir;
}

juce::String Tone3000Client::sanitizeFilename(const juce::String& raw) {
    auto s = raw.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ")
                .trim()
                .replaceCharacter(' ', '_');
    while (s.contains("__")) s = s.replace("__", "_");
    if (s.length() > 64) s = s.substring(0, 64);
    return s.isEmpty() ? "capture" : s;
}

juce::File Tone3000Client::getCachedFilePathForModel(const ModelItem& model) {
    const auto ext = model.fileExtension();
    const auto fileName = sanitizeFilename(model.name) + "_" + juce::String(model.id) + ext;
    return getCacheDirectory().getChildFile(fileName);
}

bool Tone3000Client::isModelCached(const ModelItem& model) {
    const auto f = getCachedFilePathForModel(model);
    return f.existsAsFile() && f.getSize() > 0;
}

std::vector<CachedLocalFile> Tone3000Client::scanLocalCache() {
    std::vector<CachedLocalFile> out;
    const auto dir = getCacheDirectory();
    juce::Array<juce::File> files;
    dir.findChildFiles(files, juce::File::findFiles, false, "*.nam;*.wav;*.aif;*.aiff");
    for (const auto& f : files) {
        if (!f.existsAsFile() || f.getSize() <= 0) continue;
        CachedLocalFile item;
        item.file = f;
        item.displayName = f.getFileNameWithoutExtension().replaceCharacter('_', ' ');
        item.isIr = f.hasFileExtension(".wav;.aif;.aiff");
        item.sizeBytes = f.getSize();
        item.modifiedTime = f.getLastModificationTime();
        out.push_back(std::move(item));
    }
    std::sort(out.begin(), out.end(), [](const CachedLocalFile& a, const CachedLocalFile& b) {
        return a.modifiedTime > b.modifiedTime;
    });
    return out;
}

// ==============================================================================
// Helpers (PKCE, HTTP, Parsing)
// ==============================================================================
juce::String Tone3000Client::sha256Base64Url(const juce::String& input) {
    const auto digest = computeSha256(reinterpret_cast<const uint8_t*>(input.toRawUTF8()),
                                      input.getNumBytesAsUTF8());
    return toBase64Url(digest.data(), digest.size());
}

juce::String Tone3000Client::generateRandomBase64Url(int numBytes) {
    juce::Random rng(juce::Random::getSystemRandom().nextInt64() ^ juce::Time::getHighResolutionTicks());
    juce::MemoryBlock mb(static_cast<size_t>(std::max(16, numBytes)));
    for (size_t i = 0; i < mb.getSize(); ++i) {
        mb[i] = static_cast<char>(rng.nextInt(256));
    }
    return toBase64Url(mb.getData(), mb.getSize());
}

juce::String Tone3000Client::formEncode(const std::vector<std::pair<juce::String, juce::String>>& fields) {
    juce::StringArray parts;
    for (const auto& [k, v] : fields) {
        parts.add(juce::URL::addEscapeChars(k, true) + "=" + juce::URL::addEscapeChars(v, true));
    }
    return parts.joinIntoString("&");
}

Tone3000Client::HttpRes Tone3000Client::sendHttpSync(const juce::URL& url,
                                                     const juce::String& method,
                                                     const juce::StringPairArray& headers,
                                                     const juce::String& body,
                                                     int timeoutMs) {
    HttpRes out;
    juce::String headerBlock;
    for (const auto& k : headers.getAllKeys()) {
        headerBlock << k << ": " << headers[k] << "\r\n";
    }

    auto reqUrl = url;
    if (method == "POST" || method == "PUT") {
        reqUrl = reqUrl.withPOSTData(body);
    }

    int statusCode = 0;
    juce::StringPairArray respHeaders;
    auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withHttpRequestCmd(method)
                    .withExtraHeaders(headerBlock)
                    .withConnectionTimeoutMs(timeoutMs)
                    .withNumRedirectsToFollow(5)
                    .withStatusCode(&statusCode)
                    .withResponseHeaders(&respHeaders);

    if (auto stream = reqUrl.createInputStream(opts)) {
        stream->readIntoMemoryBlock(out.body);
        out.status = statusCode;
    } else {
        out.status = statusCode;
        if (statusCode == 0) {
            out.error = "Unable to connect to tone3000.com";
        }
    }
    return out;
}

juce::String Tone3000Client::parseErrorMessage(const HttpRes& res) {
    const auto v = res.json();
    const auto desc = jsonStr(v, "error_description");
    if (desc.isNotEmpty()) return desc;
    const auto err = jsonStr(v, "error");
    if (err.isNotEmpty()) return err;
    const auto msg = jsonStr(v, "message");
    if (msg.isNotEmpty()) return msg;
    return res.error;
}

UserInfo Tone3000Client::parseUser(const juce::var& v) {
    UserInfo u;
    u.id          = jsonInt(v, "id");
    u.username    = jsonStr(v, "username");
    u.displayName = jsonStr(v, "display_name");
    u.isVerified  = static_cast<bool>(v.getProperty("is_verified", false));
    return u;
}

ToneItem Tone3000Client::parseTone(const juce::var& v) {
    ToneItem t;
    t.id             = jsonInt(v, "id");
    t.title          = jsonStr(v, "title");
    t.description    = jsonStr(v, "description");
    t.gear           = jsonStr(v, "gear");
    t.format         = jsonStr(v, "format");
    t.license        = jsonStr(v, "license");
    t.url            = jsonStr(v, "url");
    t.publishedAt    = jsonStr(v, "published_at");
    t.creator        = parseUser(v.getProperty("user", juce::var()));
    t.modelsCount       = jsonInt(v, "models_count");
    t.a1ModelsCount     = jsonInt(v, "a1_models_count");
    t.a2ModelsCount     = jsonInt(v, "a2_models_count");
    t.customModelsCount = jsonInt(v, "custom_models_count");
    t.irsCount          = jsonInt(v, "irs_count");
    t.downloadsCount = jsonInt(v, "downloads_count");
    t.favoritesCount = jsonInt(v, "favorites_count");
    t.isFavorite     = static_cast<bool>(v.getProperty("is_favorite", false));

    const auto makesVar = v.getProperty("makes", juce::var());
    if (const auto* arr = makesVar.getArray()) {
        for (const auto& m : *arr) {
            const auto nm = jsonStr(m, "name");
            if (nm.isNotEmpty()) t.makes.add(nm);
        }
    }
    const auto tagsVar = v.getProperty("tags", juce::var());
    if (const auto* arr = tagsVar.getArray()) {
        for (const auto& tg : *arr) {
            const auto nm = jsonStr(tg, "name");
            if (nm.isNotEmpty()) t.tags.add(nm);
        }
    }
    return t;
}

ModelItem Tone3000Client::parseModel(const juce::var& v) {
    ModelItem m;
    m.id                  = jsonInt(v, "id");
    m.toneId              = jsonInt(v, "tone_id");
    m.name                = jsonStr(v, "name");
    m.modelUrl            = jsonStr(v, "model_url");
    m.size                = jsonStr(v, "size");
    m.architectureVersion = jsonStr(v, "architecture_version");
    return m;
}

TonePage Tone3000Client::parseTonePage(const juce::var& v) {
    TonePage page;
    page.page       = std::max(1, jsonInt(v, "page", 1));
    page.pageSize   = jsonInt(v, "page_size", 20);
    page.total      = jsonInt(v, "total", 0);
    page.totalPages = std::max(1, jsonInt(v, "total_pages", 1));
    const auto dataVar = v.getProperty("data", juce::var());
    if (const auto* arr = dataVar.isArray() ? dataVar.getArray() : v.getArray()) {
        page.items.reserve(static_cast<size_t>(arr->size()));
        for (const auto& item : *arr) {
            page.items.push_back(parseTone(item));
        }
    }
    if (page.total == 0) {
        page.total = static_cast<int>(page.items.size());
    }
    return page;
}

// ==============================================================================
// OAuth 2.0 PKCE Flows (Connect & Select Tone)
// ==============================================================================
void Tone3000Client::startConnectFlow(std::function<void(OAuthFlowResult)> onComplete) {
    juce::StringPairArray extra;
    beginOAuthFlow(extra, std::move(onComplete));
}

void Tone3000Client::startSelectToneFlow(const juce::String& gears,
                                         const juce::String& format,
                                         int architecture,
                                         std::function<void(OAuthFlowResult)> onComplete) {
    juce::StringPairArray extra;
    extra.set("prompt", "select_tone");
    extra.set("preview", "true");
    if (gears.isNotEmpty())  extra.set("gears", gears);
    if (format.isNotEmpty()) extra.set("format", format);
    if (architecture > 0)    extra.set("architecture", juce::String(architecture));
    beginOAuthFlow(extra, std::move(onComplete));
}

void Tone3000Client::cancelActiveAuthFlow() {
    if (loopback != nullptr) {
        loopback->stop();
    }
    authFlowActive.store(false, std::memory_order_release);
    pendingCodeVerifier.clear();
    pendingState.clear();
    pendingAuthCallback = nullptr;
}

juce::URL Tone3000Client::prepareEmbeddedOAuthUrl(const juce::String& redirectUri,
                                                  const juce::String& prompt,
                                                  const juce::String& gears,
                                                  const juce::String& format,
                                                  int architecture,
                                                  bool showMenubar) {
    const auto key = getPublishableKey();
    if (key.isEmpty()) {
        return {};
    }

    if (loopback != nullptr) {
        loopback->stop();
    }
    pendingCodeVerifier = generateRandomBase64Url(32);
    pendingState        = generateRandomBase64Url(16);
    const auto challenge = sha256Base64Url(pendingCodeVerifier);
    authFlowActive.store(true, std::memory_order_release);

    auto authUrl = juce::URL(apiOrigin + "/api/v1/oauth/authorize")
                       .withParameter("client_id", key)
                       .withParameter("redirect_uri", redirectUri)
                       .withParameter("response_type", "code")
                       .withParameter("code_challenge", challenge)
                       .withParameter("code_challenge_method", "S256")
                       .withParameter("state", pendingState);
    if (prompt.isNotEmpty())  authUrl = authUrl.withParameter("prompt", prompt);
    if (prompt == "select_tone") authUrl = authUrl.withParameter("preview", "true");
    if (gears.isNotEmpty())   authUrl = authUrl.withParameter("gears", gears);
    if (format.isNotEmpty())  authUrl = authUrl.withParameter("format", format);
    if (architecture > 0)     authUrl = authUrl.withParameter("architecture", juce::String(architecture));
    if (showMenubar)          authUrl = authUrl.withParameter("menubar", "true");
    return authUrl;
}

void Tone3000Client::completeEmbeddedOAuthCallback(const juce::String& query,
                                                   const juce::String& redirectUri,
                                                   std::function<void(OAuthFlowResult)> onComplete) {
    pendingAuthCallback = std::move(onComplete);
    handleLoopbackQuery(query, redirectUri);
}

void Tone3000Client::beginOAuthFlow(const juce::StringPairArray& extraParams,
                                    std::function<void(OAuthFlowResult)> onComplete) {
    const auto key = getPublishableKey();
    if (key.isEmpty()) {
        if (onComplete) {
            onComplete({ false, false, 0, "Enter your TONE3000 Publishable Key (t3k_pub_...) first." });
        }
        return;
    }

    cancelActiveAuthFlow();
    pendingAuthCallback = std::move(onComplete);
    pendingCodeVerifier = generateRandomBase64Url(32);
    pendingState        = generateRandomBase64Url(16);
    const auto challenge = sha256Base64Url(pendingCodeVerifier);

    auto alive = aliveFlag;
    if (!loopback->start([this, alive](juce::String query) {
            if (!alive->load(std::memory_order_acquire)) return;
            const auto uri = loopback->getRedirectUri();
            loopback->stop();
            handleLoopbackQuery(query, uri);
        })) {
        finishAuthFlow({ false, false, 0, "Could not bind local loopback port on 127.0.0.1." });
        return;
    }

    authFlowActive.store(true, std::memory_order_release);
    const auto redirectUri = loopback->getRedirectUri();

    auto authUrl = juce::URL(apiOrigin + "/api/v1/oauth/authorize")
                       .withParameter("client_id", key)
                       .withParameter("redirect_uri", redirectUri)
                       .withParameter("response_type", "code")
                       .withParameter("code_challenge", challenge)
                       .withParameter("code_challenge_method", "S256")
                       .withParameter("state", pendingState);
    for (const auto& k : extraParams.getAllKeys()) {
        authUrl = authUrl.withParameter(k, extraParams[k]);
    }

    if (!authUrl.launchInDefaultBrowser()) {
        loopback->stop();
        finishAuthFlow({ false, false, 0, "Failed to open system web browser for TONE3000 login." });
    }
}

void Tone3000Client::handleLoopbackQuery(const juce::String& query, const juce::String& redirectUri) {
    if (!authFlowActive.load(std::memory_order_acquire)) return;

    juce::String code, state, error, toneIdStr;
    bool canceled = false;

    const auto cleaned = query.upToFirstOccurrenceOf("#", false, false);
    for (const auto& pair : juce::StringArray::fromTokens(cleaned, "&", {})) {
        const auto k = juce::URL::removeEscapeChars(pair.upToFirstOccurrenceOf("=", false, false));
        const auto v = juce::URL::removeEscapeChars(pair.fromFirstOccurrenceOf("=", false, false).replaceCharacter('+', ' '));
        if (k == "code")          code = v;
        else if (k == "state")    state = v;
        else if (k == "error")    error = v;
        else if (k == "tone_id")  toneIdStr = v;
        else if (k == "canceled") canceled = (v == "true");
    }

    if (state != pendingState) {
        finishAuthFlow({ false, false, 0, "OAuth state mismatch. Please retry." });
        return;
    }
    if (canceled && code.isEmpty()) {
        finishAuthFlow({ false, true, 0, {} });
        return;
    }
    if (error.isNotEmpty()) {
        finishAuthFlow({ false, false, 0, "TONE3000 auth error: " + error });
        return;
    }
    if (code.isEmpty()) {
        finishAuthFlow({ false, false, 0, "Missing authorization code from TONE3000." });
        return;
    }

    const auto verifier = pendingCodeVerifier;
    const auto key = getPublishableKey();
    const int pickedToneId = toneIdStr.getIntValue();
    auto alive = aliveFlag;

    workerPool.addJob([this, alive, code, verifier, redirectUri, key, pickedToneId, canceled] {
        juce::StringPairArray hdrs;
        hdrs.set("Content-Type", "application/x-www-form-urlencoded");
        hdrs.set("Accept", "application/json");
        const auto body = formEncode({
            { "grant_type", "authorization_code" },
            { "code", code },
            { "code_verifier", verifier },
            { "redirect_uri", redirectUri },
            { "client_id", key }
        });

        const auto res = sendHttpSync(juce::URL(apiOrigin + kTokenEndpoint), "POST", hdrs, body);
        OAuthFlowResult flowRes;
        if (res.ok()) {
            const auto v = res.json();
            Tokens t;
            t.accessToken  = jsonStr(v, "access_token");
            t.refreshToken = jsonStr(v, "refresh_token");
            const double expiresIn = static_cast<double>(v.getProperty("expires_in", 3600));
            t.expiresAtMs = juce::Time::currentTimeMillis() + static_cast<juce::int64>(expiresIn * 1000.0);

            if (t.isValid()) {
                {
                    std::lock_guard<std::mutex> lock(stateMutex);
                    tokens = t;
                }
                saveStoredTokens(t);
                flowRes.connected = true;
                flowRes.canceled = canceled;
                flowRes.selectedToneId = pickedToneId;
            } else {
                flowRes.errorMessage = "Invalid token response from TONE3000.";
            }
        } else {
            const auto msg = parseErrorMessage(res);
            flowRes.errorMessage = msg.isNotEmpty() ? msg : ("Token exchange failed (HTTP " + juce::String(res.status) + ")");
        }

        juce::MessageManager::callAsync([this, alive, flowRes = std::move(flowRes)]() mutable {
            if (!alive->load(std::memory_order_acquire)) return;
            finishAuthFlow(std::move(flowRes));
        });
    });
}

void Tone3000Client::finishAuthFlow(OAuthFlowResult result) {
    authFlowActive.store(false, std::memory_order_release);
    pendingCodeVerifier.clear();
    pendingState.clear();
    auto cb = std::move(pendingAuthCallback);
    pendingAuthCallback = nullptr;
    if (cb) {
        cb(std::move(result));
    }
}

// ==============================================================================
// Authenticated REST Plumbing (Single-Flight Refresh + 401 Retry)
// ==============================================================================
Tone3000Client::Tokens Tone3000Client::getTokensCopy() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return tokens;
}

bool Tone3000Client::refreshTokensSync() {
    std::lock_guard<std::mutex> singleFlight(refreshMutex);
    auto cur = getTokensCopy();
    if (!cur.isValid()) return false;
    if (juce::Time::currentTimeMillis() < cur.expiresAtMs - kRefreshLeadMs) {
        return true;
    }

    const auto key = getPublishableKey();
    juce::StringPairArray hdrs;
    hdrs.set("Content-Type", "application/x-www-form-urlencoded");
    hdrs.set("Accept", "application/json");
    const auto body = formEncode({
        { "grant_type", "refresh_token" },
        { "refresh_token", cur.refreshToken },
        { "client_id", key }
    });

    const auto res = sendHttpSync(juce::URL(apiOrigin + kTokenEndpoint), "POST", hdrs, body);
    if (res.ok()) {
        const auto v = res.json();
        Tokens fresh;
        fresh.accessToken  = jsonStr(v, "access_token");
        fresh.refreshToken = jsonStr(v, "refresh_token");
        if (fresh.refreshToken.isEmpty()) fresh.refreshToken = cur.refreshToken;
        const double expiresIn = static_cast<double>(v.getProperty("expires_in", 3600));
        fresh.expiresAtMs = juce::Time::currentTimeMillis() + static_cast<juce::int64>(expiresIn * 1000.0);
        if (fresh.isValid()) {
            {
                std::lock_guard<std::mutex> lock(stateMutex);
                tokens = fresh;
            }
            saveStoredTokens(fresh);
            return true;
        }
    }

    if (res.status == 400 || res.status == 401) {
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            tokens = {};
            cachedUser = {};
        }
        clearStoredTokens();
        return false;
    }
    return true; // Transient network error: keep tokens
}

juce::String Tone3000Client::getValidAccessToken(bool forceRefresh) {
    auto cur = getTokensCopy();
    if (!cur.isValid()) return {};

    if (forceRefresh || juce::Time::currentTimeMillis() > cur.expiresAtMs - kRefreshLeadMs) {
        if (forceRefresh) {
            std::lock_guard<std::mutex> lock(stateMutex);
            tokens.expiresAtMs = 0;
        }
        if (!refreshTokensSync()) return {};
        cur = getTokensCopy();
    }
    return cur.accessToken;
}

Tone3000Client::HttpRes Tone3000Client::authenticatedRequestSync(const juce::String& method,
                                                                 const juce::String& pathAndQuery,
                                                                 const juce::String& body,
                                                                 ApiError& outErr) {
    auto bearer = getValidAccessToken(false);
    if (bearer.isEmpty()) {
        outErr = { 401, "Not connected to TONE3000." };
        return {};
    }

    auto doSend = [&](const juce::String& tok) {
        juce::StringPairArray hdrs;
        hdrs.set("Authorization", "Bearer " + tok);
        hdrs.set("Accept", "application/json");
        if (body.isNotEmpty()) {
            hdrs.set("Content-Type", "application/json");
        }
        return sendHttpSync(juce::URL(apiOrigin + pathAndQuery), method, hdrs, body);
    };

    auto res = doSend(bearer);
    if (res.status == 401) {
        const auto refreshed = getValidAccessToken(true);
        if (refreshed.isNotEmpty()) {
            res = doSend(refreshed);
        }
    }

    if (res.status == 0) {
        outErr = { 0, res.error };
    } else if (!res.ok()) {
        outErr = { res.status, parseErrorMessage(res) };
    }
    return res;
}

// ==============================================================================
// Public REST API v1 Calls
// ==============================================================================
void Tone3000Client::fetchCurrentUser(std::function<void(ApiResult<UserInfo>)> done) {
    auto alive = aliveFlag;
    workerPool.addJob([this, alive, cb = std::move(done)]() mutable {
        ApiError err;
        const auto res = authenticatedRequestSync("GET", "/api/v1/user", {}, err);
        ApiResult<UserInfo> out = res.ok()
            ? ApiResult<UserInfo>::success(parseUser(res.json()))
            : ApiResult<UserInfo>::failure(err);
        if (out.ok()) {
            std::lock_guard<std::mutex> lock(stateMutex);
            cachedUser = *out.value;
        }
        juce::MessageManager::callAsync([alive, cb = std::move(cb), out = std::move(out)]() mutable {
            if (!alive->load(std::memory_order_acquire)) return;
            if (cb) cb(std::move(out));
        });
    });
}

void Tone3000Client::fetchTrending(const juce::String& gear, std::function<void(ApiResult<TonePage>)> done) {
    auto url = juce::URL("https://x/api/v1/tones/trending");
    if (gear.isNotEmpty()) url = url.withParameter("gear", gear);
    const auto path = pathOfUrl(url);
    auto alive = aliveFlag;

    workerPool.addJob([this, alive, path, cb = std::move(done)]() mutable {
        ApiError err;
        const auto res = authenticatedRequestSync("GET", path, {}, err);
        ApiResult<TonePage> out = res.ok()
            ? ApiResult<TonePage>::success(parseTonePage(res.json()))
            : ApiResult<TonePage>::failure(err);
        juce::MessageManager::callAsync([alive, cb = std::move(cb), out = std::move(out)]() mutable {
            if (!alive->load(std::memory_order_acquire)) return;
            if (cb) cb(std::move(out));
        });
    });
}

void Tone3000Client::fetchLatest(std::function<void(ApiResult<TonePage>)> done) {
    auto alive = aliveFlag;
    workerPool.addJob([this, alive, cb = std::move(done)]() mutable {
        ApiError err;
        const auto res = authenticatedRequestSync("GET", "/api/v1/tones/latest", {}, err);
        ApiResult<TonePage> out = res.ok()
            ? ApiResult<TonePage>::success(parseTonePage(res.json()))
            : ApiResult<TonePage>::failure(err);
        juce::MessageManager::callAsync([alive, cb = std::move(cb), out = std::move(out)]() mutable {
            if (!alive->load(std::memory_order_acquire)) return;
            if (cb) cb(std::move(out));
        });
    });
}

void Tone3000Client::searchTones(const SearchQuery& q, std::function<void(ApiResult<TonePage>)> done) {
    auto url = juce::URL("https://x/api/v1/tones/search")
                   .withParameter("page", juce::String(std::max(1, q.page)))
                   .withParameter("page_size", juce::String(std::clamp(q.pageSize, 6, 48)));
    if (q.query.trim().isNotEmpty()) url = url.withParameter("query", q.query.trim());
    if (q.sort.isNotEmpty())         url = url.withParameter("sort", q.sort);
    if (q.gear.isNotEmpty())         url = url.withParameter("gears", q.gear);
    if (q.format.isNotEmpty())       url = url.withParameter("format", q.format);
    if (q.architecture > 0)          url = url.withParameter("architecture", juce::String(q.architecture));

    const auto path = pathOfUrl(url);
    auto alive = aliveFlag;
    workerPool.addJob([this, alive, path, cb = std::move(done)]() mutable {
        ApiError err;
        const auto res = authenticatedRequestSync("GET", path, {}, err);
        ApiResult<TonePage> out = res.ok()
            ? ApiResult<TonePage>::success(parseTonePage(res.json()))
            : ApiResult<TonePage>::failure(err);
        juce::MessageManager::callAsync([alive, cb = std::move(cb), out = std::move(out)]() mutable {
            if (!alive->load(std::memory_order_acquire)) return;
            if (cb) cb(std::move(out));
        });
    });
}

void Tone3000Client::fetchLibrary(const juce::String& endpoint,
                                  const juce::String& gear,
                                  const juce::String& query,
                                  int page,
                                  std::function<void(ApiResult<TonePage>)> done) {
    auto url = juce::URL("https://x/api/v1/tones/" + endpoint)
                   .withParameter("page", juce::String(std::max(1, page)))
                   .withParameter("page_size", "24");
    if (gear.isNotEmpty())         url = url.withParameter("gear", gear);
    if (query.trim().isNotEmpty()) url = url.withParameter("query", query.trim());

    const auto path = pathOfUrl(url);
    auto alive = aliveFlag;
    workerPool.addJob([this, alive, path, cb = std::move(done)]() mutable {
        ApiError err;
        const auto res = authenticatedRequestSync("GET", path, {}, err);
        ApiResult<TonePage> out = res.ok()
            ? ApiResult<TonePage>::success(parseTonePage(res.json()))
            : ApiResult<TonePage>::failure(err);
        juce::MessageManager::callAsync([alive, cb = std::move(cb), out = std::move(out)]() mutable {
            if (!alive->load(std::memory_order_acquire)) return;
            if (cb) cb(std::move(out));
        });
    });
}

void Tone3000Client::fetchModelsForTone(int toneId,
                                        int architecture,
                                        std::function<void(ApiResult<std::vector<ModelItem>>)> done) {
    auto alive = aliveFlag;
    workerPool.addJob([this, alive, toneId, architecture, cb = std::move(done)]() mutable {
        auto queryModels = [&](int archFilter, ApiError& err) -> std::pair<bool, std::vector<ModelItem>> {
            auto url = juce::URL("https://x/api/v1/models")
                           .withParameter("tone_id", juce::String(toneId))
                           .withParameter("page_size", "100");
            if (archFilter > 0) {
                url = url.withParameter("architecture", juce::String(archFilter));
            }
            const auto res = authenticatedRequestSync("GET", pathOfUrl(url), {}, err);
            if (!res.ok()) return { false, {} };
            std::vector<ModelItem> models;
            const auto parsed = res.json();
            const auto dataVar = parsed.getProperty("data", juce::var());
            if (const auto* arr = dataVar.isArray() ? dataVar.getArray() : parsed.getArray()) {
                for (const auto& item : *arr) {
                    models.push_back(parseModel(item));
                }
            }
            return { true, std::move(models) };
        };

        auto appendUnique = [](std::vector<ModelItem>& dst, const std::vector<ModelItem>& src) {
            for (const auto& item : src) {
                bool dup = false;
                for (const auto& ex : dst) {
                    if (ex.id == item.id) { dup = true; break; }
                }
                if (!dup) dst.push_back(item);
            }
        };

        ApiError err;
        auto [ok, models] = queryModels(architecture, err);
        // Bidirectional smart fallback: supports BOTH A1/Custom AND A2 Fast models seamlessly!
        if (ok && models.empty()) {
            const int altArch = (architecture == 2) ? 0 : 2;
            ApiError fallbackErr;
            auto [okFb, fbModels] = queryModels(altArch, fallbackErr);
            if (okFb) appendUnique(models, fbModels);
        } else if (ok && architecture == 0) {
            // Also check if tone provides A2 captures alongside A1 so user sees every compatible capture
            ApiError a2Err;
            auto [okA2, a2Models] = queryModels(2, a2Err);
            if (okA2 && !a2Models.empty()) {
                appendUnique(models, a2Models);
            }
        }

        ApiResult<std::vector<ModelItem>> out = ok
            ? ApiResult<std::vector<ModelItem>>::success(std::move(models))
            : ApiResult<std::vector<ModelItem>>::failure(err);

        juce::MessageManager::callAsync([alive, cb = std::move(cb), out = std::move(out)]() mutable {
            if (!alive->load(std::memory_order_acquire)) return;
            if (cb) cb(std::move(out));
        });
    });
}

void Tone3000Client::fetchToneWithModels(int toneId,
                                         int architecture,
                                         std::function<void(ApiResult<std::pair<ToneItem, std::vector<ModelItem>>>)> done) {
    auto alive = aliveFlag;
    workerPool.addJob([this, alive, toneId, architecture, cb = std::move(done)]() mutable {
        auto toneUrl = juce::URL("https://x/api/v1/tones/" + juce::String(toneId));
        if (architecture > 0) {
            toneUrl = toneUrl.withParameter("architecture", juce::String(architecture));
        }

        ApiError err;
        const auto toneRes = authenticatedRequestSync("GET", pathOfUrl(toneUrl), {}, err);
        if (!toneRes.ok()) {
            auto fail = ApiResult<std::pair<ToneItem, std::vector<ModelItem>>>::failure(err);
            juce::MessageManager::callAsync([alive, cb = std::move(cb), fail = std::move(fail)]() mutable {
                if (!alive->load(std::memory_order_acquire)) return;
                if (cb) cb(std::move(fail));
            });
            return;
        }

        auto tone = parseTone(toneRes.json());

        auto queryModels = [&](int archFilter, ApiError& mErr) -> std::pair<bool, std::vector<ModelItem>> {
            auto url = juce::URL("https://x/api/v1/models")
                           .withParameter("tone_id", juce::String(toneId))
                           .withParameter("page_size", "100");
            if (archFilter > 0) url = url.withParameter("architecture", juce::String(archFilter));
            const auto res = authenticatedRequestSync("GET", pathOfUrl(url), {}, mErr);
            if (!res.ok()) return { false, {} };
            std::vector<ModelItem> models;
            const auto parsed = res.json();
            const auto dataVar = parsed.getProperty("data", juce::var());
            if (const auto* arr = dataVar.isArray() ? dataVar.getArray() : parsed.getArray()) {
                for (const auto& item : *arr) {
                    models.push_back(parseModel(item));
                }
            }
            return { true, std::move(models) };
        };

        auto appendUnique = [](std::vector<ModelItem>& dst, const std::vector<ModelItem>& src) {
            for (const auto& item : src) {
                bool dup = false;
                for (const auto& ex : dst) {
                    if (ex.id == item.id) { dup = true; break; }
                }
                if (!dup) dst.push_back(item);
            }
        };

        ApiError mErr;
        bool okM = false;
        std::vector<ModelItem> models;

        if (architecture == 2) {
            auto [ok2, m2] = queryModels(2, mErr);
            okM = ok2;
            models = std::move(m2);
            if (okM && models.empty()) {
                ApiError fbErr;
                auto [ok1, m1] = queryModels(0, fbErr);
                if (ok1) appendUnique(models, m1);
            }
        } else {
            // Default / All compatible: query A1+Custom+IR AND A2 whenever tone has A2 captures (or A1 returned empty!)
            const bool onlyA2Expected = (tone.a2ModelsCount > 0 && tone.a1ModelsCount == 0 && tone.customModelsCount == 0 && tone.irsCount == 0);
            if (onlyA2Expected) {
                auto [ok2, m2] = queryModels(2, mErr);
                okM = ok2;
                models = std::move(m2);
            } else {
                auto [ok1, m1] = queryModels(0, mErr);
                okM = ok1;
                models = std::move(m1);
                if (okM && (models.empty() || tone.a2ModelsCount > 0)) {
                    ApiError a2Err;
                    auto [ok2, m2] = queryModels(2, a2Err);
                    if (ok2 && !m2.empty()) {
                        appendUnique(models, m2);
                    }
                }
            }
        }

        if (okM && tone.modelsCount == 0 && !models.empty()) {
            tone.modelsCount = static_cast<int>(models.size());
        }

        auto out = okM
            ? ApiResult<std::pair<ToneItem, std::vector<ModelItem>>>::success({ std::move(tone), std::move(models) })
            : ApiResult<std::pair<ToneItem, std::vector<ModelItem>>>::failure(mErr);

        juce::MessageManager::callAsync([alive, cb = std::move(cb), out = std::move(out)]() mutable {
            if (!alive->load(std::memory_order_acquire)) return;
            if (cb) cb(std::move(out));
        });
    });
}

void Tone3000Client::setToneFavorite(int toneId, bool favorite, std::function<void(ApiResult<bool>)> done) {
    const auto path = "/api/v1/tones/" + juce::String(toneId) + "/favorite";
    const auto method = favorite ? juce::String("PUT") : juce::String("DELETE");
    auto alive = aliveFlag;

    workerPool.addJob([this, alive, path, method, favorite, cb = std::move(done)]() mutable {
        ApiError err;
        const auto res = authenticatedRequestSync(method, path, {}, err);
        auto out = res.ok() ? ApiResult<bool>::success(favorite) : ApiResult<bool>::failure(err);
        juce::MessageManager::callAsync([alive, cb = std::move(cb), out = std::move(out)]() mutable {
            if (!alive->load(std::memory_order_acquire)) return;
            if (cb) cb(std::move(out));
        });
    });
}

void Tone3000Client::downloadModelFile(const ModelItem& model, std::function<void(ApiResult<juce::File>)> done) {
    const auto target = getCachedFilePathForModel(model);
    if (target.existsAsFile() && target.getSize() > 0) {
        juce::MessageManager::callAsync([cb = std::move(done), target] {
            if (cb) cb(ApiResult<juce::File>::success(target));
        });
        return;
    }

    const auto modelPath = pathOfUrl(juce::URL(model.modelUrl));
    auto alive = aliveFlag;

    workerPool.addJob([this, alive, modelPath, target, cb = std::move(done)]() mutable {
        ApiError err;
        const auto res = authenticatedRequestSync("GET", modelPath, {}, err);
        ApiResult<juce::File> out;
        if (!res.ok()) {
            out = ApiResult<juce::File>::failure(err);
        } else if (res.body.getSize() == 0) {
            out = ApiResult<juce::File>::failure({ 0, "Downloaded file was empty." });
        } else if (target.getParentDirectory().createDirectory() &&
                   target.replaceWithData(res.body.getData(), res.body.getSize())) {
            out = ApiResult<juce::File>::success(target);
        } else {
            out = ApiResult<juce::File>::failure({ 0, "Failed to write cached file: " + target.getFullPathName() });
        }

        juce::MessageManager::callAsync([alive, cb = std::move(cb), out = std::move(out)]() mutable {
            if (!alive->load(std::memory_order_acquire)) return;
            if (cb) cb(std::move(out));
        });
    });
}

} // namespace t3k
} // namespace pedalboard
