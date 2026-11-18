# The engine-loaded definitions and the content set's identity. Above config, universe, progression
# and effects, which never depend back on it (docs/stage-2-content-plan.md).
manifest_add_module(content
	DEPENDS
		config
	HEADERS
		include/manifest/content/ContentIdentity.h
		include/manifest/content/Definitions.h
	SOURCES
		src/ContentIdentity.cpp
		src/Definitions.cpp
	TESTS
		tests/ContentTests.cpp
	PUBLIC_LIBRARIES
		EnTT::EnTT
		Jettison::Serialise
	PRIVATE_LIBRARIES
		Jettison::Core
		Jettison::Reflect
		Jettison::Sim
)
if(TARGET manifest_content_tests)
	# The legacy loaders are the equivalence checks' reference until C03 retires them.
	target_link_libraries(manifest_content_tests PRIVATE Manifest::Universe Jettison::Sim Jettison::Vfs)
	target_compile_definitions(manifest_content_tests PRIVATE
		MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}"
		MANIFEST_MOD_PACK="${PROJECT_BINARY_DIR}/manifest.pack")
	add_dependencies(manifest_content_tests manifest_pack)
endif()
