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
)

target_link_libraries(keety_unit_tests PRIVATE
    keety_core
    keety_warnings
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(keety_unit_tests)
