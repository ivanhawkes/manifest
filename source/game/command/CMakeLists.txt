# The one admission path into the hosted world and every actor's request ledger: ordering, retries
# and structured results (docs/world.md). Commands' rules register their handlers.
manifest_add_module(command
	DEPENDS
		world
	HEADERS
		include/manifest/command/Request.h
		include/manifest/command/Requests.h
	SOURCES
		src/Request.cpp
		src/Requests.cpp
	TESTS
		tests/CommandTests.cpp
	PUBLIC_LIBRARIES
		EnTT::EnTT
		Jettison::Serialise
		Jettison::Sim
	PRIVATE_LIBRARIES
		Jettison::Core
		Jettison::Reflect
)
