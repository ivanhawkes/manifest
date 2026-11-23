# Regions' public projects and their procurement (W12, docs/stage-4-trading-plan.md, "Tender contract
# (W11)"): a project's inputs gathered, consumed once and worked at its region's workshop, unlocking
# its recipe there.
manifest_add_module(procurement
	DEPENDS
		content
		world
		trade
	HEADERS
		include/manifest/procurement/Procurement.h
	SOURCES
		src/Procurement.cpp
	TESTS
		tests/ProcurementTests.cpp
	PUBLIC_LIBRARIES
		Jettison::Sim
	PRIVATE_LIBRARIES
		Jettison::Core
		Jettison::Profile
)
if(TARGET manifest_procurement_tests)
	target_link_libraries(manifest_procurement_tests PRIVATE manifest_company Jettison::Vfs)
	target_compile_definitions(manifest_procurement_tests PRIVATE MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}")
	add_dependencies(manifest_procurement_tests manifest_validate_pack)
endif()
