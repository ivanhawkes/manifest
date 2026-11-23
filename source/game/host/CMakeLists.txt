# Manifest composed into Jettison's host: its package policy and start-up. Not in Manifest::Game,
# so the model library never depends on the host that will drive it.
manifest_add_module(host
	DEPENDS
		content
		world
		command
		travel
		company
		trade
		policy
		procurement
	HEADERS
		include/manifest/host/Journal.h
		include/manifest/host/Start.h
		include/manifest/host/WorldDirectory.h
	SOURCES
		src/Journal.cpp
		src/Start.cpp
		src/WorldDirectory.cpp
	TESTS
		tests/HostTests.cpp
		tests/JournalTests.cpp
	PUBLIC_LIBRARIES
		Jettison::Core
		Jettison::Host
)

# The trees the build is made from, taken again at every build (cmake/Revision.cmake). The SDK's
# checkout is found from one of its targets, whether it was fetched at the pin or given locally.
find_package(Git REQUIRED)
get_target_property(sdk_directory Jettison::Core SOURCE_DIR)
set(revision "${CMAKE_CURRENT_BINARY_DIR}/Revision.cpp")
add_custom_target(manifest_revision
	COMMAND "${CMAKE_COMMAND}" "-DGIT=${GIT_EXECUTABLE}" "-DGAME=${PROJECT_SOURCE_DIR}"
		"-DSDK=${sdk_directory}" "-DVERSION=${PROJECT_VERSION}"
		"-DWORK=${CMAKE_CURRENT_BINARY_DIR}/revision"
		"-DIN=${CMAKE_CURRENT_SOURCE_DIR}/src/Revision.cpp.in" "-DOUT=${revision}"
		-P "${PROJECT_SOURCE_DIR}/cmake/Revision.cmake"
	BYPRODUCTS "${revision}"
	VERBATIM)
target_sources(manifest_host PRIVATE "${revision}")
add_dependencies(manifest_host manifest_revision)

if(TARGET manifest_host_tests)
	target_link_libraries(manifest_host_tests PRIVATE Jettison::Vfs)
	target_compile_definitions(manifest_host_tests PRIVATE
		MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}"
		MANIFEST_MOD_PACK="${PROJECT_BINARY_DIR}/manifest.pack"
		MANIFEST_HOST_OVERLAYS="${CMAKE_CURRENT_SOURCE_DIR}/tests/overlays")
	# Also what stages the content the manifest executable's checks start from.
	add_dependencies(manifest_host_tests manifest_validate_pack)
endif()
