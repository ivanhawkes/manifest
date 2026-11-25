# The hosted world's state on the engine's SimWorld: owner identities, accounts, regions and the
# counters new IDs come from (docs/world.md). Above content, whose scenario seeds it.
manifest_add_module(world
	DEPENDS
		content
	HEADERS
		include/manifest/world/Identities.h
		include/manifest/world/State.h
		include/manifest/world/World.h
	SOURCES
		src/Identities.cpp
		src/State.cpp
		src/World.cpp
	TESTS
		tests/WorldTests.cpp
	PUBLIC_LIBRARIES
		EnTT::EnTT
		Jettison::Sim
	PRIVATE_LIBRARIES
		Jettison::Core
		Jettison::Reflect
)
if(TARGET manifest_world_tests)
	target_link_libraries(manifest_world_tests PRIVATE Jettison::Vfs)
	target_compile_definitions(manifest_world_tests PRIVATE MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}")
	add_dependencies(manifest_world_tests manifest_validate_pack)
endif()
