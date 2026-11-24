cmake_minimum_required(VERSION 3.22)

project(PedalBoard VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.9
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# ------------------------------------------------------------------------------
# Neural Amp Modeler Core (MIT) + Eigen 3.4.1
# ------------------------------------------------------------------------------
set(NAM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NeuralAmpModelerCore")
set(EIGEN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/eigen")

if(EXISTS "${NAM_ROOT}/NAM/dsp.cpp" AND EXISTS "${EIGEN_ROOT}/Eigen/Dense")
    file(GLOB NAM_CORE_SOURCES
        "${NAM_ROOT}/NAM/*.cpp"
        "${NAM_ROOT}/NAM/*.c"
        "${NAM_ROOT}/NAM/*/*.cpp"
    )
    add_library(nam_core STATIC ${NAM_CORE_SOURCES})
    target_compile_features(nam_core PUBLIC cxx_std_20)
    target_include_directories(nam_core
        SYSTEM PUBLIC
            "${NAM_ROOT}"
            "${NAM_ROOT}/Dependencies/nlohmann"
            "${EIGEN_ROOT}"
    )
    target_compile_definitions(nam_core
        PUBLIC
            NAM_ENABLE_A2_FAST
            PEDALBOARD_HAS_NAM=1
        PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
    )
    if(MSVC)
        target_compile_options(nam_core PRIVATE /W0 "$<$<CONFIG:RELEASE>:/O2>")
    else()
        target_compile_options(nam_core PRIVATE -w "$<$<CONFIG:RELEASE>:-O3>")
    endif()
    set(PEDALBOARD_NAM_AVAILABLE TRUE)
    message(STATUS "PedalBoard: NeuralAmpModelerCore enabled (Eigen: ${EIGEN_ROOT})")
else()
    set(PEDALBOARD_NAM_AVAILABLE FALSE)
    message(WARNING "PedalBoard: NeuralAmpModelerCore or Eigen not found in third_party/ -> NAM disabled")
endif()

# ------------------------------------------------------------------------------
# Optional Steinberg ASIO SDK auto-detection (JUCE_ASIO=1)
# ------------------------------------------------------------------------------
set(ASIO_SDK_DIR "" CACHE PATH "Optional path to extracted Steinberg ASIO SDK containing common/iasiodrv.h")
set(_asio_candidates
    "${ASIO_SDK_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/asiosdk"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ASIOSDK"
    "C:/SDKs/ASIO_SDK"
    "C:/ASIOSDK"
)
set(PEDALBOARD_ASIO_COMMON_DIR "")
foreach(_cand IN LISTS _asio_candidates)
    if(_cand AND EXISTS "${_cand}/common/iasiodrv.h")
        set(PEDALBOARD_ASIO_COMMON_DIR "${_cand}/common")
        break()
    elseif(_cand AND EXISTS "${_cand}/iasiodrv.h")
        set(PEDALBOARD_ASIO_COMMON_DIR "${_cand}")
        break()
    endif()
endforeach()

# ------------------------------------------------------------------------------
# Windows: Microsoft Edge WebView2 SDK (static loader for embedded TONE3000 popup)
# ------------------------------------------------------------------------------
option(PEDALBOARD_ENABLE_WEBVIEW2 "Embed Microsoft Edge WebView2 in TONE3000 Popup Window" ON)
set(_pb_webview2 FALSE)
if(WIN32 AND PEDALBOARD_ENABLE_WEBVIEW2)
    set(PB_WEBVIEW2_VERSION "1.0.1901.177")
    set(_wv2_root "${CMAKE_BINARY_DIR}/webview2")
    set(_wv2_pkg  "${_wv2_root}/Microsoft.Web.WebView2.${PB_WEBVIEW2_VERSION}")
    if(NOT EXISTS "${_wv2_pkg}/build/native/include/WebView2.h")
        message(STATUS "PedalBoard: Downloading Microsoft.Web.WebView2 ${PB_WEBVIEW2_VERSION} SDK...")
        file(MAKE_DIRECTORY "${_wv2_root}")
        file(DOWNLOAD
            "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${PB_WEBVIEW2_VERSION}"
            "${_wv2_root}/webview2.nupkg"
            STATUS _wv2_status
            SHOW_PROGRESS
        )
        list(GET _wv2_status 0 _wv2_code)
        if(NOT _wv2_code EQUAL 0)
            message(FATAL_ERROR "PedalBoard: WebView2 SDK download failed (${_wv2_status}). Pass -DPEDALBOARD_ENABLE_WEBVIEW2=OFF to disable.")
        endif()
        file(ARCHIVE_EXTRACT INPUT "${_wv2_root}/webview2.nupkg" DESTINATION "${_wv2_pkg}")
    endif()
    if(NOT JUCE_WEBVIEW2_PACKAGE_LOCATION)
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${_wv2_root}")
    endif()
    set(_pb_webview2 TRUE)
    message(STATUS "PedalBoard: Microsoft Edge WebView2 enabled (${_wv2_pkg})")
endif()

# ------------------------------------------------------------------------------
# Main Plugin Target (Standalone .exe + VST3)
# ------------------------------------------------------------------------------
juce_add_plugin(PedalBoard
    COMPANY_NAME "PedalBoard"
    PLUGIN_MANUFACTURER_CODE PdBd
    PLUGIN_CODE Pdb1
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEBVIEW2 ${_pb_webview2}
    FORMATS Standalone VST3
    PRODUCT_NAME "PedalBoard"
)

target_sources(PedalBoard
    PRIVATE
        src/cloud/Tone3000Client.cpp
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp
)

if(TARGET PedalBoard_Standalone)
    target_sources(PedalBoard_Standalone
        PRIVATE
            src/plugin/StandaloneBridge.cpp
    )
endif()

target_include_directories(PedalBoard
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(PedalBoard
    PUBLIC
        JUCE_WEB_BROWSER=$<BOOL:${_pb_webview2}>
        PEDALBOARD_HAS_WEBVIEW2=$<BOOL:${_pb_webview2}>
        $<$<BOOL:${_pb_webview2}>:JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1>
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
)

if(PEDALBOARD_ASIO_COMMON_DIR)
    message(STATUS "PedalBoard: Steinberg ASIO SDK detected at ${PEDALBOARD_ASIO_COMMON_DIR} -> JUCE_ASIO=1")
    target_compile_definitions(PedalBoard PUBLIC JUCE_ASIO=1 PEDALBOARD_HAS_ASIO=1)
    target_include_directories(PedalBoard SYSTEM PUBLIC "${PEDALBOARD_ASIO_COMMON_DIR}")
else()
    message(STATUS "PedalBoard: Steinberg ASIO SDK not found -> WASAPI/DirectSound mode (place SDK in third_party/asiosdk to auto-enable ASIO)")
    target_compile_definitions(PedalBoard PUBLIC JUCE_ASIO=0 PEDALBOARD_HAS_ASIO=0)
endif()

if(PEDALBOARD_NAM_AVAILABLE)
    target_link_libraries(PedalBoard PUBLIC $<LINK_LIBRARY:WHOLE_ARCHIVE,nam_core>)
else()
    target_compile_definitions(PedalBoard PUBLIC PEDALBOARD_HAS_NAM=0)
endif()

# ------------------------------------------------------------------------------
# Lightweight Embedded AmpliTube-Style 3D Hardware Control Filmstrips Only
# (Chassis, stage & icons use crisp, ultra-light Valeton GP-200 vector assets!)
# ------------------------------------------------------------------------------
set(SKIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/skins/default")
juce_add_binary_data(PedalBoardAssets
    SOURCES
        "${SKIN_DIR}/knob_vintage_skirted_64.png"
        "${SKIN_DIR}/knob_gold_plexi_64.png"
        "${SKIN_DIR}/knob_chrome_knurled_64.png"
        "${SKIN_DIR}/knob_cyan_stomp_64.png"
        "${SKIN_DIR}/footswitch_chrome_2.png"
        "${SKIN_DIR}/toggle_chrome_2.png"
        "${SKIN_DIR}/fader_console_cap.png"
)
if(MSVC)
    target_compile_options(PedalBoardAssets PRIVATE /W0)
endif()

target_link_libraries(PedalBoard
    PRIVATE
        PedalBoardAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(WIN32)
    target_link_libraries(PedalBoard PRIVATE crypt32)
endif()
