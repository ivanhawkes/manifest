# A player's company in the hosted world: its provisioning and its ships' cargo, as commands on the
# one admission path (docs/companies.md, "The commands").
manifest_add_module(company
	DEPENDS
		content
		world
		command
		trade
		travel
	HEADERS
		include/manifest/company/Company.h
	SOURCES
		src/Company.cpp
	TESTS
		tests/CompanyTests.cpp
	PUBLIC_LIBRARIES
		Jettison::Sim
	PRIVATE_LIBRARIES
		Jettison::Core
		Jettison::Physics
		Jettison::Reflect
)
if(TARGET manifest_company_tests)
	target_link_libraries(manifest_company_tests PRIVATE manifest_query Jettison::Vfs)
	target_compile_definitions(manifest_company_tests PRIVATE MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}")
	add_dependencies(manifest_company_tests manifest_validate_pack)
endif()
