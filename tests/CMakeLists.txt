include(FetchContent)

# Catch2 v3 provides fast, modern testing with C++20 support
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
)

FetchContent_MakeAvailable(Catch2)

# Unit test runner
add_executable(keety_unit_tests
    unit/test_version.cpp
    unit/test_source_location.cpp
    unit/test_source_manager.cpp
    unit/test_diagnostic_emitter.cpp
    unit/test_error_code.cpp
    unit/test_runtime_error.cpp
    unit/test_runtime_bool_byte.cpp
    unit/test_runtime_deep_copy_ptr.cpp
    unit/test_runtime_numeric.cpp
)

target_link_libraries(keety_unit_tests PRIVATE
    keety_core
    keety_warnings
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(keety_unit_tests)
