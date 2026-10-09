cmake_minimum_required(VERSION 3.28)

project(Opcoda VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OPCODA_BUILD_TESTS "Compila os testes" ON)
option(OPCODA_BUILD_PLUGIN "Compila Standalone e VST3" ON)
option(OPCODA_ASAN "Ativa o AddressSanitizer" OFF)
option(OPCODA_WERROR "Trata warnings como erro" ON)

set(JUCE_DIR "I:/deps/JUCE" CACHE PATH "Clone do JUCE")

if(OPCODA_ASAN)
    add_compile_options(/fsanitize=address)
    add_link_options(/fsanitize=address)
endif()

add_library(opcoda_core STATIC
    src/opcoda_core/pe/pe_parser.cpp
    src/opcoda_core/pe/byte_to_sample.cpp
    src/opcoda_core/pe/byte_range.cpp
    src/opcoda_core/pe/byte_to_position.cpp
    src/opcoda_core/pe/column_reduction.cpp
    src/opcoda_core/entropy/shannon_entropy.cpp
    src/opcoda_core/dsp/biquad.cpp
    src/opcoda_core/dsp/lfo.cpp
    src/opcoda_core/dsp/dc_blocker.cpp
    src/opcoda_core/dsp/limiter.cpp
    src/opcoda_core/dsp/window.cpp
    src/opcoda_core/dsp/granular_engine.cpp
    src/opcoda_core/rt/alloc_guard.cpp
    src/opcoda_core/rt/note_tracker.cpp
    src/opcoda_core/rt/transport.cpp)

target_include_directories(opcoda_core PUBLIC src)
target_compile_options(opcoda_core PUBLIC /W4 /permissive- /Zc:__cplusplus /utf-8 /EHsc)
target_compile_definitions(opcoda_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)

if(OPCODA_WERROR)
    target_compile_options(opcoda_core PUBLIC /WX)
endif()

if(OPCODA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(OPCODA_BUILD_PLUGIN)
    add_subdirectory(src/opcoda_plugin)
endif()
