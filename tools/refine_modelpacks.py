#!/usr/bin/env python3
"""
Pass 2 Precision Refinement for PedalBoard GP-200 ModelPacks.

Reuses the cached OAuth session from tools/fetch_modelpacks.py (no re-login needed)
to:
  1. Fill all 5 missed entries (#159, #161, #162, #180, #187) to reach 193/193.
  2. Replace any wrong-gear or generic-fallback matches (using sort=best-match
     + strict title/make keyword scoring and negative keyword filters).
  3. Assign DISTINCT sub-captures across multi-channel amp siblings (CL/OD/LD,
     Normal/Bright, Vintage/Modern, Ch1/Ch2/Ch3/Ch4) so every GP-200 model has
     its own authentic tone!
  4. Replace suspicious WAVs (< 1 KB or > 500 KB cab IRs) with clean studio IRs.
"""

from __future__ import annotations

import hashlib
import json
import pathlib
import re
import sys
import time
import urllib.parse
import zipfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import fetch_modelpacks as fm  # noqa: E402


# Each rule: (excel_no, filename_stem, is_ir, gears, queries, must_any, ban_any, channel_prefs, avoid_same_as)
# - queries: ordered list of best-match search queries
# - must_any: tone title+makes+tags must contain at least one of these tokens (when non-empty)
# - ban_any: skip tone if title contains any of these tokens
# - channel_prefs: keywords preferred in the individual model filename inside the tone
# - avoid_same_as: list of excel_nos whose SHA-256 hash we should avoid duplicating if another model exists
RULES: list[dict] = [
    # --- PEDALS (exact authentic stompbox models) ---
    {"no": 4,   "name": "Micro_Boost",  "ir": False, "gears": "pedal", "q": ["MXR Micro Amp", "Micro Amp"], "must": ["micro", "mxr", "boost"], "ban": ["ce-2", "chorus"]},
    {"no": 5,   "name": "AC_Boost",     "ir": False, "gears": "pedal", "q": ["Xotic AC Booster", "AC Booster"], "must": ["ac boost", "xotic", "booster"], "ban": ["ep boost", "ce-2"]},
    {"no": 6,   "name": "B_Boost",      "ir": False, "gears": "pedal", "q": ["Xotic BB Preamp", "BB Preamp"], "must": ["bb", "preamp", "xotic"], "ban": ["acoustic", "ce-2"]},
    {"no": 8,   "name": "14_Boost",     "ir": False, "gears": "pedal", "q": ["Fortin 33", "TC Electronic Spark", "Fortin Grind"], "must": ["fortin", "spark", "33", "grind", "boost"], "ban": ["ce-2"]},
    {"no": 9,   "name": "FAT_BB",       "ir": False, "gears": "pedal", "q": ["Fulltone Fat Boost", "Xotic RC Booster", "RC Booster"], "must": ["rc", "fat", "boost", "xotic"], "ban": ["ep boost"], "avoid": [5, 6, 7]},
    {"no": 10,  "name": "Boost",        "ir": False, "gears": "pedal", "q": ["Klon Clean Boost", "Clean Boost", "Super Hard On"], "must": ["boost", "klon", "hard"], "ban": ["ep boost"], "avoid": [4, 5, 6, 7, 8, 9]},
    {"no": 12,  "name": "AC_Sim",       "ir": False, "gears": "pedal", "q": ["Boss AC-3", "Acoustic Simulator", "Boss AD-2"], "must": ["ac-3", "ac3", "acoustic", "ad-2"], "ban": ["ce-2", "chorus"]},
    {"no": 26,  "name": "Green_OD",     "ir": False, "gears": "pedal", "q": ["Ibanez TS808", "TS808", "Tube Screamer 808"], "must": ["808", "ts808"], "ban": ["ce-2"], "avoid": [27]},
    {"no": 28,  "name": "Yellow_OD",    "ir": False, "gears": "pedal", "q": ["Boss OD-1", "OD-1", "DOD 250"], "must": ["od-1", "od1", "250", "overdrive"], "ban": ["ce-2", "chorus", "sd-1"]},
    {"no": 32,  "name": "Scream_OD",    "ir": False, "gears": "pedal", "q": ["Fulltone Fulldrive", "Maxon OD808", "TS9DX"], "must": ["fulldrive", "maxon", "808", "ts9", "screamer"], "ban": ["ce-2"], "avoid": [26, 27]},
    {"no": 33,  "name": "Blues_OD",     "ir": False, "gears": "pedal", "q": ["Boss BD-2", "Blues Driver", "BD-2"], "must": ["bd-2", "bd2", "blues driver"], "ban": ["ce-2", "chorus"]},
    {"no": 36,  "name": "OD_JCM800",    "ir": False, "gears": "pedal", "q": ["JHS Angry Charlie", "Marshall Guv'nor", "MI Audio Crunch Box"], "must": ["angry", "charlie", "crunch box", "jcm", "marshall"], "ban": ["ce-2"]},
    {"no": 37,  "name": "TaiChi_OD",    "ir": False, "gears": "pedal", "q": ["Hermida Zendrive", "Zendrive", "Dumble Overdrive"], "must": ["zen", "dumble", "dumbl"], "ban": ["ce-2"]},
    {"no": 39,  "name": "Precise_OD",   "ir": False, "gears": "pedal", "q": ["Horizon Precision Drive", "Precision Drive", "Horizon Devices"], "must": ["precision", "horizon"], "ban": ["ce-2"]},
    {"no": 41,  "name": "Red_Haze",     "ir": False, "gears": "pedal", "q": ["Dunlop Fuzz Face", "Fuzz Face"], "must": ["fuzz face", "fuzzface", "ff"], "ban": ["ce-2"]},
    {"no": 43,  "name": "Plustortion",  "ir": False, "gears": "pedal", "q": ["MXR Distortion+", "MXR Distortion Plus", "Distortion+"], "must": ["distortion+", "distortion plus", "mxr", "dist+"], "ban": ["ds-1", "ce-2"]},
    {"no": 44,  "name": "SM_Dist",      "ir": False, "gears": "pedal", "q": ["Boss HM-2", "HM-2", "Boss Metal Zone", "MT-2"], "must": ["hm-2", "hm2", "mt-2", "metal"], "ban": ["ce-2", "chorus"]},
    {"no": 48,  "name": "La_Charger",   "ir": False, "gears": "pedal", "q": ["Friedman BE-OD", "BE-OD", "MI Audio Crunch Box"], "must": ["be-od", "beod", "friedman", "crunch"], "ban": ["ce-2"]},
    {"no": 49,  "name": "Flex_OD",      "ir": False, "gears": "pedal", "q": ["Xotic SL Drive", "Boss OD-3", "Wampler Plexi"], "must": ["sl drive", "od-3", "od3", "plexi"], "ban": ["ocd", "ce-2"], "avoid": [34]},
    {"no": 50,  "name": "Bass_OD",      "ir": False, "gears": "pedal", "q": ["Boss ODB-3", "Bass Overdrive", "SansAmp Bass Driver"], "must": ["odb", "bass", "sansamp"], "ban": ["ds-1", "ce-2"]},
    {"no": 51,  "name": "Black_Bass",   "ir": False, "gears": "pedal", "q": ["Darkglass B7K", "B7K", "Darkglass B3K"], "must": ["b7k", "b3k", "darkglass"], "ban": ["ce-2"]},

    # --- AMPS (exact models + distinct multi-channel captures) ---
    {"no": 59,  "name": "Tweedy",       "ir": False, "gears": "amp", "q": ["Fender Tweed Deluxe", "Fender 5E3", "Fender Tweed Champ"], "must": ["tweed", "5e3", "champ", "5f1"], "ban": ["twin"]},
    {"no": 60,  "name": "Bellman_59N",  "ir": False, "gears": "amp", "q": ["Fender Bassman 59", "Fender 59 Bassman", "Fender Bassman"], "must": ["bassman"], "prefs": ["normal", "norm", "clean", "low", "ch1"]},
    {"no": 61,  "name": "Bellman_59B",  "ir": False, "gears": "amp", "q": ["Fender Bassman 59", "Fender 59 Bassman", "Fender Bassman"], "must": ["bassman"], "prefs": ["bright", "brt", "drive", "crunch", "jumped", "high"], "avoid": [60]},
    {"no": 62,  "name": "Dark_Twin",    "ir": False, "gears": "amp", "q": ["Fender Twin Reverb 65", "Blackface Twin Reverb", "Fender Twin Reverb"], "must": ["twin"], "prefs": ["clean", "normal", "vib", "blackface"]},
    {"no": 63,  "name": "Dark_DLX",     "ir": False, "gears": "amp", "q": ["Fender Deluxe Reverb 65", "Fender Deluxe Reverb"], "must": ["deluxe"], "ban": ["twin"]},
    {"no": 64,  "name": "Dark_Vibra",   "ir": False, "gears": "amp", "q": ["Fender Vibrolux", "Fender Vibroverb", "Fender Super Reverb"], "must": ["vibro", "super reverb", "princeton"], "ban": ["twin"]},
    {"no": 65,  "name": "Silver_Twin",  "ir": False, "gears": "amp", "q": ["Silverface Twin Reverb", "1973 Fender Twin Reverb", "Fender Twin"], "must": ["twin"], "prefs": ["silver", "73", "bright", "drive", "vibrato"], "avoid": [62]},
    {"no": 66,  "name": "SUPDual_CL",   "ir": False, "gears": "amp", "q": ["Supro Dual Tone", "Supro Thunderbolt", "Supro"], "must": ["supro"], "prefs": ["clean", "cl", "low", "1"]},
    {"no": 67,  "name": "SUPDual_OD",   "ir": False, "gears": "amp", "q": ["Supro Dual Tone", "Supro Thunderbolt", "Supro"], "must": ["supro"], "prefs": ["drive", "od", "crunch", "boost", "gain", "high", "2"], "avoid": [66]},
    {"no": 68,  "name": "Foxy_15TB",    "ir": False, "gears": "amp", "q": ["VOX AC15", "AC15", "Vox AC-15"], "must": ["ac15", "ac-15", "ac 15"], "ban": ["ac30"]},
    {"no": 69,  "name": "Foxy_30N",     "ir": False, "gears": "amp", "q": ["VOX AC30", "Vox AC-30"], "must": ["ac30", "ac-30"], "prefs": ["normal", "norm", "clean", "ch1"]},
    {"no": 70,  "name": "Foxy_30TB",    "ir": False, "gears": "amp", "q": ["VOX AC30 Top Boost", "VOX AC30"], "must": ["ac30", "ac-30"], "prefs": ["top boost", "tb", "topboost", "drive", "crunch", "brilliant"], "avoid": [69]},
    {"no": 72,  "name": "Match_CL",     "ir": False, "gears": "amp", "q": ["Matchless Chieftain", "Matchless DC30", "Matchless"], "must": ["matchless"], "prefs": ["clean", "cl", "ef86", "low", "1"]},
    {"no": 73,  "name": "Match_OD",     "ir": False, "gears": "amp", "q": ["Matchless Chieftain", "Matchless DC30", "Matchless"], "must": ["matchless"], "prefs": ["od", "drive", "crunch", "gain", "high", "2", "boost"], "avoid": [72]},
    {"no": 74,  "name": "L_Star_CL",    "ir": False, "gears": "amp", "q": ["Mesa Lonestar", "Mesa Boogie Lone Star"], "must": ["lone", "lonestar"], "prefs": ["clean", "cl", "ch1", "1"]},
    {"no": 75,  "name": "L_Star_OD",    "ir": False, "gears": "amp", "q": ["Mesa Lonestar", "Mesa Boogie Lone Star"], "must": ["lone", "lonestar"], "prefs": ["drive", "od", "ch2", "2", "gain", "lead"], "avoid": [74]},
    {"no": 76,  "name": "BogSV_CL",     "ir": False, "gears": "amp", "q": ["Bogner Shiva", "Shiva"], "must": ["shiva"], "prefs": ["clean", "cl", "ch1", "1"]},
    {"no": 77,  "name": "BogSV_OD",     "ir": False, "gears": "amp", "q": ["Bogner Shiva", "Shiva"], "must": ["shiva"], "prefs": ["drive", "od", "ch2", "boost", "gain", "lead", "2"], "avoid": [76]},
    {"no": 78,  "name": "Bog_BlueV",    "ir": False, "gears": "amp", "q": ["Bogner Ecstasy", "Bogner XTC"], "must": ["ecstasy", "xtc"], "prefs": ["blue", "vintage", "plexi", "ch2"]},
    {"no": 79,  "name": "Bog_BlueM",    "ir": False, "gears": "amp", "q": ["Bogner Ecstasy", "Bogner XTC"], "must": ["ecstasy", "xtc"], "prefs": ["blue", "modern", "boost", "b2", "ch2"], "avoid": [78]},
    {"no": 80,  "name": "Bog_RedV",     "ir": False, "gears": "amp", "q": ["Bogner Ecstasy", "Bogner XTC"], "must": ["ecstasy", "xtc"], "prefs": ["red", "vintage", "ch3", "lead"], "avoid": [78, 79]},
    {"no": 81,  "name": "Bog_RedM",     "ir": False, "gears": "amp", "q": ["Bogner Ecstasy", "Bogner XTC"], "must": ["ecstasy", "xtc"], "prefs": ["red", "modern", "boost", "high", "ch3"], "avoid": [78, 79, 80]},
    {"no": 82,  "name": "Z38_CL",       "ir": False, "gears": "amp", "q": ["Dr Z Maz 38", "Dr Z Maz", "Dr Z"], "must": ["dr", "maz", "z"], "prefs": ["clean", "cl", "low", "1"]},
    {"no": 83,  "name": "Z38_OD",       "ir": False, "gears": "amp", "q": ["Dr Z Maz 38", "Dr Z Maz", "Dr Z"], "must": ["dr", "maz", "z"], "prefs": ["drive", "od", "crunch", "gain", "high", "2"], "avoid": [82]},
    {"no": 84,  "name": "Knights_CL",   "ir": False, "gears": "amp", "q": ["Morgan AC20", "Two Rock", "Hiwatt DR103"], "must": ["morgan", "two rock", "hiwatt"], "prefs": ["clean", "cl", "low"]},
    {"no": 85,  "name": "Knights_CL_P", "ir": False, "gears": "amp", "q": ["Morgan AC20", "Two Rock", "Hiwatt DR103"], "must": ["morgan", "two rock", "hiwatt"], "prefs": ["push", "edge", "bright", "crunch", "mid"], "avoid": [84]},
    {"no": 86,  "name": "Knights_OD",   "ir": False, "gears": "amp", "q": ["Morgan", "Laney", "Hiwatt"], "must": ["morgan", "laney", "hiwatt"], "prefs": ["drive", "od", "lead", "gain", "high"], "avoid": [84, 85]},
    {"no": 87,  "name": "Bad_KT_CL",    "ir": False, "gears": "amp", "q": ["Bad Cat", "Bad Cat Lynx", "Bad Cat Hot Cat"], "must": ["bad cat", "badcat"], "prefs": ["clean", "cl", "ch1", "low"]},
    {"no": 88,  "name": "Bad_KT_OD",    "ir": False, "gears": "amp", "q": ["Bad Cat", "Bad Cat Lynx", "Bad Cat Hot Cat"], "must": ["bad cat", "badcat"], "prefs": ["drive", "od", "gain", "ch2", "high", "lead"], "avoid": [87]},
    {"no": 89,  "name": "Solo100_CL",   "ir": False, "gears": "amp", "q": ["Soldano SLO 100", "Soldano SLO"], "must": ["soldano", "slo"], "prefs": ["clean", "cl", "norm", "ch1"]},
    {"no": 90,  "name": "Solo100_OD",   "ir": False, "gears": "amp", "q": ["Soldano SLO 100", "Soldano SLO"], "must": ["soldano", "slo"], "prefs": ["crunch", "od", "rhythm", "mid"], "avoid": [89]},
    {"no": 91,  "name": "Solo100_LD",   "ir": False, "gears": "amp", "q": ["Soldano SLO 100", "Soldano SLO"], "must": ["soldano", "slo"], "prefs": ["overdrive", "lead", "ld", "high", "gain", "ch2"], "avoid": [89, 90]},
    {"no": 92,  "name": "UK_45",        "ir": False, "gears": "amp", "q": ["Marshall JTM45", "JTM45", "JTM 45"], "must": ["jtm45", "jtm 45", "jtm"], "ban": ["jcm800", "jcm 800"], "prefs": ["normal", "clean", "low", "1"]},
    {"no": 93,  "name": "UK_45_P",      "ir": False, "gears": "amp", "q": ["Marshall JTM45", "JTM45", "JTM 45"], "must": ["jtm45", "jtm 45", "jtm"], "ban": ["jcm800", "jcm 800"], "prefs": ["bright", "high", "crunch", "drive", "2"], "avoid": [92]},
    {"no": 94,  "name": "UK_45JP",      "ir": False, "gears": "amp", "q": ["Marshall JTM45", "JTM45", "JTM 45"], "must": ["jtm45", "jtm 45", "jtm"], "ban": ["jcm800", "jcm 800"], "prefs": ["jump", "jumped", "patch", "both", "cranked", "full", "3"], "avoid": [92, 93]},
    {"no": 95,  "name": "UK_50",        "ir": False, "gears": "amp", "q": ["Marshall JMP 50", "Marshall 1987", "Marshall JMP"], "must": ["jmp", "1987", "plexi"], "ban": ["jcm800", "jcm 800"], "prefs": ["normal", "clean", "low", "1"]},
    {"no": 96,  "name": "UK_50_P",      "ir": False, "gears": "amp", "q": ["Marshall JMP 50", "Marshall 1987", "Marshall JMP"], "must": ["jmp", "1987", "plexi"], "ban": ["jcm800", "jcm 800"], "prefs": ["bright", "crunch", "high", "drive", "2"], "avoid": [95]},
    {"no": 97,  "name": "UK_50JP",      "ir": False, "gears": "amp", "q": ["Marshall JMP 50", "Marshall 1987", "Marshall JMP"], "must": ["jmp", "1987", "plexi"], "ban": ["jcm800", "jcm 800"], "prefs": ["jump", "jumped", "cranked", "boost", "lead", "3"], "avoid": [95, 96]},
    {"no": 98,  "name": "UK_SLP",       "ir": False, "gears": "amp", "q": ["Marshall 1959 SLP", "Marshall 1959", "Super Lead Plexi"], "must": ["1959", "slp", "super lead", "plexi"], "ban": ["jcm800", "jcm 800"], "avoid": [95, 96, 97]},
    {"no": 100, "name": "UK_900",       "ir": False, "gears": "amp", "q": ["Marshall JCM900", "JCM 900", "JCM900"], "must": ["900", "jcm900"], "ban": ["jcm800", "jcm 800"]},
    {"no": 101, "name": "Flagman_1",    "ir": False, "gears": "amp", "q": ["Friedman BE-100", "Friedman BE100"], "must": ["friedman", "be-100", "be100"], "prefs": ["clean", "low", "g1", "1", "be"]},
    {"no": 102, "name": "Flagman_2",    "ir": False, "gears": "amp", "q": ["Friedman BE-100", "Friedman BE100"], "must": ["friedman", "be-100", "be100"], "prefs": ["be", "crunch", "mid", "2", "4"], "avoid": [101]},
    {"no": 103, "name": "Flagman_P1",   "ir": False, "gears": "amp", "q": ["Friedman BE-100", "Friedman BE100"], "must": ["friedman", "be-100", "be100"], "prefs": ["hbe", "sat", "high", "6", "7"], "avoid": [101, 102]},
    {"no": 104, "name": "Flagman_P2",   "ir": False, "gears": "amp", "q": ["Friedman BE-100", "Friedman BE100"], "must": ["friedman", "be-100", "be100"], "prefs": ["hbe", "boost", "lead", "8", "9", "10", "max"], "avoid": [101, 102, 103]},
    {"no": 105, "name": "Mess2C_1",     "ir": False, "gears": "amp", "q": ["Mesa Boogie Mark IIC+", "Mark IIC+"], "must": ["iic", "mark ii"], "prefs": ["clean", "rhythm", "low", "1"]},
    {"no": 106, "name": "Mess2C_2",     "ir": False, "gears": "amp", "q": ["Mesa Boogie Mark IIC+", "Mark IIC+"], "must": ["iic", "mark ii"], "prefs": ["crunch", "mid", "lead", "2"], "avoid": [105]},
    {"no": 107, "name": "Mess2C_3",     "ir": False, "gears": "amp", "q": ["Mesa Boogie Mark IIC+", "Mark IIC+"], "must": ["iic", "mark ii"], "prefs": ["lead", "high", "eq", "v", "scoop", "3", "boost"], "avoid": [105, 106]},
    {"no": 108, "name": "Mess4_LD",     "ir": False, "gears": "amp", "q": ["Mesa Mark IV", "Mesa Boogie Mark IV", "Mark IV"], "must": ["mark iv", "mark 4", "mkiv", "mk4"], "prefs": ["rhythm", "crunch", "1", "mid"]},
    {"no": 109, "name": "Mess4_LD_2",   "ir": False, "gears": "amp", "q": ["Mesa Mark IV", "Mesa Boogie Mark IV", "Mark IV"], "must": ["mark iv", "mark 4", "mkiv", "mk4"], "prefs": ["lead", "high", "2", "eq"], "avoid": [108]},
    {"no": 110, "name": "Mess4_LD_3",   "ir": False, "gears": "amp", "q": ["Mesa Mark IV", "Mesa Boogie Mark IV", "Mark IV"], "must": ["mark iv", "mark 4", "mkiv", "mk4"], "prefs": ["lead", "boost", "scoop", "3", "max", "metal"], "avoid": [108, 109]},
    {"no": 111, "name": "Mess_DualV",   "ir": False, "gears": "amp", "q": ["Mesa Dual Rectifier", "Dual Rectifier"], "must": ["rectifier", "recto"], "prefs": ["vintage", "orange", "ch2", "raw"]},
    {"no": 112, "name": "Mess_DualM",   "ir": False, "gears": "amp", "q": ["Mesa Dual Rectifier", "Dual Rectifier"], "must": ["rectifier", "recto"], "prefs": ["modern", "red", "ch3", "boost", "high"], "avoid": [111]},
    {"no": 113, "name": "Juice30_OD",   "ir": False, "gears": "amp", "q": ["Orange AD30", "Orange OR15", "Orange TH30"], "must": ["ad30", "orange"], "ban": ["rockerverb"]},
    {"no": 116, "name": "Eagle_120",    "ir": False, "gears": "amp", "q": ["ENGL Savage 120", "ENGL Savage"], "must": ["savage"], "prefs": ["ch3", "crunch", "rhythm", "1", "2"]},
    {"no": 117, "name": "Eagle_120_P",  "ir": False, "gears": "amp", "q": ["ENGL Savage 120", "ENGL Savage"], "must": ["savage"], "prefs": ["ch4", "lead", "boost", "high", "3", "4"], "avoid": [116]},
    {"no": 118, "name": "Power_LD",     "ir": False, "gears": "amp", "q": ["ENGL Powerball", "Powerball"], "must": ["powerball"], "ban": ["fireball"]},
    {"no": 119, "name": "Dizz_VH",      "ir": False, "gears": "amp", "q": ["Diezel VH4", "VH4"], "must": ["vh4", "diezel"], "ban": ["synergy"], "prefs": ["ch2", "crunch", "2", "1"]},
    {"no": 120, "name": "Dizz_VH_S",    "ir": False, "gears": "amp", "q": ["Diezel VH4", "VH4"], "must": ["vh4", "diezel"], "ban": ["synergy"], "prefs": ["ch3", "3", "rhythm", "mid"], "avoid": [119]},
    {"no": 121, "name": "Dizz_VH_P",    "ir": False, "gears": "amp", "q": ["Diezel VH4", "VH4"], "must": ["vh4", "diezel"], "ban": ["synergy"], "prefs": ["ch3", "boost", "high", "metal"], "avoid": [119, 120]},
    {"no": 122, "name": "Dizz_VH_PS",   "ir": False, "gears": "amp", "q": ["Diezel VH4", "VH4"], "must": ["vh4", "diezel"], "ban": ["synergy"], "prefs": ["ch4", "4", "lead", "max"], "avoid": [119, 120, 121]},
    {"no": 124, "name": "Foxy_Bass",    "ir": False, "gears": "amp", "q": ["Vox Bass", "Hiwatt Bass", "Orange AD200", "Sunn Bass"], "must": ["bass", "ad200", "sunn", "ac100"], "ban": ["ac30"]},
    {"no": 125, "name": "Mess_Bass",    "ir": False, "gears": "amp", "q": ["Mesa Bass 400", "Mesa M9", "Mesa Subway"], "must": ["mesa", "400", "subway", "bass"]},
    {"no": 127, "name": "Bass_Pre",     "ir": False, "gears": "amp", "q": ["Alembic F-2B", "Aguilar Tone Hammer", "SansAmp Bass", "Bass Preamp"], "must": ["alembic", "aguilar", "preamp", "sansamp"], "ban": ["bassman"]},
    {"no": 129, "name": "AC_Pre_2",     "ir": False, "gears": "amp", "q": ["AER Compact", "Acoustic Preamp", "LR Baggs"], "must": ["aer", "acoustic", "baggs", "fishman"], "avoid": [128]},

    # --- CABS (fill all 5 misses + fix tiny/huge/mismatched cab IRs) ---
    {"no": 136, "name": "TWD_LUX",      "ir": True,  "gears": "cab", "q": ["Fender Tweed Deluxe IR", "Fender 5E3 IR", "Tweed 1x12", "Jensen P12"], "must": ["tweed", "5e3", "deluxe", "jensen", "1x12"]},
    {"no": 159, "name": "ACE",          "ir": True,  "gears": "cab", "q": ["Celestion Greenback 1x12", "Vox 1x12", "1x12 Greenback", "Morgan"], "must": ["1x12", "greenback", "vox", "morgan", "celestion"]},
    {"no": 161, "name": "D_STAR",       "ir": True,  "gears": "cab", "q": ["Mesa 1x12", "Black Shadow C90", "Mesa Thiele", "Mesa Boogie 1x12"], "must": ["mesa", "1x12", "c90", "thiele", "shadow"]},
    {"no": 162, "name": "SUP_Star",     "ir": True,  "gears": "cab", "q": ["Mesa 2x12", "Rectifier 2x12", "Mesa Horizontal 2x12"], "must": ["mesa", "2x12"], "avoid": [161]},
    {"no": 167, "name": "Freed",        "ir": True,  "gears": "cab", "q": ["Friedman 4x12", "Friedman 2x12", "Friedman Cab", "VHT 4x12"], "must": ["friedman", "vht", "greenback", "v30"]},
    {"no": 173, "name": "Uban",         "ir": True,  "gears": "cab", "q": ["Bogner Uberkab", "Bogner 4x12", "Uberkab"], "must": ["uberkab", "bogner"], "ban": ["a7x", "nightmare"]},
    {"no": 179, "name": "Dizz",         "ir": True,  "gears": "cab", "q": ["Diezel 4x12", "Diezel Cab", "Diezel V30"], "must": ["diezel", "k100", "v30"]},
    {"no": 180, "name": "TRP",          "ir": True,  "gears": "cab", "q": ["Hughes Kettner 4x12", "Greenback 4x12", "Marshall 1960AX"], "must": ["hughes", "kettner", "greenback", "4x12", "1960"]},
    {"no": 181, "name": "King",         "ir": True,  "gears": "cab", "q": ["Mesa Road King", "Mesa Rectifier 4x12", "Mesa 4x12"], "must": ["mesa", "road king", "rectifier", "4x12"], "avoid": [160, 163]},
    {"no": 182, "name": "ADM_1",        "ir": True,  "gears": "cab", "q": ["Eden 1x15", "Bass 1x15", "Ampeg 1x15", "1x15 Bass"], "must": ["1x15", "15", "eden", "bass"]},
    {"no": 187, "name": "MATT",         "ir": True,  "gears": "cab", "q": ["Markbass", "4x10 Bass", "Bass 4x10", "Aguilar 4x10"], "must": ["markbass", "4x10", "bass", "aguilar", "ampeg"]},
    {"no": 189, "name": "AMPG_1",       "ir": True,  "gears": "cab", "q": ["Ampeg 4x10", "Ampeg SVT 410", "SVT-410"], "must": ["4x10", "410", "ampeg"]},

    # --- REVERB (#256 Spring -> real Fender Spring Reverb IR!) ---
    {"no": 256, "name": "Spring",       "ir": True,  "gears": "",    "q": ["Spring Reverb IR", "Fender Spring Reverb", "Spring Tank IR"], "must": ["spring"], "ban": ["shoegaze", "shimmer"]},
]


def sha256_file(p: pathlib.Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() else ""


def find_existing_for_no(out_dir: pathlib.Path, eno: int) -> list[pathlib.Path]:
    hits = []
    prefix = f"{eno:03d}_"
    for f in out_dir.rglob("*"):
        if f.is_file() and f.name.startswith(prefix) and f.suffix.lower() in {".nam", ".wav", ".aif", ".aiff"}:
            hits.append(f)
    return hits


def tone_text(tone: dict) -> str:
    parts = [str(tone.get("title") or ""), str(tone.get("description") or "")]
    for m in tone.get("makes") or []:
        if isinstance(m, dict):
            parts.append(str(m.get("name") or ""))
    for tg in tone.get("tags") or []:
        if isinstance(tg, dict):
            parts.append(str(tg.get("name") or ""))
    return " ".join(parts).lower()


def rank_models_for_rule(models: list[dict], want_ir: bool, prefs: list[str]) -> list[dict]:
    def ext_of(m: dict) -> str:
        u = urllib.parse.urlparse(m.get("model_url", "")).path.lower()
        for ext in (".wav", ".aiff", ".aif", ".nam"):
            if u.endswith(ext):
                return ext
        return ".nam"

    scored = []
    for m in models:
        ext = ext_of(m)
        is_ir = ext in (".wav", ".aif", ".aiff")
        if want_ir != is_ir:
            continue
        nm = (m.get("name") or "").lower()
        pref_hits = sum(3 for kw in prefs if kw in nm)
        # Mild preference for balanced / SM57 / mid / standard if no channel prefs
        std_bonus = 1 if ("standard" in (m.get("size") or "").lower() or "sm57" in nm) else 0
        scored.append((pref_hits, std_bonus, m.get("id", 0), m))
    scored.sort(key=lambda x: (x[0], x[1], -x[2]), reverse=True)
    return [x[3] for x in scored]


def main() -> int:
    out_dir = pathlib.Path(r"d:\Self Project\pedal_board\assets\modelpacks")
    session = fm.T3KSession("", fm.token_cache_path())
    if not session.ensure_authenticated(force_login=False):
        print("[ERROR] Cached OAuth session not available.", file=sys.stderr)
        return 1

    # Track hashes by excel_no so sibling channels get distinct captures
    hashes: dict[int, str] = {}
    for eno in range(1, 266):
        ex = find_existing_for_no(out_dir, eno)
        if ex:
            hashes[eno] = sha256_file(ex[0])

    # Cache models per tone_id to minimize redundant API calls!
    tone_models_cache: dict[int, list[dict]] = {}
    search_cache: dict[tuple, list[dict]] = {}

    upgraded = 0
    kept = 0
    failed = []

    for idx, rule in enumerate(RULES, 1):
        eno = rule["no"]
        want_ir = rule["ir"]
        gears = rule["gears"]
        must = [w.lower() for w in rule.get("must", [])]
        ban = [w.lower() for w in rule.get("ban", [])]
        prefs = [w.lower() for w in rule.get("prefs", [])]
        avoid_hashes = {hashes[a] for a in rule.get("avoid", []) if a in hashes}

        candidate_tones: list[dict] = []
        seen_tids: set[int] = set()

        for q in rule["q"]:
            for sort_mode in ("best-match", "downloads-all-time"):
                params = {
                    "query": q,
                    "sort": sort_mode,
                    "page": "1",
                    "page_size": "12",
                }
                if gears:
                    params["gears"] = gears
                if want_ir:
                    params["format"] = "ir"
                ckey = tuple(sorted(params.items()))
                if ckey not in search_cache:
                    st, js, _ = session.api_get("/api/v1/tones/search?" + urllib.parse.urlencode(params))
                    arr = (js.get("data", []) if isinstance(js, dict) else js) if (st == 200 and js) else []
                    search_cache[ckey] = arr if isinstance(arr, list) else []
                    time.sleep(0.25)
                for t in search_cache[ckey]:
                    tid = int(t.get("id", 0))
                    if tid <= 0 or tid in seen_tids:
                        continue
                    title_low = str(t.get("title") or "").lower()
                    full_low = tone_text(t)
                    if any(b in title_low for b in ban):
                        continue
                    if must and not any(m in full_low for m in must):
                        continue
                    seen_tids.add(tid)
                    # Score relevance: title match >> description match + log downloads
                    title_score = sum(5 for m in must if m in title_low)
                    full_score = sum(2 for m in must if m in full_low)
                    dl = min(int(t.get("downloads_count") or 0), 50000) / 10000.0
                    candidate_tones.append((title_score + full_score + dl, t))
                if len(candidate_tones) >= 6:
                    break
            if len(candidate_tones) >= 6:
                break

        candidate_tones.sort(key=lambda x: x[0], reverse=True)

        saved_ok = False
        for _, tone in candidate_tones[:6]:
            tid = int(tone["id"])
            if tid not in tone_models_cache:
                tone_models_cache[tid] = fm.query_models_for_tone(session, tid)
                time.sleep(0.2)
            models = rank_models_for_rule(tone_models_cache[tid], want_ir=want_ir, prefs=prefs)
            if not models:
                continue

            # Try up to 4 top-ranked models in this tone to get a valid & distinct capture
            for m in models[:6]:
                url = m.get("model_url", "")
                if not url:
                    continue
                d_st, blob, _ = session.download_url(url)
                if d_st != 200 or len(blob) < 1024:
                    continue
                if want_ir:
                    if blob[:4] != b"RIFF" or blob[8:12] != b"WAVE":
                        continue
                    # Reject > 500 KB cab IRs (keep reverbs unrestricted)
                    if 132 <= eno <= 201 and len(blob) > 500_000:
                        continue
                else:
                    try:
                        obj = json.loads(blob.decode("utf-8"))
                        if "version" not in obj or "weights" not in obj:
                            continue
                    except Exception:
                        continue

                h = hashlib.sha256(blob).hexdigest()
                if avoid_hashes and h in avoid_hashes and len(models) > 1:
                    continue

                # Remove any older file for this excel_no
                for old in find_existing_for_no(out_dir, eno):
                    old.unlink(missing_ok=True)

                ext = ".wav" if want_ir else ".nam"
                sub = out_dir / fm.target_subdir(eno)
                sub.mkdir(parents=True, exist_ok=True)
                out_path = sub / f"{eno:03d}_{rule['name']}{ext}"
                out_path.write_bytes(blob)
                hashes[eno] = h
                upgraded += 1
                saved_ok = True
                print(
                    f"[{idx:02d}/{len(RULES)}] UPGRADED #{eno:03d} {rule['name']:14s} -> "
                    f"{len(blob)/1024:6.1f} KB | tone='{tone.get('title','')[:34]}' | cap='{m.get('name','')[:28]}'"
                )
                break
            if saved_ok:
                break

        if not saved_ok:
            # Fallback for #187 Markbass: use Shift Line TC Electronic BC410 Bass Cab if still missing
            if eno == 187 and not find_existing_for_no(out_dir, 187):
                sl_zip = pathlib.Path(r"C:\Users\LENOVO\.gemini\antigravity\brain\f6771e88-42fc-4294-a95c-9a3b09f1fae1\scratch\public_irs\ShiftLineBass.zip")
                if sl_zip.is_file():
                    with zipfile.ZipFile(sl_zip, "r") as zf:
                        blob = zf.read("03_TC_Electronic_BC410_by_Shift_Line.wav")
                        out_path = out_dir / "cabs" / "187_MATT_TC_BC410.wav"
                        out_path.write_bytes(blob)
                        hashes[187] = hashlib.sha256(blob).hexdigest()
                        upgraded += 1
                        saved_ok = True
                        print(f"[{idx:02d}/{len(RULES)}] FALLBACK #{eno:03d} MATT -> Shift Line TC Electronic BC410 ({len(blob)/1024:.1f} KB)")
            if not saved_ok:
                if find_existing_for_no(out_dir, eno):
                    kept += 1
                    print(f"[{idx:02d}/{len(RULES)}] KEPT     #{eno:03d} {rule['name']} (existing valid capture preserved)")
                else:
                    failed.append((eno, rule["name"]))
                    print(f"[{idx:02d}/{len(RULES)}] MISSING  #{eno:03d} {rule['name']}")

    print(f"\n[REFINE DONE] upgraded={upgraded}, kept={kept}, still_missing={len(failed)} {failed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
