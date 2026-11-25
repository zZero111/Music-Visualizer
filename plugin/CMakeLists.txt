cmake_minimum_required(VERSION 3.22)
project(SphereVisualizer VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5) # libogg 1.3.5 declares an old minimum that CMake 4 rejects
# Opus decoding (JUCE has no Opus reader): Xiph's libogg + libopus, and opusfile built from source.
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_DISABLE_INTRINSICS ON CACHE BOOL "" FORCE) # keeps universal (arm64 + x86_64) Mac builds simple
FetchContent_Declare(ogg GIT_REPOSITORY https://github.com/xiph/ogg.git GIT_TAG v1.3.5 GIT_SHALLOW TRUE)
FetchContent_Declare(opus GIT_REPOSITORY https://github.com/xiph/opus.git GIT_TAG v1.5.2 GIT_SHALLOW TRUE)
FetchContent_Declare(opusfile GIT_REPOSITORY https://github.com/xiph/opusfile.git GIT_TAG v0.12 GIT_SHALLOW TRUE
    SOURCE_SUBDIR no-cmake-build) # download only; built below
FetchContent_MakeAvailable(ogg opus opusfile)
add_library(sphere_opusfile STATIC
    ${opusfile_SOURCE_DIR}/src/info.c
    ${opusfile_SOURCE_DIR}/src/internal.c
    ${opusfile_SOURCE_DIR}/src/opusfile.c
    ${opusfile_SOURCE_DIR}/src/stream.c)
target_include_directories(sphere_opusfile PUBLIC ${opusfile_SOURCE_DIR}/include ${opus_SOURCE_DIR}/include)
target_compile_definitions(sphere_opusfile PRIVATE OP_DISABLE_HTTP )
target_link_libraries(sphere_opusfile PUBLIC ogg opus)
set_target_properties(sphere_opusfile ogg opus PROPERTIES POSITION_INDEPENDENT_CODE ON)

# AAX needs Avid's SDK: download it, then configure with -DAAX_SDK_PATH=/path/to/aax-sdk
set(SPHERE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SPHERE_FORMATS AU)
endif()
if(AAX_SDK_PATH)
    juce_set_aax_sdk_path(${AAX_SDK_PATH})
    list(APPEND SPHERE_FORMATS AAX)
endif()

juce_add_plugin(SphereVisualizer
    COMPANY_NAME "Sphere Audio"
    BUNDLE_ID com.sphereaudio.spherevisualizer
    PLUGIN_MANUFACTURER_CODE Sphr
    PLUGIN_CODE Sph1
    FORMATS ${SPHERE_FORMATS}
    PRODUCT_NAME "Sphere Visualizer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE)

juce_add_binary_data(SphereAssets SOURCES
    ../web/index.html
    Resources/three.min.js
    Resources/mp4-muxer.js)

target_sources(SphereVisualizer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/OpusFormat.cpp)

target_compile_definitions(SphereVisualizer PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(SphereVisualizer
    PRIVATE
        SphereAssets
        sphere_opusfile
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
