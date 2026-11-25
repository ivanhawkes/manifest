# The development console's commands, apart from any terminal: built and tested with the TUI off
# (docs/console.md).
add_library(manifest_console STATIC ManifestConsole.cpp ManifestConsole.h)
target_include_directories(manifest_console PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(manifest_console
    PUBLIC Manifest::Game Manifest::Host Manifest::Remote Jettison::Devtools
    PRIVATE Jettison::Reflect manifest_warnings)
set_target_properties(manifest_console PROPERTIES FOLDER "Tools")

if(MANIFEST_BUILD_TESTS AND BUILD_TESTING)
    add_executable(manifest_console_tests tests/ManifestConsoleTests.cpp)
    target_link_libraries(manifest_console_tests PRIVATE
        manifest_console manifest_test_helpers doctest::doctest_with_main manifest_warnings)
    target_compile_definitions(manifest_console_tests PRIVATE
        MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}"
        MANIFEST_MOD_PACK="${MANIFEST_PACK}")
    add_dependencies(manifest_console_tests manifest_validate_pack)
    set_target_properties(manifest_console_tests PROPERTIES FOLDER "Test")
    add_test(NAME console COMMAND manifest_console_tests)
    # Timed, so alone: nothing else in the gate shares the machine while it runs.
    add_test(NAME step-budget COMMAND manifest_console_tests --no-skip "-tc=*within its time budget")
    set_tests_properties(step-budget PROPERTIES RUN_SERIAL TRUE)
endif()
