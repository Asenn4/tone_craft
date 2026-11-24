# Steinberg ASIO SDK (Optional)

PedalBoard's CMake configuration automatically enables native **ASIO** (`JUCE_ASIO=1`)
when Steinberg's ASIO SDK headers are present in this directory.

## How to enable ASIO:
1. Download the free **ASIO SDK** from https://www.steinberg.net/developers/
2. Extract it so that `iasiodrv.h` is located at:
   `third_party/asiosdk/common/iasiodrv.h`
   (Alternatively, pass `-DASIO_SDK_DIR="C:/path/to/asiosdk"` to CMake.)
3. Re-run CMake configure & build:
   ```powershell
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release --parallel
   ```
4. Launch `PedalBoard.exe` -> Options -> Audio/MIDI Settings -> select **ASIO** (e.g., ASIO4ALL v2 or your audio interface driver).
