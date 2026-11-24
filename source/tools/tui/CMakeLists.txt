find_package(Threads REQUIRED)
add_executable(manifest_tui main.cpp ManifestTui.cpp ManifestTui.h)
set_target_properties(manifest_tui PROPERTIES FOLDER "Tools")
target_link_libraries(manifest_tui PRIVATE manifest_console Manifest::Host
    ftxui::screen ftxui::component ftxui::dom Threads::Threads manifest_warnings)
manifest_distributed(manifest_tui)
# The content it finds by default, beside it.
add_dependencies(manifest_tui manifest_validate_pack)
