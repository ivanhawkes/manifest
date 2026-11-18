# Manifest composed into Jettison's host: its package policy and start-up. Not in Manifest::Game,
# so the model library never depends on the host that will drive it.
manifest_add_module(host
	HEADERS
		include/manifest/host/Start.h
	SOURCES
		src/Start.cpp
	TESTS
		tests/HostTests.cpp
	PUBLIC_LIBRARIES
		Jettison::Host
)
if(TARGET manifest_host_tests)
	target_link_libraries(manifest_host_tests PRIVATE Jettison::Vfs)
	target_compile_definitions(manifest_host_tests PRIVATE
		MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}"
		MANIFEST_MOD_PACK="${PROJECT_BINARY_DIR}/manifest.pack")
	# Also what stages the content the manifest executable's checks start from.
	add_dependencies(manifest_host_tests manifest_pack)
endif()
