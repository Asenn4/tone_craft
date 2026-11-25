PedalBoard Bundled ModelPacks Directory
=======================================

Place your downloaded .nam (Neural Amp Modeler) and .wav/.aiff (Cabinet / Acoustic / Reverb IR)
files in this folder (or in subfolders like Amps/, Cabs/, Pedals/, Reverbs/).

When you build PedalBoard with CMake, every matching file here is automatically compressed into
a single ModelPacks.zip (~20-30 MB for all 193 models) and embedded directly inside BOTH:
  - PedalBoard.exe (Standalone)
  - PedalBoard.vst3 (DAW Plugin)

At runtime, PedalBoard uses LAZY LOADING: only the 1-3 models active in your current preset are
decompressed into RAM (~1-2 MB). Inactive models stay compressed and use zero DSP RAM!

FILE NAMING CONVENTION (3-digit Excel No from Daftar_Effect_GP-200_Checklist_Gratis.xlsx):
  - PRE Boosts / Acoustic / Tape (#4..#12, #20):
      004_MXR_Micro_Amp.nam
      005_Xotic_AC_Booster.nam
      011_Acoustic_Refiner.wav
      012_Boss_AC3.nam
      020_Strymon_Deco_Tape.nam
      (Note: #4..#10 automatically share captures with DST Boosts #52..#58!)
  - DST Overdrive / Fuzz / Distortion / Bass Drive (#26..#58):
      026_Ibanez_TS808.nam
      029_Klon_Centaur.nam
      045_ProCo_Rat.nam
      051_Darkglass_B7K.nam
  - AMP Heads (#59..#129):
      062_Fender_65_Twin.nam
      098_Marshall_1959HW.nam
      115_Peavey_5150.nam
      123_Ampeg_SVT.nam
  - CAB & Acoustic IRs (#132..#201):
      139_Fender_65_Twin_2x12.wav
      146_Marshall_1960AV.wav
      160_Mesa_Rectifier_4x12.wav
      192_Acoustic_Dreadnought.wav
  - RVB Space / Plate / Spring IRs (#252..#260):
      252_Room.wav
      255_EMT140_Plate.wav
      256_Fender_Spring.wav

You can also drop files at runtime into %APPDATA%\PedalBoard\ModelPacks\ and click
"RESCAN PACKS" in Studio Settings -> Storage & Cloud to test or override any model without rebuilding!
