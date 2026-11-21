# Standing trade policies and their jobs, run by the world for a player's ship or an NPC's through the
# players' own commands (docs/stage-4-trading-plan.md, "W10 plan").
manifest_add_module(policy
	DEPENDS
		content
		world
		command
		trade
		travel
		company
	HEADERS
		include/manifest/policy/Policy.h
	SOURCES
		src/Policy.cpp
	TESTS
		tests/PolicyTests.cpp
	PUBLIC_LIBRARIES
		Jettison::Sim
	PRIVATE_LIBRARIES
		Jettison::Core
		Jettison::Physics
		Jettison::Reflect
		Jettison::Space
)
if(TARGET manifest_policy_tests)
	target_link_libraries(manifest_policy_tests PRIVATE manifest_query Jettison::Space Jettison::Vfs)
	target_compile_definitions(manifest_policy_tests PRIVATE MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}")
	add_dependencies(manifest_policy_tests manifest_validate_pack)
endif()
