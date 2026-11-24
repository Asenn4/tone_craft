#pragma once

// Auto-generated from Daftar_Effect_GP-200.xlsx (Valeton GP-200 Firmware V1.3.0 Complete Effect List)
// Contains all 265 models across NR, PRE, WAH, DST, AMP (+ Neural NAM), CAB, EQ, MOD, DLY, RVB.

#include "ModuleId.h"
#include <array>
#include <cstdint>
#include <juce_gui_basics/juce_gui_basics.h>
#include <span>

namespace pedalboard::gp200 {

struct ModelEntry {
    int excelNo;
    const char* name;
    const char* subType;
    const char* description;
    const char* paramsHint;
    uint32_t chassisArgb;
};

inline constexpr std::array<ModelEntry, 2> kNrModels = {{
    { 130, "Gate 1", "Gate", "ISP Decimator noise gate", "Threshold", 0xffe6e9efu },
    { 131, "Gate 2", "Gate", "Noise gate fleksibel", "Threshold, Attack, Release", 0xffe6e9efu },
}};

inline constexpr std::array<ModelEntry, 20> kPreModels = {{
    {   1, "COMP", "Comp", "Ross Compressor", "Sustain, Volume", 0xff1e78ffu },
    {   2, "COMP4", "Comp", "Keeley C4 4-knob compressor", "Sustain, Attack, Volume, Clipping", 0xff1e78ffu },
    {   3, "S-Comp", "Comp", "Kompresor fleksibel, fully adjustable", "Threshold, Ratio, Volume, Attack, Release, Tone, Blend", 0xff1e78ffu },
    {   4, "Micro Boost", "Boost", "MXR M133 Micro Amp (hingga 20dB)", "Gain", 0xfff59e0bu },
    {   5, "AC Boost", "Boost", "Xotic AC Booster", "-", 0xfff59e0bu },
    {   6, "B-Boost", "Boost", "Xotic BB Preamp (hingga 30dB)", "Gain, Volume, Bass/Treble", 0xfff59e0bu },
    {   7, "P-Boost", "Boost", "Xotic RC Booster (20dB, EQ ±15dB)", "Gain, Volume, Bass/Treble", 0xfff59e0bu },
    {   8, "14 Boost", "Boost", "Fortin Grind (hingga +20dB)", "Gain", 0xfff59e0bu },
    {   9, "FAT BB", "Boost", "Clean boost/preamp + low-cut switch", "Bass/Treble, Volume, Low Cut", 0xfff59e0bu },
    {  10, "Boost", "Boost", "Xotic EP Booster (+20dB)", "Gain, +3dB, Bright", 0xfff59e0bu },
    {  11, "AC Refiner", "Acoustic", "Untuk instrumen akustik, suara 'woody' natural", "Shape", 0xffd97706u },
    {  12, "AC Sim", "Acoustic", "Simulator gitar akustik (stompbox analog klasik)", "Body, Top, Volume, Mode (Standard/Jumbo/Enhanced/Piezo)", 0xffd97706u },
    {  13, "T-Wah", "Filter", "Touch wah / envelope filter (gitar & bass)", "Sens, Range, Q, Mix, Mode (Guitar/Bass)", 0xff10b981u },
    {  14, "A-WAH", "Auto Filter", "Auto wah dengan rate variabel (gitar & bass)", "Depth, Rate, Volume, Low, Q, High, Sync", 0xff10b981u },
    {  15, "Step Filter", "Filter", "Auto filter 4-step untuk suara synth-like", "Step 1-4, Rate, Sync", 0xff10b981u },
    {  16, "OCTA", "Pitch", "Efek oktaf polifonik", "Low Oct, High Oct, Dry", 0xff8b5cf6u },
    {  17, "Pitch", "Pitch", "Pitch shifter/harmonizer polifonik", "Low/Hi Pitch, Dry, Low/Hi Vol", 0xff8b5cf6u },
    {  18, "P-Bend", "Pitch", "Pitch shifter/harmonizer polifonik", "Low/Hi Pitch, Wet, Dry, Range", 0xff8b5cf6u },
    {  19, "Ring Mod", "Special", "Ring modulator (bell/chime inharmonik)", "Mix, Freq, Fine, Tone", 0xff3b82f6u },
    {  20, "Saturate", "Special", "Simulasi saturasi tape vintage", "Saturation, Mix, Volume, High Cut", 0xff3b82f6u },
}};

inline constexpr std::array<ModelEntry, 5> kWahModels = {{
    {  21, "V-Wah", "Wah", "VOX V846 wah", "Range, Q, Volume", 0xffd4af37u },
    {  22, "C-Wah", "Wah", "Dunlop CryBaby wah", "Range, Q, Volume", 0xfff5c518u },
    {  23, "P-Wah", "Wah", "Rack wah John Petrucci (Cry Baby)", "Range, Q, Volume, EQ", 0xff475569u },
    {  24, "S-Wah", "Wah", "Wah klasik era 60-an", "Range, Q, Volume", 0xfff5c518u },
    {  25, "B-Wah", "Wah", "Wah untuk bass", "-", 0xff0284c7u },
}};

inline constexpr std::array<ModelEntry, 34> kDstModels = {{
    {  26, "Green OD", "OD", "Ibanez TS-808 Tube Screamer", "Gain, Tone, Volume", 0xff16a34au },
    {  27, "OD 9", "OD", "Ibanez TS9 Tube Screamer", "Gain, Tone, Volume", 0xff16a34au },
    {  28, "Yellow OD", "OD", "Overdrive asimetris klasik", "Gain, Volume", 0xffeab308u },
    {  29, "Penesas OD", "OD", "Klon Centaur", "Gain, Tone, Volume", 0xffd6d3c4u },
    {  30, "Swarm OD", "OD", "Providence SOV-2 Stampede", "Gain, Tone, Volume", 0xffeab308u },
    {  31, "Super OD", "OD", "Overdrive asimetris hangat", "Gain, Tone, Volume", 0xffeab308u },
    {  32, "Scream OD", "OD", "Gaya Tube Screamer, karakter unik", "Gain, Tone, Volume, Fat, Air", 0xff16a34au },
    {  33, "Blues OD", "OD", "Overdrive hangat-natural hingga distorsi", "Gain, Tone, Volume", 0xff2563ebu },
    {  34, "Force OD", "OD", "Fulltone OCD", "Gain, Tone, Volume, Mode (HP/LP)", 0xffd6d3c4u },
    {  35, "Blues Master", "OD", "Marshall BluesBreaker", "Gain, Tone, Volume", 0xff2563ebu },
    {  36, "OD JCM800", "OD", "Tone gaya JCM800", "Gain, Master Volume, Bass/Middle/Treble", 0xff262930u },
    {  37, "TaiChi OD", "OD", "Hermida Zendrive", "Gain, Tone, Volume, Voice", 0xfff01e3cu },
    {  38, "Timmy OD", "OD", "Paul Cochrane Timmy", "Gain, Volume, Bass/Treble, Mode (I/II/III)", 0xff2563ebu },
    {  39, "Precise OD", "OD", "Horizon Devices Precision Drive", "Gain, Tone, Volume, Attack (1-6), Gate", 0xff2563ebu },
    {  40, "Lazaro Fuzz", "Fuzz", "Electro-Harmonix Big Muff Pi", "Sustain, Tone, Volume", 0xffdc2626u },
    {  41, "Red Haze", "Fuzz", "Dallas-Arbiter Fuzz Face", "Fuzz, Volume", 0xfff97316u },
    {  42, "Sora Fuzz", "Fuzz", "Sola Sound Tone Bender", "Fuzz, Volume", 0xffdc2626u },
    {  43, "Plustortion", "Fuzz", "MXR M104 Distortion+", "Gain, Volume", 0xfff97316u },
    {  44, "SM Dist", "Distortion", "Distortion 3-knob oranye klasik (70-80an)", "Gain, Tone, Volume", 0xfff97316u },
    {  45, "Darktale", "Distortion", "ProCo The Rat (versi LM308 awal)", "Gain, Filter, Volume", 0xff262930u },
    {  46, "Chief", "Distortion", "Marshall Guv'nor", "Gain, Volume, Bass/Middle/Treble", 0xff262930u },
    {  47, "Master Dist", "Distortion", "Marshall Shredmaster", "Gain, Volume, Bass/Contour/Treble", 0xfff01e3cu },
    {  48, "La Charger", "Distortion", "MI Audio Crunch Box", "Gain, Tone, Volume", 0xfff01e3cu },
    {  49, "Flex OD", "Bass Drive", "Distorsi sederhana untuk gitar & bass", "Gain, Tone, Volume, Mode (Normal/Scoop/Edge), Blend", 0xfff01e3cu },
    {  50, "Bass OD", "Bass Drive", "Overdrive khusus bass", "Gain, Blend, Volume, Bass/Treble", 0xffeab308u },
    {  51, "Black Bass", "Bass Preamp", "Darkglass Microtubes B7K", "Gain, Blend, Volume, Low/Lo-mid/Hi-mid/Treble, Attack (Cut/Boost/Flat)", 0xff262930u },
    {  52, "Micro Boost", "Boost", "MXR M133 Micro Amp2 (hingga 20dB)", "Gain", 0xff0ea5e9u },
    {  53, "AC Boost", "Boost", "Xotic AC Booster", "-", 0xffd6d3c4u },
    {  54, "B-Boost", "Boost", "Xotic BB Preamp (hingga 30dB)", "Gain, Volume, Bass/Treble", 0xff0ea5e9u },
    {  55, "P-Boost", "Boost", "Xotic RC Booster (20dB, EQ ±15dB)", "Gain, Volume, Bass/Treble", 0xffd6d3c4u },
    {  56, "14 Boost", "Boost", "Fortin Grind (hingga +20dB)", "Gain", 0xff262930u },
    {  57, "FAT BB", "Boost", "Clean boost/preamp + low-cut switch", "Bass/Treble, Volume, Low Cut", 0xff0ea5e9u },
    {  58, "Boost", "Boost", "Xotic EP Booster (+20dB)", "Gain, +3dB, Bright", 0xff0ea5e9u },
    { 998, "NAM Pedal", "Neural NAM", "Neural Amp Modeler Pedal (.nam Overdrive / Fuzz / Preamp)", "Input, Output, Tone, Mix", 0xff06b6d4u },
}};

inline constexpr std::array<ModelEntry, 72> kAmpModels = {{
    {  59, "Tweedy", "Clean", "Fender Tweed Deluxe", "Gain, Tone, Volume", 0xff38bdf8u },
    {  60, "Bellman 59N", "Clean", "Fender '59 Bassman (Normal)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xff38bdf8u },
    {  61, "Bellman 59B", "Drive", "Fender '59 Bassman (Bright)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    {  62, "Dark Twin", "Clean", "Fender '65 Twin Reverb", "Gain, Volume, Bass/Middle/Treble, Bright", 0xff38bdf8u },
    {  63, "Dark DLX", "Clean", "Fender Deluxe Reverb", "Gain, Volume, Bass/Treble", 0xff38bdf8u },
    {  64, "Dark Vibra", "Clean", "Fender Vibroverb 6G16 (1963)", "Gain, Volume, Bass/Treble, Bright", 0xff38bdf8u },
    {  65, "Silver Twin", "Clean", "Fender Silverface Twin Reverb", "Gain, Volume, Bass/Middle/Treble", 0xff38bdf8u },
    {  66, "SUPDual CL", "Clean", "Supro Dual-Tone 1624T (CH1 clean)", "Gain, Tone, Volume", 0xff38bdf8u },
    {  67, "SUPDual OD", "Drive", "Supro Dual-Tone 1624T (CH1+2 dirty)", "Gain 1/2, Tone 1/2, Volume", 0xfff59e0bu },
    {  68, "Foxy 15TB", "Clean", "VOX AC-100 bass amp", "Gain, Tone cut, Volume, Bass/Treble", 0xff38bdf8u },
    {  69, "Foxy 30N", "Clean", "VOX AC30HW (Normal)", "Gain, Tone cut, Volume, Bright", 0xff38bdf8u },
    {  70, "Foxy 30TB", "Drive", "VOX AC30HW (Normal)", "Gain, Tone cut, Volume, Bass/Treble, Char (Cool/Hot)", 0xfff59e0bu },
    {  71, "J-120 CL", "Clean", "Roland Jazz Chorus (solid state)", "Gain, Bright, Bass/Middle/Treble", 0xff38bdf8u },
    {  72, "Match CL", "Clean", "Matchless Chieftain 212 (clean)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xff38bdf8u },
    {  73, "Match OD", "Drive", "Matchless Chieftain 212 (overdrive)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    {  74, "L-Star CL", "Clean", "Mesa/Boogie Lone Star (CH1)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xff38bdf8u },
    {  75, "L-Star OD", "Drive", "Mesa/Boogie Lone Star (CH2)", "Input, Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    {  76, "BogSV CL", "Clean", "Bogner Shiva 20th Anniv. (Ch1)", "Gain, Presence, Volume, Bass/Treble, Bright", 0xff38bdf8u },
    {  77, "BogSV OD", "Drive", "Bogner Shiva 20th Anniv. (Ch2)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    {  78, "Bog BlueV", "Drive", "Bogner XTC (blue channel)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    {  79, "Bog BlueM", "Drive", "Bogner XTC (blue channel)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    {  80, "Bog RedV", "Hi Gain", "Bogner XTC (red channel)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    {  81, "Bog RedM", "Hi Gain", "Bogner XTC (red channel)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    {  82, "Z38 CL", "Clean", "Dr. Z Maz 38 Sr. (clean)", "Gain, Tone cut, Volume, Bass/Middle/Treble", 0xff38bdf8u },
    {  83, "Z38 OD", "Drive", "Dr. Z Maz 38 Sr.", "Gain, Tone cut, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    {  84, "Knights CL", "Clean", "Grindrod Pendragon PG20C (Normal, bright off)", "Gain, Volume, Bass/Middle/Treble", 0xff38bdf8u },
    {  85, "Knights CL+", "Clean", "Grindrod Pendragon PG20C (Normal, bright on)", "Gain, Volume, Bass/Middle/Treble", 0xff38bdf8u },
    {  86, "Knights OD", "Drive", "Grindrod Pendragon PG20C (Drive)", "Gain, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    {  87, "Bad-KT CL", "Clean", "Bad Cat Hot Cat 30 (clean)", "Gain, Presence, Volume", 0xff38bdf8u },
    {  88, "Bad-KT OD", "Drive", "Bad Cat Hot Cat 30 (overdrive)", "Gain, Presence, Volume, Edge, Bass/Treble", 0xfff59e0bu },
    {  89, "Solo100 CL", "Clean", "Soldano SLO100 (clean)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xff38bdf8u },
    {  90, "Solo100 OD", "Drive", "Soldano SLO100 (crunch)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    {  91, "Solo100 LD", "Hi Gain", "Soldano SLO100 (overdrive)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    {  92, "UK 45", "Drive", "Marshall JTM45 (Normal)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    {  93, "UK 45+", "Drive", "Marshall JTM45", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    {  94, "UK 45JP", "Drive", "Marshall JTM45", "Gain 1/2, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    {  95, "UK 50", "Drive", "Marshall JMP50 (Jump)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    {  96, "UK 50+", "Drive", "Marshall JMP50 (Jump)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    {  97, "UK 50JP", "Drive", "Marshall JMP50 (Jump)", "Gain 1/2, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    {  98, "UK SLP", "Drive", "Marshall 1959HW (Plexi reissue)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    {  99, "UK 800", "Drive", "Marshall JCM800", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    { 100, "UK 900", "Hi Gain", "Marshall JCM900", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    { 101, "Flagman 1", "Drive", "Boutique UK 'Brown Eye' (BE channel)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    { 102, "Flagman 2", "Drive", "Boutique UK 'Brown Eye' (BE channel)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    { 103, "Flagman+ 1", "Hi Gain", "Boutique UK 'Brown Eye' (HBE channel)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    { 104, "Flagman+ 2", "Hi Gain", "Boutique UK 'Brown Eye' (HBE channel)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    { 105, "Mess2C+ 1", "Drive", "Mesa/Boogie Mark II C+ (Lead)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    { 106, "Mess2C+ 2", "Drive", "Mesa/Boogie Mark II C+ (Lead)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    { 107, "Mess2C+ 3", "Drive", "Mesa/Boogie Mark II C+ (Lead)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    { 108, "Mess4 LD", "Hi Gain", "Mesa/Boogie Mark IV (Lead)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    { 109, "Mess4 LD 2", "Hi Gain", "Mesa/Boogie Mark IV (Lead 2)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    { 110, "Mess4 LD 3", "Hi Gain", "Mesa/Boogie Mark IV (Lead 3)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    { 111, "Mess DualV", "Hi Gain", "Mesa/Boogie Dual Rectifier (Vintage)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    { 112, "Mess DualM", "Hi Gain", "Mesa/Boogie Dual Rectifier (Modern)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    { 113, "Juice30 OD", "Drive", "Orange AD30 (Dirty)", "Gain, Volume, Bass/Middle/Treble", 0xfff59e0bu },
    { 114, "Juice R100", "Hi Gain", "Orange Rockerverb 100 (Dirty)", "Gain, Volume, Bass/Middle/Treble", 0xffef4444u },
    { 115, "EV 51", "Hi Gain", "Peavey 5150 (Lead)", "Gain, Volume, Bass/Middle/Treble, Presence", 0xffef4444u },
    { 116, "Eagle 120", "Hi Gain", "ENGL Savage 120", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    { 117, "Eagle 120+", "Hi Gain", "ENGL Savage 120", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    { 118, "Power LD", "Hi Gain", "ENGL Powerball II E645/2 (CH4)", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    { 119, "Dizz VH", "Hi Gain", "Diezel VH4", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    { 120, "Dizz VH S", "Hi Gain", "Diezel VH4", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    { 121, "Dizz VH+", "Hi Gain", "Diezel VH4", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    { 122, "Dizz VH+ S", "Hi Gain", "Diezel VH4", "Gain, Presence, Volume, Bass/Middle/Treble", 0xffef4444u },
    { 123, "Classic Bass", "Bass", "Ampeg SVT", "Gain, Midrange (220Hz/450Hz/800Hz/1.6kHz/3kHz), Bass/Middle/Treble, Volume", 0xff10b981u },
    { 124, "Foxy Bass", "Bass", "VOX AC-100 bass amp", "Volume, Bass/Treble", 0xff10b981u },
    { 125, "Mess Bass", "Bass", "Mesa/Boogie Bass 400", "Gain, Volume, Bass/Middle/Treble", 0xff10b981u },
    { 126, "Mini Bass", "Bass", "Ampeg B-15 'Flip Top'", "Volume, Bass/Treble", 0xff10b981u },
    { 127, "Bass Pre", "Bass", "Alembic F-2B preamp", "Volume, Bright, Bass/Middle/Treble", 0xff10b981u },
    { 128, "AC Pre", "Acoustic EQ", "AER Colourizer 2 acoustic preamp", "Volume, Tone, Balance, EQ Freq (90Hz-1.6kHz), EQ Q, EQ Gain", 0xffa855f7u },
    { 129, "AC Pre 2", "Acoustic EQ", "AER Colourizer 2 acoustic preamp", "Volume, Tone, Balance, EQ Freq (680Hz-11kHz), EQ Q, EQ Gain", 0xffa855f7u },
    { 999, "Neural NAM", "NAM Capture", "Neural Amp Modeler (.nam deep-learning capture)", "Drive, Bass, Middle, Treble, Presence, Master", 0xffa855f7u },
}};

inline constexpr std::array<ModelEntry, 71> kCabModels = {{
    { 132, "SUP ZEP", "Cabinet 1 x 6\"", "Supro 1x6 oval speaker", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 133, "TWD CP", "Cabinet 1 x 8\"", "Vintage Fender Champ", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 134, "TWD PRC", "Cabinet 1 x 10\"", "Vintage Fender Princeton", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 135, "TWD SUP", "Cabinet 2 x 10\"", "Custom Fender Tweed", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 136, "TWD LUX", "Cabinet 1 x 12\"", "Fender Tweed Deluxe", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 137, "Dark LUX", "Cabinet 1 x 12\"", "Vintage Fender Deluxe", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 138, "Dark VIT", "Cabinet 1 x 12\"", "Vintage Fender Vibrolux", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 139, "Dark Twin", "Cabinet 2 x 12\"", "Vintage Fender '65 Twin Reverb", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 140, "Dark CS", "Cabinet 2 x 12\"", "Custom modified Fender", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 141, "Bellman 1", "Cabinet 2 x 12\"", "Vintage Fender 'Piggyback' Bassman", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 142, "Bellman 2", "Cabinet 4 x 10\"", "Fender '59 Bassman", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 143, "J-120", "Cabinet 2 x 12\"", "Legendary 'Jazz Chorus'", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 144, "UK G12", "Cabinet 1 x 12\"", "Marshall 1x12", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 145, "UK GRN 1", "Cabinet 2 x 12\"", "Marshall 2550", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 146, "UK LD", "Cabinet 4 x 12\"", "Marshall 1960AV", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 147, "UK TD", "Cabinet 4 x 12\"", "'68 Marshall Basketweave", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 148, "UK MD", "Cabinet 4 x 12\"", "Custom modified Marshall", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 149, "UK GRN 2", "Cabinet 4 x 12\"", "Vintage Marshall, Celestion Greenback", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 150, "UK 75", "Cabinet 4 x 12\"", "Marshall, Celestion G12T-75", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 151, "UK Dark", "Cabinet 4 x 12\"", "1968 Marshall", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 152, "FOXY 1", "Cabinet 1 x 12\"", "Vintage VOX AC15", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 153, "FOXY 2", "Cabinet 2 x 12\"", "Vintage VOX AC30", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 154, "ROUT", "Cabinet 1 x 12\"", "Carr Rambler", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 155, "BogSV", "Cabinet 1 x 12\"", "Bogner Shiva", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 156, "Bad-KT", "Cabinet 1 x 12\"", "Bad Cat Hot Cat", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 157, "Match", "Cabinet 2 x 12\"", "Matchless Chieftain", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 158, "TOM OPEN", "Cabinet 1 x 12\"", "Swart Atomic Space", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 159, "ACE", "Cabinet 1 x 12\"", "Morgan AC-20 Deluxe", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 160, "Mess", "Cabinet 4 x 12\"", "Mesa/Boogie Rectifier", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 161, "D STAR", "Cabinet 1 x 12\"", "Mesa/Boogie Lonestar", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 162, "SUP Star", "Cabinet 2 x 12\"", "Mesa/Boogie Lonestar", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 163, "US STO", "Cabinet 1 x 12\"", "1980s Mesa/Boogie", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 164, "BOUTI", "Cabinet 2 x 12\"", "Custom 2x12 unik", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 165, "SUP", "Cabinet 2 x 12\"", "Supro 1624T", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 166, "MATT TWD", "Cabinet 2 x 12\"", "Matchless", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 167, "Freed", "Cabinet 2 x 12\"", "Fryette Deliverance", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 168, "DB Rock", "Cabinet 2 x 12\"", "Two-Rock", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 169, "Blue SK", "Cabinet 2 x 12\"", "Custom, Celestion Alnico Blue", "Volume, Low Cut, Hi Cut", 0xff38bdf8u },
    { 170, "EV", "Cabinet 4 x 12\"", "Peavey 6505", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 171, "Bog", "Cabinet 4 x 12\"", "Bogner", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 172, "Eagle", "Cabinet 4 x 12\"", "ENGL", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 173, "Uban", "Cabinet 4 x 12\"", "Bogner Uberkab", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 174, "Solo", "Cabinet 4 x 12\"", "Soldano", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 175, "Juice", "Cabinet 4 x 12\"", "Orange PPC412", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 176, "H-WAY", "Cabinet 4 x 12\"", "Vintage Hiwatt SE4123", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 177, "Way", "Cabinet 4 x 12\"", "Vintage WEM", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 178, "Dumb", "Cabinet 4 x 12\"", "Dumble", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 179, "Dizz", "Cabinet 4 x 12\"", "Diezel", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 180, "TRP", "Cabinet 4 x 12\"", "Hughes & Kettner Triamp", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 181, "King", "Cabinet 4 x 12\"", "Mesa/Boogie Road King", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
    { 182, "ADM 1", "Cabinet 1 x 15\" (bass)", "David Eden", "Volume, Low Cut, Hi Cut", 0xff10b981u },
    { 183, "ADM 2", "Cabinet 4 x 10\" (bass)", "David Eden", "Volume, Low Cut, Hi Cut", 0xff10b981u },
    { 184, "Workman 1", "Cabinet 1 x 15\" (bass)", "SWR", "Volume, Low Cut, Hi Cut", 0xff10b981u },
    { 185, "Workman 2", "Cabinet 4 x 10\" (bass)", "SWR Workingman's", "Volume, Low Cut, Hi Cut", 0xff10b981u },
    { 186, "US BASS", "Cabinet 2 x 10\" (bass)", "Mesa/Boogie", "Volume, Low Cut, Hi Cut", 0xff10b981u },
    { 187, "MATT", "Cabinet 2 x 10\" (bass)", "Mark Bass", "Volume, Low Cut, Hi Cut", 0xff10b981u },
    { 188, "F-TOP", "Cabinet 1 x 15\" (bass)", "Ampeg PF-115HE", "Volume, Low Cut, Hi Cut", 0xff10b981u },
    { 189, "AMPG 1", "Cabinet 4 x 10\" (bass)", "Ampeg SVT-410HE", "Volume, Low Cut, Hi Cut", 0xff10b981u },
    { 190, "AMPG 2", "Cabinet 8 x 10\" (bass)", "Ampeg SVT-810E", "Volume, Low Cut, Hi Cut", 0xff10b981u },
    { 191, "HACK", "Cabinet 4 x 12\" (bass)", "Hartke", "Volume, Low Cut, Hi Cut", 0xff10b981u },
    { 192, "AC", "Acoustic", "Dreadnought guitar simulation 1", "Volume, Low Cut, Hi Cut", 0xfff59e0bu },
    { 193, "AC Dream", "Acoustic", "Dreadnought guitar simulation 2", "Volume, Low Cut, Hi Cut", 0xfff59e0bu },
    { 194, "OM", "Acoustic", "Gitar akustik tipe OM", "Volume, Low Cut, Hi Cut", 0xfff59e0bu },
    { 195, "JUMBO", "Acoustic", "Gitar akustik jumbo", "Volume, Low Cut, Hi Cut", 0xfff59e0bu },
    { 196, "Bird", "Acoustic", "Gitar akustik 'H-Bird' ikonik", "Volume, Low Cut, Hi Cut", 0xfff59e0bu },
    { 197, "GA", "Acoustic", "Gitar akustik tipe GA", "Volume, Low Cut, Hi Cut", 0xfff59e0bu },
    { 198, "Classic AC", "Acoustic", "Gitar klasik", "Volume, Low Cut, Hi Cut", 0xfff59e0bu },
    { 199, "Mandolin", "Acoustic", "Mandolin", "Volume, Low Cut, Hi Cut", 0xfff59e0bu },
    { 200, "Fretless Bass", "Acoustic", "Bass akustik fretless", "Volume, Low Cut, Hi Cut", 0xfff59e0bu },
    { 201, "Double Bass", "Acoustic", "Double bass", "Volume, Low Cut, Hi Cut", 0xfff59e0bu },
    { 202, "User IR 1~20", "User IR", "File IR WAV buatan pengguna (44kHz/1024 sample)", "Volume, Low Cut, Hi Cut", 0xffe5e7ebu },
}};

inline constexpr std::array<ModelEntry, 6> kEqModels = {{
    { 203, "Guitar EQ 1", "EQ", "EQ 5-band gitar", "Band 1-5: 125Hz, 400Hz, 800Hz, 1.6kHz, 4kHz; Volume", 0xff17e2a5u },
    { 204, "Guitar EQ 2", "EQ", "EQ 5-band gitar", "Band 1-5: 100Hz, 500Hz, 1kHz, 3kHz, 6kHz; Volume", 0xff17e2a5u },
    { 205, "Bass EQ 1", "EQ", "EQ 5-band bass", "Band 1-5: 33Hz, 150Hz, 600Hz, 2kHz, 8kHz; Volume", 0xff0ea5e9u },
    { 206, "Bass EQ 2", "EQ", "EQ 5-band bass", "Band 1-5: 50Hz, 120Hz, 400Hz, 800Hz, 4.5kHz; Volume", 0xff0ea5e9u },
    { 207, "Mess EQ", "EQ", "5-band EQ ala Mesa/Boogie (V-shape klasik)", "Band 1-5: 80Hz, 240Hz, 750Hz, 2.2kHz, 6.6kHz", 0xff334155u },
    { 208, "Hyper EQ", "EQ", "10-band graphic EQ", "Band 1-10: 31Hz, 63Hz, 125Hz, 250Hz, 500Hz, 1kHz, 2kHz, 4kHz, 8kHz, 16kHz; Volume", 0xff17e2a5u },
}};

inline constexpr std::array<ModelEntry, 24> kModModels = {{
    { 209, "G-Chorus", "Chorus", "Chorus ensemble legendaris akhir 70-an (mode chorus)", "Depth, Rate, Volume, Sync", 0xff1ab5feu },
    { 210, "C-Chorus", "Chorus", "Stereo chorus 4-tombol ungu legendaris", "Mode (4 mode chorus)", 0xff1ab5feu },
    { 211, "B-Chorus", "Chorus", "Ensemble chorus untuk bassis", "-", 0xff1ab5feu },
    { 212, "M-Chorus", "Chorus", "Chorus multi-dimensi (cocok untuk stereo)", "Mix, Rate, Filter, Depth L/C/R, Sync", 0xff1ab5feu },
    { 213, "Jet", "Flanger", "Flanger klasik", "Depth, Rate, Pre Delay, Feedback, Sync", 0xff6366f1u },
    { 214, "B-Jet", "Flanger", "Flanger klasik untuk bass", "Depth, Rate, Pre Delay, Feedback, Sync", 0xff6366f1u },
    { 215, "N-Jet", "Flanger", "Flanger dengan negative feedback ('underwater')", "Depth, Rate, Pre Delay, Feedback, Sync", 0xff6366f1u },
    { 216, "Trem Jet", "Flanger", "Flanger + tremolo", "Flg Depth, Flg Rate, Feedback, Trm Depth, Trm Rate, Flg Sync, Trm Sync", 0xff6366f1u },
    { 217, "V-Roto", "Vibrato", "Blue vibrato pedal berbasis BBD", "Depth, Rate, Sync", 0xff3b82f6u },
    { 218, "G-Roto", "Vibrato", "Chorus ensemble legendaris (mode vibrato)", "Depth, Rate, Volume, Sync", 0xff3b82f6u },
    { 219, "Vibrato", "Vibrato", "Vibrato klasik, rentang luas", "Depth, Rate, Volume, Sync", 0xff3b82f6u },
    { 220, "O-Phase", "Phaser", "MXR M101 Phase 90", "Rate, Sync", 0xfff97316u },
    { 221, "G-Phase", "Phaser", "Green phase pedal berbasis BBD", "Depth, Rate, Sync", 0xfff97316u },
    { 222, "S-Phase", "Phaser", "Electro-Harmonix Small Stone", "Color (Warm/Sharp), Rate, Sync", 0xfff97316u },
    { 223, "Pan Phase", "Phaser", "Phaser halus + tremolo/pan", "Phs Depth, Phs Rate, Pan Depth, Pan Rate, Phs Sync, Pan Sync", 0xfff97316u },
    { 224, "M-Vibe", "Phaser", "Voodoo Lab Micro Vibe (Uni-Vibe 1968)", "Depth, Rate, Sync", 0xfff97316u },
    { 225, "Vibe", "Phaser", "Shin-Ei Uni-Vibe", "Depth, Rate, Volume, Mode (Chorus/Vibrato), Sync", 0xfff97316u },
    { 226, "O-Trem", "Tremolo", "Demeter TRM-1 Tremulator (opto tremolo)", "Depth, Rate, Sync", 0xffeab308u },
    { 227, "Sine Trem", "Tremolo", "Tremolo gelombang sine", "Depth, Rate, Volume, Sync", 0xffeab308u },
    { 228, "Triangle Trem", "Tremolo", "Tremolo gelombang triangle", "Depth, Rate, Volume, Sync", 0xffeab308u },
    { 229, "Bias Trem", "Tremolo", "Tremolo gelombang bias", "Depth, Rate, Volume, Sync, Bias", 0xffeab308u },
    { 230, "Detune", "Pitch", "Detune (sinyal sedikit bergeser + asli, mirip chorus)", "Detune (-50 s/d +50 cents), Dry/Wet", 0xff1ab5feu },
    { 231, "Bit Smash", "Special", "Bitcrusher / sample reducer", "Mix, Krush, Bit, Hi Cut, Lo Cut", 0xff1ab5feu },
    { 232, "Auto Swell", "Special", "Auto swell (suara seperti biola)", "Attack, Curve (Line/Exp/Log)", 0xff1ab5feu },
}};

inline constexpr std::array<ModelEntry, 19> kDlyModels = {{
    { 233, "BBD Delay S", "Delay", "Stereo analog delay berbasis chip BBD", "Mix, Feedback, Time, Time R%, Spread, Level, Sync, Trail", 0xffbe123cu },
    { 234, "Digital Delay S", "Delay", "Stereo digital delay, bersih & akurat", "Mix, Feedback, Time, Time R%, Spread, Level, Sync, Trail", 0xff3b82f6u },
    { 235, "Tape Delay S", "Delay", "Simulasi tape echo stereo", "Mix, Feedback, Time, Time R%, Spread, Wow & Flutter, Age, Scrape, Drive, Level, Sync, Trail", 0xff10b981u },
    { 236, "Ambience 1", "Delay", "Multi-tap delay (variasi 1)", "Mix, Feedback, Time, Level, Mod, Tone, Sync, Trail", 0xffe02f75u },
    { 237, "Ambience 2", "Delay", "Multi-tap delay (variasi 2)", "Mix, Feedback, Time, Level, Mod, Tone, Sync, Trail", 0xffe02f75u },
    { 238, "Pure Delay", "Delay", "Delay murni dan presisi", "Mix, Time, Feedback, Sync, Trail", 0xffe02f75u },
    { 239, "Analog Delay", "Delay", "Delay hangat dengan nuansa analog", "Mix, Time, Feedback, Sync, Trail", 0xffbe123cu },
    { 240, "Tape Delay", "Delay", "Simulasi tape echo solid-state", "Mix, Time, Feedback, Sync, Trail", 0xff10b981u },
    { 241, "Ping Pong", "Delay", "Ping-pong delay stereo kiri-kanan", "Mix, Time, Feedback, Sync, Trail", 0xffe02f75u },
    { 242, "Slapback", "Delay", "Efek slapback echo klasik", "Mix, Time, Feedback, Trail", 0xffe02f75u },
    { 243, "Sweep Echo", "Delay", "Delay dengan repeat termodulasi sweep filter", "Mix, Time, Feedback, Sweep Depth, Sweep Rate, Sweep Sync, Time Sync, Trail", 0xff10b981u },
    { 244, "Ring Echo", "Delay", "Delay dengan repeat ring-modulated", "Dly Mix, Time, Feedback, Ring Mix, Freq, Tone, Sync, Trail", 0xff10b981u },
    { 245, "Tube Delay", "Delay", "Simulasi tube-driven tape echo", "Mix, Time, Feedback, Sync, Trail", 0xffe02f75u },
    { 246, "M-Echo", "Delay", "Multi-tap delay, simulasi tape echo 4-head", "Mix, Time, Feedback, Tone, Mode (12 variasi head), Sync, Trail", 0xff10b981u },
    { 247, "Sweet Echo", "Delay", "Analog delay 1981-1984 (20-300ms)", "Mix, Time, Feedback, Sync, Trail", 0xff10b981u },
    { 248, "999 Echo", "Delay", "Maxon AD900 Analog Delay", "Mix, Time, Feedback, Sync, Trail", 0xff10b981u },
    { 249, "Vintage Rack", "Delay", "Rack delay 80-an, feedback sample-reduced", "Mix, Time, Feedback, Mod, Tone, Sync, Trail", 0xff3b82f6u },
    { 250, "Lofi Echo", "Delay", "Delay dengan repeat lo-fi", "Mix, Time, Feedback, Mod, Tone, Sync, Trail", 0xff10b981u },
    { 251, "Rev Echo", "Delay", "Delay dengan reversed feedback", "Mix, Time, Feedback, Level, Sync, Trail", 0xff10b981u },
}};

inline constexpr std::array<ModelEntry, 13> kRvbModels = {{
    { 252, "Room", "Reverb", "Simulasi ruangan", "Mix, Pre Delay, Decay, Trail", 0xfff69c1bu },
    { 253, "Hall", "Reverb", "Simulasi hall pertunjukan", "Mix, Pre Delay, Decay, Trail", 0xfff69c1bu },
    { 254, "Church", "Reverb", "Simulasi gereja", "Mix, Pre Delay, Decay, Trail", 0xfff69c1bu },
    { 255, "Plate", "Reverb", "Vintage plate reverberator", "Mix, Decay, High Damp, Trail", 0xffd97706u },
    { 256, "Spring", "Reverb", "Vintage spring reverberator", "Mix, Decay, Tone, Trail", 0xffd97706u },
    { 257, "Studio", "Reverb", "Simulasi studio rekaman", "Mix, Pre Delay, Decay, Low Damp, Hi Damp, Mod, Trail", 0xfff69c1bu },
    { 258, "Club", "Reverb", "Simulasi club", "Mix, Pre Delay, Decay, Low Damp, Hi Damp, Mod, Trail", 0xfff69c1bu },
    { 259, "Concert", "Reverb", "Simulasi concert hall", "Mix, Pre Delay, Decay, Low Damp, Hi Damp, Mod, Trail", 0xfff69c1bu },
    { 260, "Arena", "Reverb", "Simulasi arena live", "Mix, Pre Delay, Decay, Low Damp, Hi Damp, Mod, Trail", 0xfff69c1bu },
    { 261, "N-Star", "Reverb", "Reverb tuned, decay cerah & lush", "Mix, Decay, Trail", 0xff8b5cf6u },
    { 262, "Deepsea", "Reverb", "Reverb tuned, decay besar & dalam", "Mix, Decay, Trail", 0xff8b5cf6u },
    { 263, "Sweet Space", "Reverb", "Reverb termodulasi, lush & manis", "Mix, Pre Delay, Decay", 0xfff69c1bu },
    { 264, "Shimmer", "Reverb", "Reverb shimmering yang kaya", "Lo End, High End, Trail", 0xff8b5cf6u },
}};

inline std::span<const ModelEntry> modelsForModule(ModuleId id) noexcept {
    switch (id) {
        case ModuleId::NoiseGate:    return kNrModels;
        case ModuleId::Compressor:   return kPreModels;
        case ModuleId::Wah:          return kWahModels;
        case ModuleId::Overdrive:    return kDstModels;
        case ModuleId::AmpHead:      return kAmpModels;
        case ModuleId::CabinetIR:    return kCabModels;
        case ModuleId::GraphicEq:    return kEqModels;
        case ModuleId::StereoChorus: return kModModels;
        case ModuleId::StereoDelay:  return kDlyModels;
        case ModuleId::Reverb:       return kRvbModels;
        default:                     return kDstModels;
    }
}

inline const ModelEntry& getModelEntry(ModuleId id, int index) noexcept {
    const auto list = modelsForModule(id);
    if (list.empty()) return kDstModels[0];
    const int clamped = (index < 0) ? 0 : ((index >= static_cast<int>(list.size())) ? static_cast<int>(list.size()) - 1 : index);
    return list[static_cast<size_t>(clamped)];
}

inline juce::StringArray getModelNameArray(ModuleId id) {
    juce::StringArray out;
    for (const auto& m : modelsForModule(id)) {
        out.add(juce::String::fromUTF8(m.name));
    }
    return out;
}

} // namespace pedalboard::gp200
