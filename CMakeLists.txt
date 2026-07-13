cmake_minimum_required(VERSION 3.16)
project(openmw05 CXX)

# Hard constraint (CLAUDE.md §2): C++17, strictly.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OMW05_WITH_IMGUI "Build the Dear ImGui debug UI layer" ON)
option(OMW05_BUILD_TESTS "Build unit tests" ON)
option(OMW05_BUILD_TOOLS "Build mw-* CLI tools" ON)
option(OMW05_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(Omw05Warnings)

add_subdirectory(extern)
add_subdirectory(src)

if(OMW05_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

if(OMW05_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
