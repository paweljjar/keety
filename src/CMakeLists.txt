# Library containing compiler internals
add_library(keety_core STATIC
    diag/source_manager.cpp
)

target_include_directories(keety_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
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
