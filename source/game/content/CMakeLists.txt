# The engine-loaded definitions, their validation as one set and its identity. Above config,
# universe, progression, sim and effects, which never depend back on it
# (docs/stage-2-content-plan.md).
manifest_add_module(content
	DEPENDS
		config
		universe
		progression
		sim
		effects
	HEADERS
		include/manifest/content/Content.h
		include/manifest/content/ContentIdentity.h
		include/manifest/content/Definitions.h
	SOURCES
		src/Content.cpp
		src/ContentIdentity.cpp
		src/Definitions.cpp
	TESTS
		tests/ContentTests.cpp
	PUBLIC_LIBRARIES
		EnTT::EnTT
		Jettison::Serialise
		Jettison::Vfs
	PRIVATE_LIBRARIES
		Jettison::Core
		Jettison::Dock
		Jettison::Reflect
		Jettison::Sim
)
if(TARGET manifest_content_tests)
	target_link_libraries(manifest_content_tests PRIVATE Jettison::Dock Jettison::Reflect Jettison::Sim yaml-cpp::yaml-cpp)
	target_compile_definitions(manifest_content_tests PRIVATE
		MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}"
		MANIFEST_MOD_PACK="${PROJECT_BINARY_DIR}/manifest.pack")
	add_dependencies(manifest_content_tests manifest_pack)
endif()
