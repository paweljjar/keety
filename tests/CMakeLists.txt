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
    unit/test_token.cpp
    unit/test_lexer.cpp
    unit/test_blank_ident.cpp
    unit/test_angle_brackets.cpp
    unit/test_template_literals.cpp
    unit/test_ast_decl.cpp
    unit/test_ast_stmt.cpp
    unit/test_ast_expr.cpp
    unit/test_ast_type.cpp
    unit/test_parser.cpp
    unit/test_pratt_parser.cpp
    unit/test_header_disambiguation.cpp
    unit/test_zonk_disambiguation.cpp
    unit/test_loop_switch_labels.cpp
    unit/test_module_path_resolver.cpp
    unit/test_module_graph.cpp
    unit/test_cycle_detector.cpp
    unit/test_topological_sorter.cpp
    unit/test_symbol_scope.cpp
    unit/test_universe_symbols.cpp
    unit/test_no_shadowing.cpp
    unit/test_const_view.cpp
    unit/test_type_checker.cpp
    unit/test_keet_ir.cpp
    unit/test_ast_lowerer.cpp
    unit/test_zonk_lowering.cpp
)

target_link_libraries(keety_unit_tests PRIVATE
    keety_core
    keety_warnings
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(keety_unit_tests)
