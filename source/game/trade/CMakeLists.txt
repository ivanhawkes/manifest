# The hosted market: orders reserving against owners' accounts and holdings at a venue, matched and
# settled in one planned write, their expiry, and each venue's broker (docs/stage-4-trading-plan.md,
# "Order and settlement contract").
manifest_add_module(trade
	DEPENDS
		content
		world
		command
		sim
		types
	HEADERS
		include/manifest/trade/Trade.h
	SOURCES
		src/Trade.cpp
	TESTS
		tests/TradeTests.cpp
	PUBLIC_LIBRARIES
		Jettison::Sim
	PRIVATE_LIBRARIES
		Jettison::Core
		Jettison::Reflect
)
if(TARGET manifest_trade_tests)
	target_link_libraries(manifest_trade_tests PRIVATE manifest_company Jettison::Vfs)
	target_compile_definitions(manifest_trade_tests PRIVATE MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}")
	add_dependencies(manifest_trade_tests manifest_validate_pack)
endif()
