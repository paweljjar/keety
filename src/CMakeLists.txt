# Library containing compiler internals
add_library(keety_core STATIC
    diag/source_manager.cpp
    diag/diagnostic_emitter.cpp
    diag/error_code.cpp
    lexer/token.cpp
    lexer/lexer.cpp
    parser/parser.cpp
    resolver/module_path_resolver.cpp
    resolver/module_graph.cpp
    resolver/cycle_detector.cpp
    resolver/topological_sorter.cpp
    sema/scope.cpp
    sema/universe.cpp
    sema/shadow_checker.cpp
    sema/const_view_checker.cpp
    typechecker/type.cpp
    typechecker/type_checker.cpp
)

target_include_directories(keety_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../runtime>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(keety_core PUBLIC
    keety_warnings
)

# Main compiler executable
add_executable(keety
    driver/main.cpp
)

target_link_libraries(keety PRIVATE
    keety_core
    keety_warnings
)

set_target_properties(keety PROPERTIES
    OUTPUT_NAME "keety"
)
