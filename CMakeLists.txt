cmake_minimum_required(VERSION 3.24)
project(125A_Akai_S3000XL VERSION 0.5.0 LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "125A Akai S3000XL Windows VST3 requires MSVC/Visual Studio 2022.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(AKAI_MAME_ROOT "" CACHE PATH "Patched MAME 0.288 source root and VS2022 static libraries")
set(AKAI_JUCE_ROOT "" CACHE PATH "JUCE 8.0.12 source directory")
if(NOT EXISTS "${AKAI_MAME_ROOT}/src/mame/akai/s3000.cpp")
    message(FATAL_ERROR "Provide -DAKAI_MAME_ROOT pointing to patched MAME 0.288")
endif()
if(NOT EXISTS "${AKAI_JUCE_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "Provide -DAKAI_JUCE_ROOT pointing to JUCE 8.0.12")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(AKAI_ASSETS "${CMAKE_CURRENT_BINARY_DIR}/Generated/OpenResources")
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build/generate_open_resources.py"
            --mame-root "${AKAI_MAME_ROOT}" --out "${AKAI_ASSETS}"
    RESULT_VARIABLE _akai_resource_status
    OUTPUT_VARIABLE _akai_resource_report
    ERROR_VARIABLE _akai_resource_error)
if(NOT _akai_resource_status EQUAL 0)
    message(FATAL_ERROR "Open source MAME layout resources: ${_akai_resource_error}")
endif()
message(STATUS "${_akai_resource_report}")
add_subdirectory("${AKAI_JUCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
juce_add_binary_data(AkaiOpenResources SOURCES
    "${AKAI_ASSETS}/floppy.svg"
    "${AKAI_ASSETS}/cart.svg"
    "${AKAI_ASSETS}/mame_plugins.zip")
juce_add_plugin(AkaiS3000XL
    COMPANY_NAME "125A"
    BUNDLE_ID "com.one25a.akai.s3000xl"
    PLUGIN_MANUFACTURER_CODE A125
    PLUGIN_CODE A3XL
    FORMATS VST3
    PRODUCT_NAME "125A Akai S3000XL"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    PRODUCES_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Instrument" "Sampler"
)
juce_generate_juce_header(AkaiS3000XL)
target_sources(AkaiS3000XL PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetBrowserComponent.cpp
)
target_compile_features(AkaiS3000XL PRIVATE cxx_std_20)
target_compile_definitions(AkaiS3000XL PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    ASIO_STANDALONE=1
)
target_compile_options(AkaiS3000XL PRIVATE
    /bigobj /permissive- /EHsc /MP /arch:AVX2 /fp:precise
)
set(MAME_SRC "${AKAI_MAME_ROOT}/src")
target_include_directories(AkaiS3000XL PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${MAME_SRC}"
    "${MAME_SRC}/frontend/mame"
    "${MAME_SRC}/emu"
    "${MAME_SRC}/osd"
    "${MAME_SRC}/osd/modules/render"
    "${MAME_SRC}/lib"
    "${MAME_SRC}/lib/util"
    "${MAME_SRC}/devices"
    "${MAME_SRC}/devices/machine"
    "${MAME_SRC}/mame/akai"
    "${AKAI_MAME_ROOT}/3rdparty"
    "${AKAI_MAME_ROOT}/3rdparty/asio/include"
    "${AKAI_MAME_ROOT}/3rdparty/zlib"
    "${AKAI_MAME_ROOT}/build/generated"
)
set(_akai_lib_paths
    "${AKAI_MAME_ROOT}/build/vs2022/bin/x64/Release"
    "${AKAI_MAME_ROOT}/build/vs2022/bin/x64/Release/mame_s3000xl"
    "${AKAI_MAME_ROOT}/build/vs2022/bin/x64/Release/mames3000xl"
)
set(_akai_mame_libs
    frontend mame_s3000xl emu ocore_windows utils optional
    expat zlib portmidi flac 7z softfloat3 utf8proc jpeg sqlite3 zstd
    formats dasm portaudio lua lualibs asmjit bgfx bimg bx ymfm
    wdlfft linenoise qtdbg_windows osd_windows
)
set(_akai_resolved_libs "")
foreach(lib IN LISTS _akai_mame_libs)
    find_file(_akai_found_${lib} NAMES "${lib}.lib"
        PATHS ${_akai_lib_paths} NO_DEFAULT_PATH)
    if(NOT _akai_found_${lib})
        message(FATAL_ERROR "MAME static library missing: ${lib}.lib\n"
            "Build mames3000xl.sln Release/x64 with the SAME VS2022 toolset first.")
    endif()
    list(APPEND _akai_resolved_libs "${_akai_found_${lib}}")
endforeach()
target_link_libraries(AkaiS3000XL PRIVATE
    AkaiOpenResources
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_plugin_client
    juce::juce_gui_extra
    juce::juce_midi_ci
    ${_akai_resolved_libs}
    ws2_32 winmm dinput8 dxguid opengl32 dsound xinput comctl32
    shlwapi ntdll userenv bcrypt advapi32 avrt
)
message(STATUS "125A S3000XL: Windows VST3; MAME 0.288; JUCE 8.0.12")
message(STATUS "Firmware: no ROM bytes are stored in this public source or CI build.")
message(STATUS "GUI: original MAME hardware panel; abandoned SD-1 label PNGs omitted.")
