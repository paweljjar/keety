cmake_minimum_required(VERSION 3.20)

add_executable(keety_benchmarks
    main.cpp
    bench_numeric_vector.cpp
    bench_hash_map.cpp
    bench_string_ingestion.cpp
    bench_task_concurrency.cpp
)

target_include_directories(keety_benchmarks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/runtime
)

target_link_libraries(keety_benchmarks PRIVATE
    keety_warnings
)

# Apply performance benchmark flags (-O3 -DNDEBUG -march=native)
target_compile_definitions(keety_benchmarks PRIVATE NDEBUG)
if(MSVC)
    target_compile_options(keety_benchmarks PRIVATE /O2 /Oi /Ot)
else()
    target_compile_options(keety_benchmarks PRIVATE -O3)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
    if(COMPILER_SUPPORTS_MARCH_NATIVE)
        target_compile_options(keety_benchmarks PRIVATE -march=native)
    endif()
endif()

if(KEETY_BUILD_TESTS)
    add_test(
        NAME bench_numeric_vector
        COMMAND keety_benchmarks --benchmark 1 --size 5000000 --warmup 2 --measure 5
    )
    add_test(
        NAME bench_hash_map
        COMMAND keety_benchmarks --benchmark 2 --size 100000 --warmup 2 --measure 5
    )
    add_test(
        NAME bench_string_ingestion
        COMMAND keety_benchmarks --benchmark 3 --size 500000 --warmup 2 --measure 5
    )
    add_test(
        NAME bench_task_concurrency
        COMMAND keety_benchmarks --benchmark 4 --size 10000 --warmup 2 --measure 3
    )
endif()
