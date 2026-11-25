cmake_minimum_required(VERSION 3.20)
project(OpenMoonLoaderTurbo VERSION 2.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# GTA San Andreas and SA-MP are 32-bit (Win32 / x86) processes.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "OpenMoonLoader must be built as a 32-bit (Win32 / x86) target.")
endif()

option(OPENMOON_WITH_LUAJIT "Link against 32-bit LuaJIT (luajit.lib) when available" OFF)
option(OPENMOON_BUILD_SAMPLE_ADDON "Build the sample native C++ addon DLL" ON)

add_library(OpenMoonLoader SHARED
    src/core/dllmain.cpp
    src/core/config.cpp
    src/core/event_bus.cpp
    src/core/addon_loader.cpp
    src/core/file_watcher.cpp
    src/runtime/script_manager.cpp
    src/runtime/lua_bridge.cpp
    src/modules/builtin_modules.cpp
)

target_include_directories(OpenMoonLoader
    PUBLIC
        include
    PRIVATE
        src
)

set_target_properties(OpenMoonLoader PROPERTIES
    PREFIX ""
    OUTPUT_NAME "MoonLoader"
    SUFFIX ".asi"
    DEFINE_SYMBOL "OPENMOON_EXPORTS"
)

target_compile_definitions(OpenMoonLoader PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

if(OPENMOON_WITH_LUAJIT)
    target_compile_definitions(OpenMoonLoader PRIVATE OPENMOON_HAS_LUAJIT=1)
    find_library(LUAJIT_LIB NAMES luajit lua51 PATHS "${CMAKE_SOURCE_DIR}/third_party/luajit/lib")
    target_include_directories(OpenMoonLoader PRIVATE "${CMAKE_SOURCE_DIR}/third_party/luajit/include")
    if(LUAJIT_LIB)
        target_link_libraries(OpenMoonLoader PRIVATE "${LUAJIT_LIB}")
    endif()
endif()

if(MSVC)
    target_compile_options(OpenMoonLoader PRIVATE
        $<$<CONFIG:Release>:/O2 /Ob2 /Oi /Ot /Oy /GT /Gy /Gw /GF /arch:SSE2 /fp:fast /EHsc /GL /DNDEBUG>
        $<$<CONFIG:Debug>:/Od /Zi /EHsc>
    )
    target_link_options(OpenMoonLoader PRIVATE
        $<$<CONFIG:Release>:/OPT:REF /OPT:ICF=10 /LTCG /INCREMENTAL:NO>
        $<$<CONFIG:Debug>:/DEBUG>
    )
endif()

if(OPENMOON_BUILD_SAMPLE_ADDON)
    add_library(SampleNativeAddon SHARED
        plugins/sample_addon/sample_addon.cpp
    )
    target_include_directories(SampleNativeAddon PRIVATE include)
    target_link_libraries(SampleNativeAddon PRIVATE OpenMoonLoader)
    set_target_properties(SampleNativeAddon PROPERTIES
        PREFIX ""
        OUTPUT_NAME "sample_addon"
        SUFFIX ".dll"
    )
    if(MSVC)
        target_compile_options(SampleNativeAddon PRIVATE
            $<$<CONFIG:Release>:/O2 /Ob2 /Oi /Ot /Oy /GT /Gy /Gw /GF /arch:SSE2 /fp:fast /EHsc /GL /DNDEBUG>
            $<$<CONFIG:Debug>:/Od /Zi /EHsc>
        )
        target_link_options(SampleNativeAddon PRIVATE
            $<$<CONFIG:Release>:/OPT:REF /OPT:ICF=10 /LTCG /INCREMENTAL:NO>
            $<$<CONFIG:Debug>:/DEBUG>
        )
    endif()
endif()
