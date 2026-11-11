# Library containing compiler internals
add_library(keety_core STATIC
    diag/source_manager.cpp
    diag/diagnostic_emitter.cpp
    diag/levenshtein.cpp
    diag/error_code.cpp
    diag/diagnostic_translator.cpp
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
    ir/operand.cpp
    ir/instruction.cpp
    ir/basic_block.cpp
    ir/function.cpp
    ir/module.cpp
    ir/ast_lowerer.cpp
    ir/liveness_analysis.cpp
    ir/move_insertion.cpp
    ir/const_ref_promotion.cpp
    ir/pipeline_reserve.cpp
    ir/recursive_boxing.cpp
    emitter/header_emitter.cpp
    emitter/cpp_emitter.cpp
    emitter/line_directive_tracker.cpp
    driver/toolchain_detector.cpp
    driver/toolchain_driver.cpp
    driver/cli_driver.cpp
    driver/project_root.cpp
    fmt/comment_attacher.cpp
    fmt/import_sorter.cpp
    fmt/formatter.cpp
    kml/kml_token.cpp
    kml/kml_lexer.cpp
    kml/kml_parser.cpp
    kml/dependency_analyzer.cpp
    kml/style_scoper.cpp
    kml/kml_lowerer.cpp
    lint/lint_config.cpp
    lint/suppression_map.cpp
    lint/lint_engine.cpp
)

target_include_directories(keety_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../runtime>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(keety_core PUBLIC
    keety_warnings
    $<$<PLATFORM_ID:Windows>:ws2_32>
    $<$<PLATFORM_ID:Linux>:${CMAKE_DL_LIBS}>
)

if(APPLE)
    target_link_libraries(keety_core PUBLIC "-framework Cocoa" "-framework CoreGraphics")
endif()

target_compile_definitions(keety_core PRIVATE
    KEETY_DEFAULT_RUNTIME_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../runtime"
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

# Package manager library
add_library(kattery_core STATIC
    pkg/diagnostics.cpp
    pkg/source_spec.cpp
    pkg/manifest.cpp
    pkg/lockfile.cpp
    pkg/workspace.cpp
    pkg/process.cpp
    pkg/git_client.cpp
    pkg/fetcher.cpp
    pkg/graph.cpp
    pkg/installer.cpp
    pkg/script_runner.cpp
    pkg/cli.cpp
)

target_include_directories(kattery_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../runtime>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(kattery_core PUBLIC
    keety_warnings
)

# Package manager executable
add_executable(kattery
    kattery/main.cpp
)

target_link_libraries(kattery PRIVATE
    kattery_core
    keety_warnings
)

set_target_properties(kattery PROPERTIES
    OUTPUT_NAME "kattery"
)

# -----------------------------------------------------------------------------
# KeetScript Language Server Protocol (LSP) Engine & Executable
# -----------------------------------------------------------------------------
add_library(keet_lsp_core STATIC
    lsp/protocol.cpp
    lsp/transport.cpp
    lsp/document.cpp
    lsp/analyzer.cpp
    lsp/server.cpp
)

target_include_directories(keet_lsp_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../runtime>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(keet_lsp_core PUBLIC
    keety_core
    keety_warnings
)

add_executable(keet-lsp
    lsp/main.cpp
)

target_link_libraries(keet-lsp PRIVATE
    keet_lsp_core
    keety_warnings
)

set_target_properties(keet-lsp PROPERTIES
    OUTPUT_NAME "keet-lsp"
)

