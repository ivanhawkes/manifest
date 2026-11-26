# Manifest's protocol on Jettison::Net: the dedicated server's admission and snapshots, the client a
# frontend connects with, the token and key files the server's CLI keeps, and the keys a client pins
# (docs/server.md).
manifest_add_module(remote
	DEPENDS
		query
		host
	HEADERS
		include/manifest/remote/Client.h
		include/manifest/remote/Keys.h
		include/manifest/remote/Protocol.h
		include/manifest/remote/Server.h
		include/manifest/remote/Tokens.h
	SOURCES
		src/Client.cpp
		src/Keys.cpp
		src/Protocol.cpp
		src/Server.cpp
		src/Tokens.cpp
	TESTS
		tests/RemoteTests.cpp
	PUBLIC_LIBRARIES
		Jettison::Net
	PRIVATE_LIBRARIES
		Jettison::Core
		Jettison::Reflect
		Jettison::Serialise
)
if(TARGET manifest_remote_tests)
	target_compile_definitions(manifest_remote_tests PRIVATE MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}"
		MANIFEST_SERVERS="${MANIFEST_SERVERS}")
	add_dependencies(manifest_remote_tests manifest_validate_pack)
endif()
