# What an actor may see of the hosted world, as value snapshots at a completed step
# (docs/stage-3-authority-plan.md, "Queries and tooling").
manifest_add_module(query
	DEPENDS
		world
		command
	HEADERS
		include/manifest/query/Query.h
	SOURCES
		src/Query.cpp
	TESTS
		tests/QueryTests.cpp
	PUBLIC_LIBRARIES
		Jettison::Sim
	PRIVATE_LIBRARIES
		Jettison::Core
)
if(TARGET manifest_query_tests)
	target_link_libraries(manifest_query_tests PRIVATE manifest_company Jettison::Vfs)
	target_compile_definitions(manifest_query_tests PRIVATE MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}")
	add_dependencies(manifest_query_tests manifest_validate_pack)
endif()
