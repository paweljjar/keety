cmake_minimum_required(VERSION 3.20)

project(keety
    VERSION 0.1.0
    DESCRIPTION "The official AOT compiler and toolchain driver for the KeetScript programming language"
    LANGUAGES CXX
)

# Enforce standard C++20 for the compiler implementation
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers (clangd, CLion, VS Code)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# Configuration Options
# -----------------------------------------------------------------------------
option(KEETY_ENABLE_WERROR "Treat warnings as errors" OFF)
option(KEETY_ENABLE_CLANG_TIDY "Enable clang-tidy analysis during build" OFF)
option(KEETY_BUILD_TESTS "Build conformance and unit test suites" ON)
option(KEETY_BUILD_BENCHMARKS "Build reference performance benchmarks" ON)

if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# -----------------------------------------------------------------------------
# Compiler Warnings Interface Target
# -----------------------------------------------------------------------------
add_library(keety_warnings INTERFACE)

if(MSVC)
    target_compile_definitions(keety_warnings INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(keety_warnings INTERFACE
        /W4
        /permissive-
        $<$<BOOL:${KEETY_ENABLE_WERROR}>:/WX>
        /w14242 # 'identfier': conversion from 'type1' to 'type1', possible loss of data
        /w14254 # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits'
        /w14263 # 'function': member function does not override any base class virtual member function
        /w14265 # 'classname': class has virtual functions, but destructor is not virtual
        /w14287 # 'operator': unsigned/negative constant mismatch
        /w14296 # 'operator': expression is always false
        /w14311 # 'variable': pointer truncation from 'type' to 'type'
        /w14545 # expression before comma evaluates to a function which is missing an argument list
        /w14546 # function call before comma missing argument list
        /w14547 # 'operator': operator before comma has no effect
        /w14549 # 'operator': operator before comma has no effect
        /w14555 # expression has no effect
        /w14619 # pragma warning: there is no warning number 'number'
        /w14640 # enable warning on thread un-safe static member initialization
        /w14826 # conversion from 'type1' to 'type_2' is sign-extended
        /w14905 # wide string literal cast to 'LPSTR'
        /w14906 # string literal cast to 'LPWSTR'
        /w14928 # illegal copy-initialization
    )
else()
    target_compile_options(keety_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        $<$<BOOL:${KEETY_ENABLE_WERROR}>:-Werror>
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
    )
endif()

# -----------------------------------------------------------------------------
# clang-tidy Integration
# -----------------------------------------------------------------------------
if(KEETY_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    else()
        message(WARNING "KEETY_ENABLE_CLANG_TIDY is ON, but clang-tidy executable was not found.")
    endif()
endif()

# -----------------------------------------------------------------------------
# clang-format Integration
# -----------------------------------------------------------------------------
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    message(STATUS "clang-format found: ${CLANG_FORMAT_EXE}")
    file(GLOB_RECURSE ALL_SOURCE_FILES
        LIST_DIRECTORIES false
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/runtime/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/runtime/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.hpp"
    )

    add_custom_target(format
        COMMAND "${CLANG_FORMAT_EXE}" -i ${ALL_SOURCE_FILES}
        COMMENT "Formatting all source files with clang-format"
    )

    add_custom_target(format-check
        COMMAND "${CLANG_FORMAT_EXE}" --dry-run --Werror ${ALL_SOURCE_FILES}
        COMMENT "Checking code formatting with clang-format"
    )
else()
    message(STATUS "clang-format not found; format targets will not be available.")
endif()

# -----------------------------------------------------------------------------
# Project Subdirectories
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt")
    add_subdirectory(src)
endif()

if(KEETY_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif()

if(KEETY_BUILD_BENCHMARKS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/CMakeLists.txt")
    add_subdirectory(benchmarks)
endif()
