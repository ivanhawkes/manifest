# The graphical client (docs/client.md): Manifest's game drawn by Jettison's shell, which starts it
# through a session of its own over the engine's assets, the staged content mounted over them.
add_executable(manifest_client main.cpp)
set_target_properties(manifest_client PROPERTIES FOLDER "Apps")
target_link_libraries(manifest_client PRIVATE Manifest::Host Jettison::App manifest_warnings)
# The engine's staged assets, found from their target: jettison_use_shared_assets reads a variable of
# the engine's own directory, which a project that fetches the engine does not see.
get_property(engine_assets TARGET jettison_assets PROPERTY JETTISON_ASSET_DIRECTORY)
file(RELATIVE_PATH asset_subdirectory "${CMAKE_CURRENT_BINARY_DIR}" "${engine_assets}")
file(RELATIVE_PATH content_subdirectory "${CMAKE_CURRENT_BINARY_DIR}" "${MANIFEST_MOD_DIRECTORY}")
target_compile_definitions(manifest_client PRIVATE
    "JETTISON_ASSET_SUBDIRECTORY=\"${asset_subdirectory}\""
    "MANIFEST_CONTENT_SUBDIRECTORY=\"${content_subdirectory}\"")
add_dependencies(manifest_client jettison_assets manifest_validate_mod)
