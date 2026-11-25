# Ships in space: Jettison's physics, docks, lanes and cruise in the hosted world, its stations and
# the ships parked on their pads, and a ship's journey from one venue to another
# (docs/travel.md). Below company, which parks a new player's ship through it.
manifest_add_module(travel
	DEPENDS
		content
		world
		command
	HEADERS
		include/manifest/travel/Travel.h
	SOURCES
		src/Travel.cpp
	TESTS
		tests/TravelTests.cpp
	PUBLIC_LIBRARIES
		Jettison::Dock
		Jettison::Sim
		Jettison::Transit
	PRIVATE_LIBRARIES
		Jettison::Ai
		Jettison::Core
		Jettison::Physics
		Jettison::Reflect
		Manifest::Space
		Jettison::Spatial
)
if(TARGET manifest_travel_tests)
	target_link_libraries(manifest_travel_tests PRIVATE manifest_host manifest_company Jettison::Vfs)
	target_compile_definitions(manifest_travel_tests PRIVATE
		MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}"
		MANIFEST_MOD_PACK="${MANIFEST_PACK}")
	add_dependencies(manifest_travel_tests manifest_validate_pack)
endif()
