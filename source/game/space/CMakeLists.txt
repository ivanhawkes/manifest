# The space setting, which Jettison keeps none of (Jettison's docs/history/space-extraction.md): star
# systems, their orbits and bodies, and the galaxy, as data; a star system as a physics frame; and space
# transit over the engine's transit, portals and slots: cruising across a system, lanes between systems,
# the gates that send ships down them, and routes over the galaxy. Below content, which defines the
# scenario's systems and drives in it.
manifest_add_module(space
	HEADERS
		include/manifest/space/Cruise.h
		include/manifest/space/Frames.h
		include/manifest/space/Galaxy.h
		include/manifest/space/Gates.h
		include/manifest/space/Lanes.h
		include/manifest/space/Orbit.h
		include/manifest/space/Routes.h
		include/manifest/space/StarSystem.h
		include/manifest/space/StarSystemFrame.h
		include/manifest/space/Types.h
	SOURCES
		src/Cruise.cpp
		src/Frames.cpp
		src/Galaxy.cpp
		src/Gates.cpp
		src/Lanes.cpp
		src/Orbit.cpp
		src/Routes.cpp
		src/StarSystem.cpp
		src/StarSystemFrame.cpp
		src/Types.cpp
	TESTS
		tests/CelestialTests.cpp
		tests/ContactTests.cpp
		tests/CruiseTests.cpp
		tests/FlightTests.cpp
		tests/GalaxyTests.cpp
		tests/GatesTests.cpp
		tests/Halcyon.h
		tests/LanesTests.cpp
		tests/RoutesTests.cpp
		tests/StarSystemTests.cpp
		tests/Systems.h
	PUBLIC_LIBRARIES
		Jettison::Ai
		Jettison::Core
		Jettison::Math
		Jettison::Physics
		Jettison::Profile
		Jettison::Reflect
		Jettison::Serialise
		Jettison::Sim
		Jettison::Spatial
		Jettison::Time
		Jettison::Transit
		Jettison::Vfs
		glm
		EnTT::EnTT
)
if(TARGET manifest_space_tests)
	# The lanes order themselves after the docks by the system's name, with no link between them; the
	# suite attaches both to show nothing orders them by chance. The system and galaxy files' own
	# tests read the fixtures.
	target_link_libraries(manifest_space_tests PRIVATE Jettison::Dock)
	target_compile_definitions(manifest_space_tests PRIVATE
		MANIFEST_SPACE_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
endif()
