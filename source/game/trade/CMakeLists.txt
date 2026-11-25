# The hosted market: orders reserving against owners' accounts and holdings at a venue, matched and
# settled in one planned write, their expiry, and each venue's broker (docs/trading.md), and the
# regional economy that makes, pays for and consumes goods through it (W04, docs/economy.md).
manifest_add_module(trade
	DEPENDS
		content
		world
		command
		sim
		travel
		types
	HEADERS
		include/manifest/trade/Economy.h
		include/manifest/trade/Trade.h
	SOURCES
		src/Economy.cpp
		src/Trade.cpp
	TESTS
		tests/EconomyTests.cpp
		tests/TradeTests.cpp
	PUBLIC_LIBRARIES
		Jettison::Sim
	PRIVATE_LIBRARIES
		Jettison::Core
		Jettison::Profile
		Jettison::Reflect
		Jettison::Spatial
)
if(TARGET manifest_trade_tests)
	target_link_libraries(manifest_trade_tests PRIVATE manifest_company manifest_travel Jettison::Vfs)
	target_compile_definitions(manifest_trade_tests PRIVATE MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}")
	add_dependencies(manifest_trade_tests manifest_validate_pack)
endif()
