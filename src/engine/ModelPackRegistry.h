#pragma once

#include "Gp200Catalogue.h"
#include "ModuleId.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <juce_core/juce_core.h>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif

namespace pedalboard {

// Metadata matching Daftar_Effect_GP-200_Checklist_Gratis.xlsx for every GP-200 Excel No (1..265).
struct ExcelPackInfo {
    bool supportsPack = false;          // True for all 193 NAM / IR models (false for 72 DSP-only effects)
    bool isIr = false;                  // True for .wav/.aif IR (70 CAB + #11 AC Refiner + 9 Space RVB)
    bool isFreeClosestAlt = false;      // True for the 32 rare/generic models using closest free alternative
    const char* searchQuery = "";       // Exact verified free search query for TONE3000
    const char* externalFreeUrl = "";   // Non-TONE3000 open IR URL (acousticir.ovh, shift-line, EMT 140, echothief)
};

// Lazy-loaded capture bytes returned on demand (ONLY when a model is actively selected!).
struct ModelPackAsset {
    int excelNo = 0;
    bool isIr = false;
    bool fromEmbeddedBundle = false;
    juce::String displayName;
    juce::String originLabel; // "Bundled Pack" or "Local Pack"
    juce::MemoryBlock bytes;
};

struct ModelPackStats {
    int totalMapped = 0;
    int bundledCount = 0;
    int localOverrideCount = 0;
    int ampMapped = 0;
    int cabMapped = 0;
    int pedalMapped = 0;
    int reverbMapped = 0;
    juce::int64 embeddedArchiveBytes = 0;
};

// ==============================================================================
// ModelPackRegistry — Single-Binary Bundled ModelPacks.zip + Local Override Lazy Loader
//
// Resolution priority (highest wins):
//   1. %APPDATA%\PedalBoard\ModelPacks\  (instant user override without rebuilding)
//   2. <executable_or_vst3_dir>\ModelPacks\ (optional portable sibling folder)
//   3. Embedded Windows PE RCDATA "PB_MODELPACKS" (ModelPacks.zip inside .exe & .vst3!)
//
// Memory efficiency:
//   - At startup / rescan, ONLY the ZIP central directory index (~10 KB) is scanned.
//   - Zero .nam or .wav payloads are decompressed until the user actually selects that model.
//   - Only the 1-3 currently active models ever live in DSP RAM (~1-2 MB).
// ==============================================================================
class ModelPackRegistry {
public:
    static ModelPackRegistry& instance() {
        static ModelPackRegistry reg;
        return reg;
    }

    static juce::File getUserOverrideDirectory() {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("PedalBoard")
                       .getChildFile("ModelPacks");
        if (!dir.exists()) {
            dir.createDirectory();
        }
        return dir;
    }

    static juce::File getPortableSiblingDirectory() {
        const auto exe = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        return exe.getParentDirectory().getChildFile("ModelPacks");
    }

    // Shared Excel aliases so PRE Boosts (#4..#10) and DST Boosts (#52..#58) share a single .nam file!
    static int getSharedAliasExcelNo(int excelNo) noexcept {
        if (excelNo >= 4 && excelNo <= 10)  return excelNo + 48; // 4->52 .. 10->58
        if (excelNo >= 52 && excelNo <= 58) return excelNo - 48; // 52->4 .. 58->10
        return 0;
    }

    static juce::String getExpectedFilenameHint(ModuleId mod, int modelIndex) {
        const auto& entry = gp200::getModelEntry(mod, modelIndex);
        const auto info = getExcelPackInfo(entry.excelNo);
        if (!info.supportsPack) return {};
        juce::String safeName = juce::String::fromUTF8(entry.name)
            .retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ")
            .trim()
            .replaceCharacter(' ', '_');
        while (safeName.contains("__")) safeName = safeName.replace("__", "_");
        const auto numStr = juce::String(entry.excelNo).paddedLeft('0', 3);
        return numStr + "_" + (safeName.isNotEmpty() ? safeName : "Model") + (info.isIr ? ".wav" : ".nam");
    }

    // Exact 1:1 mapping from Daftar_Effect_GP-200_Checklist_Gratis.xlsx
    static ExcelPackInfo getExcelPackInfo(int excelNo) noexcept {
        switch (excelNo) {
            // PRE Boosts, Acoustic, Tape (#4..#12, #20)
            case 4:  case 52: return { true, false, false, "MXR Micro Amp", "" };
            case 5:  case 53: return { true, false, false, "Xotic AC Booster", "" };
            case 6:  case 54: return { true, false, false, "Xotic BB Preamp", "" };
            case 7:  case 55: return { true, false, false, "Xotic RC Booster", "" };
            case 8:  case 56: return { true, false, false, "Fortin Grind", "" };
            case 9:  case 57: return { true, false, true,  "Xotic RC Booster", "" };
            case 10: case 58: return { true, false, false, "Xotic EP Booster", "" };
            case 11:          return { true, true,  true,  "Acoustic", "https://acousticir.ovh/" };
            case 12:          return { true, false, false, "Boss AC-3", "" };
            case 20:          return { true, false, false, "Strymon Deco Tape Saturation", "" };

            // DST Drives, Fuzzes, Distortions, Bass Drives (#26..#51)
            case 26: return { true, false, false, "Ibanez TS808", "" };
            case 27: return { true, false, false, "Ibanez TS9", "" };
            case 28: return { true, false, false, "Boss OD-1", "" };
            case 29: return { true, false, false, "Klon Centaur", "" };
            case 30: return { true, false, false, "Providence SOV-2", "" };
            case 31: return { true, false, false, "Boss SD-1", "" };
            case 32: return { true, false, false, "Tube Screamer", "" };
            case 33: return { true, false, false, "Boss BD-2", "" };
            case 34: return { true, false, false, "Fulltone OCD", "" };
            case 35: return { true, false, false, "Marshall Bluesbreaker", "" };
            case 36: return { true, false, false, "JCM800", "" };
            case 37: return { true, false, false, "Zendrive", "" };
            case 38: return { true, false, false, "Paul Cochrane Timmy", "" };
            case 39: return { true, false, false, "Precision Drive", "" };
            case 40: return { true, false, false, "Big Muff", "" };
            case 41: return { true, false, false, "Fuzz Face", "" };
            case 42: return { true, false, false, "Tone Bender", "" };
            case 43: return { true, false, false, "MXR Distortion+", "" };
            case 44: return { true, false, false, "Boss DS-1", "" };
            case 45: return { true, false, false, "ProCo Rat", "" };
            case 46: return { true, false, false, "Guv'nor", "" };
            case 47: return { true, false, false, "Shredmaster", "" };
            case 48: return { true, false, false, "MI Audio Crunch Box", "" };
            case 49: return { true, false, true,  "Fulltone OCD", "" };
            case 50: return { true, false, false, "Boss ODB-3", "" };
            case 51: return { true, false, false, "Darkglass B7K", "" };

            // AMP Heads (#59..#129)
            case 59: return { true, false, false, "Fender Tweed Deluxe", "" };
            case 60: case 61: return { true, false, false, "Fender 59 Bassman", "" };
            case 62: return { true, false, false, "Fender 65 Twin Reverb", "" };
            case 63: return { true, false, false, "Fender Deluxe Reverb", "" };
            case 64: return { true, false, false, "Fender Vibroverb", "" };
            case 65: return { true, false, false, "Silverface Twin Reverb", "" };
            case 66: case 67: return { true, false, false, "Supro", "" };
            case 68: return { true, false, false, "VOX AC15", "" };
            case 69: return { true, false, false, "VOX AC30HW", "" };
            case 70: return { true, false, false, "VOX AC30 Top Boost", "" };
            case 71: return { true, false, false, "Roland JC-120", "" };
            case 72: case 73: return { true, false, false, "Matchless Chieftain", "" };
            case 74: case 75: return { true, false, false, "Mesa Lone Star", "" };
            case 76: case 77: return { true, false, false, "Bogner Shiva", "" };
            case 78: case 79: return { true, false, false, "Bogner Ecstasy", "" };
            case 80: case 81: return { true, false, false, "Bogner Ecstasy Red", "" };
            case 82: case 83: return { true, false, false, "Dr Z Maz 38", "" };
            case 84: return { true, false, true,  "VOX AC30", "" };
            case 85: return { true, false, true,  "VOX AC30 Top Boost", "" };
            case 86: return { true, false, true,  "Marshall JMP 50", "" };
            case 87: case 88: return { true, false, true, "Bad Cat Hot Cat", "" };
            case 89: case 90: case 91: return { true, false, false, "Soldano SLO-100", "" };
            case 92: case 93: case 94: return { true, false, false, "Marshall JTM45", "" };
            case 95: case 96: case 97: return { true, false, false, "Marshall JMP 50", "" };
            case 98: return { true, false, false, "Marshall 1959", "" };
            case 99: return { true, false, false, "Marshall JCM800", "" };
            case 100: return { true, false, false, "Marshall JCM900", "" };
            case 101: case 102: return { true, false, false, "Friedman BE-100", "" };
            case 103: case 104: return { true, false, false, "Friedman BE-100 HBE", "" };
            case 105: case 106: case 107: return { true, false, false, "Mesa Mark IIC+", "" };
            case 108: case 109: case 110: return { true, false, false, "Mesa Mark IV", "" };
            case 111: return { true, false, false, "Mesa Dual Rectifier Vintage", "" };
            case 112: return { true, false, false, "Mesa Dual Rectifier Modern", "" };
            case 113: return { true, false, false, "Orange AD30", "" };
            case 114: return { true, false, false, "Orange Rockerverb 100", "" };
            case 115: return { true, false, false, "Peavey 5150", "" };
            case 116: case 117: return { true, false, false, "ENGL Savage 120", "" };
            case 118: return { true, false, false, "ENGL Powerball", "" };
            case 119: case 120: case 121: case 122: return { true, false, false, "Diezel VH4", "" };
            case 123: return { true, false, false, "Ampeg SVT", "" };
            case 124: return { true, false, false, "VOX AC100", "" };
            case 125: return { true, false, true,  "Mesa Subway", "" };
            case 126: return { true, false, false, "Ampeg B-15", "" };
            case 127: return { true, false, true,  "Fender Dual Showman", "" };
            case 128: case 129: return { true, false, true, "AER", "" };

            // CAB & Acoustic IRs (#132..#201)
            case 132: return { true, true, true,  "Fender Champ", "" };
            case 133: return { true, true, false, "Fender Champ", "" };
            case 134: return { true, true, false, "Fender Princeton", "" };
            case 135: return { true, true, false, "Fender Tweed", "" };
            case 136: return { true, true, false, "Tweed Deluxe", "" };
            case 137: return { true, true, false, "Fender Deluxe", "" };
            case 138: return { true, true, true,  "Fender Princeton", "" };
            case 139: return { true, true, false, "Twin Reverb", "" };
            case 140: return { true, true, true,  "Fender 2x12", "" };
            case 141: return { true, true, false, "Bassman 2x12", "" };
            case 142: return { true, true, false, "Bassman 4x10", "" };
            case 143: return { true, true, false, "JC-120", "" };
            case 144: return { true, true, false, "Marshall 1x12", "" };
            case 145: return { true, true, false, "Marshall 2550", "" };
            case 146: return { true, true, false, "Marshall 1960AV", "" };
            case 147: return { true, true, false, "Marshall Basketweave", "" };
            case 148: return { true, true, true,  "Marshall 4x12", "" };
            case 149: return { true, true, false, "Marshall Greenback", "" };
            case 150: return { true, true, false, "G12T-75", "" };
            case 151: return { true, true, false, "1968 Marshall", "" };
            case 152: return { true, true, false, "VOX AC15", "" };
            case 153: return { true, true, false, "VOX AC30", "" };
            case 154: return { true, true, true,  "Fender Deluxe", "" };
            case 155: return { true, true, false, "Bogner Shiva", "" };
            case 156: return { true, true, true,  "1x12 V30", "" };
            case 157: return { true, true, false, "Matchless", "" };
            case 158: return { true, true, true,  "Greenback", "" };
            case 159: return { true, true, false, "Morgan", "" };
            case 160: return { true, true, false, "Mesa Rectifier 4x12", "" };
            case 161: case 162: return { true, true, false, "Mesa Lonestar", "" };
            case 163: return { true, true, false, "Mesa 1x12", "" };
            case 164: return { true, true, true,  "Two-Rock", "" };
            case 165: return { true, true, true,  "Supro", "" };
            case 166: return { true, true, false, "Matchless 2x12", "" };
            case 167: return { true, true, false, "Fryette Deliverance", "" };
            case 168: return { true, true, false, "Two-Rock", "" };
            case 169: return { true, true, false, "Alnico Blue", "" };
            case 170: return { true, true, false, "Peavey 6505", "" };
            case 171: return { true, true, false, "Bogner 4x12", "" };
            case 172: return { true, true, false, "ENGL 4x12", "" };
            case 173: return { true, true, false, "Uberkab", "" };
            case 174: return { true, true, false, "Soldano 4x12", "" };
            case 175: return { true, true, false, "Orange PPC412", "" };
            case 176: return { true, true, false, "Hiwatt 4x12", "" };
            case 177: return { true, true, true,  "Hiwatt 4x12", "" };
            case 178: return { true, true, false, "Dumble", "" };
            case 179: return { true, true, false, "Diezel 4x12", "" };
            case 180: return { true, true, false, "Triamp", "" };
            case 181: return { true, true, false, "Mesa Road King", "" };
            case 182: return { true, true, true,  "Bassman 115", "" };
            case 183: return { true, true, false, "Eden 4x10", "" };
            case 184: return { true, true, true,  "Bass 1x15", "https://shift-line.com/irpackbass" };
            case 185: return { true, true, true,  "Eden 4x10", "" };
            case 186: return { true, true, true,  "Mesa Subway", "" };
            case 187: return { true, true, true,  "Markbass", "" };
            case 188: return { true, true, true,  "Ampeg SGT-DI", "" };
            case 189: return { true, true, false, "Ampeg 4x10", "" };
            case 190: return { true, true, false, "Ampeg 8x10", "" };
            case 191: return { true, true, false, "Hartke", "" };
            case 192: case 193: case 194: case 195: case 197: case 198: case 199:
                return { true, true, false, "Acoustic", "https://acousticir.ovh/" };
            case 196: case 200: case 201:
                return { true, true, true,  "Acoustic", "https://acousticir.ovh/" };

            // RVB Space / Plate / Spring IRs (#252..#260)
            case 252: case 254: case 258: case 260:
                return { true, true, false, "reverb", "https://www.echothief.com/" };
            case 253: case 257: case 259:
                return { true, true, false, "reverb", "https://www.liquidsonics.com/why-fusion-ir-is-different-from-other-convolution-reverbs/" };
            case 255:
                return { true, true, false, "EMT 140", "https://oramics.github.io/sampled/IR/EMT140-Plate/" };
            case 256:
                return { true, true, false, "spring reverb", "" };

            default:
                return {};
        }
    }

    uint64_t getVersion() const noexcept {
        return version.load(std::memory_order_acquire);
    }

    void rescan() {
        std::lock_guard<std::mutex> lock(mutex);
        indexMap.clear();
        embeddedPtr = nullptr;
        embeddedSize = 0;

        // Tier 3 (lowest priority, overridden by disk): Embedded Windows PE RCDATA "PB_MODELPACKS"
        locateEmbeddedResourceLocked();
        if (embeddedPtr != nullptr && embeddedSize > 22) {
            juce::MemoryInputStream mis(embeddedPtr, embeddedSize, false);
            juce::ZipFile zip(mis);
            const int numEntries = zip.getNumEntries();
            for (int i = 0; i < numEntries; ++i) {
                if (const auto* ze = zip.getEntry(i)) {
                    if (ze->uncompressedSize <= 0 || ze->filename.endsWithChar('/') || ze->filename.endsWithChar('\\')) {
                        continue;
                    }
                    const auto leaf = ze->filename.fromLastOccurrenceOf("/", false, false)
                                                  .fromLastOccurrenceOf("\\", false, false);
                    bool isIr = false;
                    const int excelNo = parseExcelNoFromFilename(leaf, isIr);
                    if (excelNo > 0) {
                        IndexEntry ie;
                        ie.excelNo = excelNo;
                        ie.isIr = isIr;
                        ie.fromEmbedded = true;
                        ie.zipEntryIndex = i;
                        ie.displayName = cleanDisplayName(leaf);
                        indexMap[makeKey(excelNo, isIr)] = std::move(ie);
                    }
                }
            }
        }

        // Tier 2: Portable sibling folder beside .exe / .vst3
        scanDiskDirectoryLocked(getPortableSiblingDirectory(), false);

        // Tier 1 (highest priority): User local override (%APPDATA%/PedalBoard/ModelPacks)
        scanDiskDirectoryLocked(getUserOverrideDirectory(), true);

        version.fetch_add(1, std::memory_order_release);
    }

    bool hasCapture(int excelNo, bool wantIr) const {
        std::lock_guard<std::mutex> lock(mutex);
        if (indexMap.find(makeKey(excelNo, wantIr)) != indexMap.end()) return true;
        const int alias = getSharedAliasExcelNo(excelNo);
        return (alias > 0) && (indexMap.find(makeKey(alias, wantIr)) != indexMap.end());
    }

    juce::String getCaptureSummary(int excelNo, bool wantIr) const {
        std::lock_guard<std::mutex> lock(mutex);
        const auto* ie = findEntryLocked(excelNo, wantIr);
        if (ie == nullptr) return {};
        return ie->displayName + (ie->fromEmbedded ? " [Bundled]" : " [Local]");
    }

    // Lazy on-demand extraction — call ONLY off the real-time audio thread!
    std::optional<ModelPackAsset> loadCaptureBytes(int excelNo, bool wantIr) const {
        IndexEntry chosen;
        const void* resPtr = nullptr;
        size_t resSize = 0;
        {
            std::lock_guard<std::mutex> lock(mutex);
            const auto* ie = findEntryLocked(excelNo, wantIr);
            if (ie == nullptr) return std::nullopt;
            chosen = *ie;
            resPtr = embeddedPtr;
            resSize = embeddedSize;
        }

        ModelPackAsset out;
        out.excelNo = excelNo;
        out.isIr = chosen.isIr;
        out.fromEmbeddedBundle = chosen.fromEmbedded;
        out.displayName = chosen.displayName;
        out.originLabel = chosen.fromEmbedded ? "Bundled Pack" : "Local Pack";

        if (!chosen.fromEmbedded) {
            if (!chosen.diskFile.existsAsFile()) return std::nullopt;
            if (!chosen.diskFile.loadFileAsData(out.bytes) || out.bytes.getSize() == 0) {
                return std::nullopt;
            }
            return out;
        }

        if (resPtr == nullptr || resSize <= 22 || chosen.zipEntryIndex < 0) {
            return std::nullopt;
        }
        juce::MemoryInputStream mis(resPtr, resSize, false);
        juce::ZipFile zip(mis);
        std::unique_ptr<juce::InputStream> entryStream(zip.createStreamForEntry(chosen.zipEntryIndex));
        if (entryStream == nullptr) {
            return std::nullopt;
        }
        entryStream->readIntoMemoryBlock(out.bytes);
        if (out.bytes.getSize() == 0) {
            return std::nullopt;
        }
        return out;
    }

    ModelPackStats getStats() const {
        std::lock_guard<std::mutex> lock(mutex);
        ModelPackStats st;
        st.embeddedArchiveBytes = static_cast<juce::int64>(embeddedSize);
        for (int excelNo = 1; excelNo <= 265; ++excelNo) {
            const auto info = getExcelPackInfo(excelNo);
            if (!info.supportsPack) continue;
            const auto* ie = findEntryLocked(excelNo, info.isIr);
            if (ie == nullptr) continue;
            ++st.totalMapped;
            if (ie->fromEmbedded) ++st.bundledCount;
            else                  ++st.localOverrideCount;

            if (excelNo >= 59 && excelNo <= 129)       ++st.ampMapped;
            else if (excelNo >= 132 && excelNo <= 201) ++st.cabMapped;
            else if (excelNo >= 252 && excelNo <= 260) ++st.reverbMapped;
            else                                       ++st.pedalMapped;
        }
        return st;
    }

private:
    struct IndexEntry {
        int excelNo = 0;
        bool isIr = false;
        bool fromEmbedded = false;
        int zipEntryIndex = -1;
        juce::File diskFile;
        juce::String displayName;
    };

    ModelPackRegistry() {
        rescan();
    }

    static int makeKey(int excelNo, bool isIr) noexcept {
        return (excelNo << 1) | (isIr ? 1 : 0);
    }

    const IndexEntry* findEntryLocked(int excelNo, bool wantIr) const noexcept {
        if (auto it = indexMap.find(makeKey(excelNo, wantIr)); it != indexMap.end()) {
            return &it->second;
        }
        const int alias = getSharedAliasExcelNo(excelNo);
        if (alias > 0) {
            if (auto it = indexMap.find(makeKey(alias, wantIr)); it != indexMap.end()) {
                return &it->second;
            }
        }
        return nullptr;
    }

    static int parseExcelNoFromFilename(const juce::String& fileName, bool& outIsIr) {
        const auto lower = fileName.toLowerCase();
        if (lower.endsWith(".nam")) {
            outIsIr = false;
        } else if (lower.endsWith(".wav") || lower.endsWith(".aif") || lower.endsWith(".aiff")) {
            outIsIr = true;
        } else {
            return 0;
        }

        const auto stem = fileName.upToLastOccurrenceOf(".", false, false);
        int digits = 0;
        while (digits < stem.length() && juce::CharacterFunctions::isDigit(stem[digits])) {
            ++digits;
        }
        if (digits <= 0 || digits > 3) return 0;
        if (digits < stem.length() && juce::CharacterFunctions::isDigit(stem[digits])) return 0;

        const int num = stem.substring(0, digits).getIntValue();
        return (num >= 1 && num <= 265) ? num : 0;
    }

    static juce::String cleanDisplayName(const juce::String& fileName) {
        auto stem = fileName.upToLastOccurrenceOf(".", false, false);
        int i = 0;
        while (i < stem.length() && juce::CharacterFunctions::isDigit(stem[i])) ++i;
        while (i < stem.length() && (stem[i] == '_' || stem[i] == '-' || stem[i] == ' ')) ++i;
        auto cleaned = stem.substring(i).replaceCharacter('_', ' ').trim();
        return cleaned.isNotEmpty() ? cleaned : stem;
    }

    void scanDiskDirectoryLocked(const juce::File& dir, bool /*isUserOverride*/) {
        if (!dir.isDirectory()) return;
        juce::Array<juce::File> files;
        dir.findChildFiles(files, juce::File::findFiles, true, "*.nam;*.wav;*.aif;*.aiff");
        files.sort();
        for (const auto& f : files) {
            if (!f.existsAsFile() || f.getSize() <= 0) continue;
            bool isIr = false;
            const int excelNo = parseExcelNoFromFilename(f.getFileName(), isIr);
            if (excelNo <= 0) continue;

            IndexEntry ie;
            ie.excelNo = excelNo;
            ie.isIr = isIr;
            ie.fromEmbedded = false;
            ie.diskFile = f;
            ie.displayName = cleanDisplayName(f.getFileName());
            indexMap[makeKey(excelNo, isIr)] = std::move(ie);
        }
    }

    static void embeddedAnchorFn() noexcept {}

    void locateEmbeddedResourceLocked() {
#if defined(_WIN32)
        HMODULE hMod = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&embeddedAnchorFn),
                               &hMod) && hMod != nullptr) {
            if (HRSRC hRes = FindResourceW(hMod, L"PB_MODELPACKS", MAKEINTRESOURCEW(10))) {
                if (HGLOBAL hMem = LoadResource(hMod, hRes)) {
                    const DWORD sz = SizeofResource(hMod, hRes);
                    const void* ptr = LockResource(hMem);
                    if (ptr != nullptr && sz > 0) {
                        embeddedPtr = ptr;
                        embeddedSize = static_cast<size_t>(sz);
                    }
                }
            }
        }
#endif
    }

    mutable std::mutex mutex;
    std::unordered_map<int, IndexEntry> indexMap;
    const void* embeddedPtr = nullptr;
    size_t embeddedSize = 0;
    std::atomic<uint64_t> version { 1 };
};

} // namespace pedalboard
