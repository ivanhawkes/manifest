# The client's model (docs/client.md, "The model"): the screens' data from the player's snapshot and
# their actions as requests, with no UI, so its suite plays the first loop with no window.
manifest_add_module(client_model
	NAMESPACE Client
	DEPENDS
		types
		query
		host
		remote
	HEADERS
		include/manifest/client/ClientModel.h
	SOURCES
		src/ClientModel.cpp
	TESTS
		tests/ClientModelTests.cpp
	PUBLIC_LIBRARIES
		Jettison::Sim
	PRIVATE_LIBRARIES
		Jettison::Core
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

# The engine's staged assets, beside the executable as the engine stages them, and Manifest's content.
jettison_use_shared_assets(manifest_client)
get_property(engine_assets TARGET jettison_assets PROPERTY JETTISON_ASSET_DIRECTORY)
file(RELATIVE_PATH content_subdirectory "${CMAKE_CURRENT_BINARY_DIR}" "${MANIFEST_MOD_DIRECTORY}")
target_compile_definitions(manifest_client PRIVATE
	"MANIFEST_CONTENT_SUBDIRECTORY=\"${content_subdirectory}\"")
add_dependencies(manifest_client jettison_assets manifest_validate_mod)

if(MANIFEST_BUILD_TESTS AND BUILD_TESTING)
	# The screens check: every document loaded over a started world, with no problem counted.
	add_executable(manifest_client_screens_tests tests/ScreensTests.cpp)
	target_link_libraries(manifest_client_screens_tests PRIVATE
		manifest_client_game Manifest::Host Jettison::Vfs manifest_test_helpers doctest::doctest_with_main
		manifest_warnings)
	target_compile_definitions(manifest_client_screens_tests PRIVATE
		MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}"
		MANIFEST_ENGINE_ASSETS="${engine_assets}")
	add_dependencies(manifest_client_screens_tests jettison_assets manifest_validate_pack)
	set_target_properties(manifest_client_screens_tests PROPERTIES FOLDER "Test")
	add_test(NAME client_screens COMMAND manifest_client_screens_tests)

	# The four languages' check: every key English has in French, German and Japanese, the engine's
	# own texts with the client's, naming the same placeholders.
	add_executable(manifest_client_strings_tests tests/StringsTests.cpp)
	target_link_libraries(manifest_client_strings_tests PRIVATE
		Jettison::Ui Jettison::Vfs yaml-cpp::yaml-cpp doctest::doctest_with_main manifest_warnings)
	target_compile_definitions(manifest_client_strings_tests PRIVATE
		MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}"
		MANIFEST_ENGINE_ASSETS="${engine_assets}")
	add_dependencies(manifest_client_strings_tests jettison_assets manifest_validate_pack)
	set_target_properties(manifest_client_strings_tests PROPERTIES FOLDER "Test")
	add_test(NAME client_strings COMMAND manifest_client_strings_tests)

	# The render test's sequence played by the client, offscreen on lavapipe, its game UI stage
	# provisioning and buying through the screens.
	get_target_property(sdk_core Jettison::Core SOURCE_DIR)
	cmake_path(GET sdk_core PARENT_PATH sdk_engine)
	cmake_path(GET sdk_engine PARENT_PATH sdk_source)
	cmake_path(GET sdk_source PARENT_PATH sdk_directory)
	add_test(NAME client_smoke COMMAND "${BASH_PROGRAM}" "${CMAKE_CURRENT_SOURCE_DIR}/client-smoke.sh"
		$<TARGET_FILE:manifest_client> "${sdk_directory}/scripts/lib/headless.sh"
		"${CMAKE_CURRENT_BINARY_DIR}/smoke-user")
	set_tests_properties(client_smoke PROPERTIES TIMEOUT 900)

	# Server play end to end: a dedicated server on a new world, the client offscreen on lavapipe
	# founding the player's company and buying a tote through the screens, and the tote still held
	# after the server's restart.
	add_test(NAME client_serve COMMAND "${BASH_PROGRAM}" "${CMAKE_CURRENT_SOURCE_DIR}/client-serve.sh"
		$<TARGET_FILE:manifest_client> $<TARGET_FILE:manifest> $<TARGET_FILE:manifest-token>
		"${MANIFEST_MOD_DIRECTORY}" "${sdk_directory}/scripts/lib/headless.sh")
	set_tests_properties(client_serve PROPERTIES TIMEOUT 900)
endif()
