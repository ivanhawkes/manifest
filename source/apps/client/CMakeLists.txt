# The client's model (docs/client.md, "The model"): the screens' data from the player's snapshot and
# their actions as requests, with no UI, so its suite plays the first loop with no window.
manifest_add_module(client_model
	NAMESPACE Client
	DEPENDS
		types
		query
		host
	HEADERS
		include/manifest/client/ClientModel.h
	SOURCES
		src/ClientModel.cpp
	TESTS
		tests/ClientModelTests.cpp
	PUBLIC_LIBRARIES
		Jettison::Sim
	PRIVATE_LIBRARIES
		Jettison::Reflect
		Jettison::Serialise
)
if(TARGET manifest_client_model_tests)
	target_compile_definitions(manifest_client_model_tests PRIVATE
		MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}")
	add_dependencies(manifest_client_model_tests manifest_validate_pack)
endif()

if(NOT MANIFEST_BUILD_CLIENT)
	return()
endif()

# The client's presentation on the shell: its screens over the model, a library so the screens check
# loads them with no shell.
add_library(manifest_client_game STATIC ClientGame.cpp ClientGame.h)
target_include_directories(manifest_client_game PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(manifest_client_game
	PUBLIC Manifest::Client Jettison::App Jettison::Ui
	PRIVATE Jettison::Reflect manifest_warnings)
set_target_properties(manifest_client_game PROPERTIES FOLDER "Apps")

# The graphical client: Manifest's game drawn by Jettison's shell, which starts it through a session
# of its own over the engine's assets, the staged content mounted over them.
add_executable(manifest_client main.cpp)
set_target_properties(manifest_client PROPERTIES FOLDER "Apps")
target_link_libraries(manifest_client PRIVATE manifest_client_game Manifest::Host manifest_warnings)

# The engine's staged assets, found from their target: jettison_use_shared_assets reads a variable of
# the engine's own directory, which a project that fetches the engine does not see.
get_property(engine_assets TARGET jettison_assets PROPERTY JETTISON_ASSET_DIRECTORY)
file(RELATIVE_PATH asset_subdirectory "${CMAKE_CURRENT_BINARY_DIR}" "${engine_assets}")
file(RELATIVE_PATH content_subdirectory "${CMAKE_CURRENT_BINARY_DIR}" "${MANIFEST_MOD_DIRECTORY}")
target_compile_definitions(manifest_client PRIVATE
	"JETTISON_ASSET_SUBDIRECTORY=\"${asset_subdirectory}\""
	"MANIFEST_CONTENT_SUBDIRECTORY=\"${content_subdirectory}\"")
add_dependencies(manifest_client jettison_assets manifest_validate_mod)
