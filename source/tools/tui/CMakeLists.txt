find_package(Threads REQUIRED)
add_executable(manifest_tui main.cpp ManifestTui.cpp ManifestTui.h)
set_target_properties(manifest_tui PROPERTIES FOLDER "Tools")
target_link_libraries(manifest_tui PRIVATE manifest_console
    ftxui::screen ftxui::component ftxui::dom Threads::Threads manifest_warnings)
target_compile_definitions(manifest_tui PRIVATE MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}")
add_dependencies(manifest_tui manifest_mod)
