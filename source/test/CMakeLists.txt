# Cross-module suites belong here, not in the effects type module.
add_executable(manifest_effects_tests EffectsIntegrationTests.cpp)
target_link_libraries(manifest_effects_tests PRIVATE Manifest::Game manifest_test_helpers doctest::doctest_with_main manifest_warnings)
target_compile_options(manifest_effects_tests PRIVATE -ffp-contract=off)
target_compile_definitions(manifest_effects_tests PRIVATE MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}")
add_dependencies(manifest_effects_tests manifest_mod)
add_test(NAME effects COMMAND manifest_effects_tests)

# The trading fixture's whole loop, from the directory and the pack (W09).
add_executable(manifest_fixture_tests FixtureTests.cpp)
target_link_libraries(manifest_fixture_tests PRIVATE Manifest::Game manifest_host Manifest::Space manifest_test_helpers doctest::doctest_with_main manifest_warnings)
target_compile_options(manifest_fixture_tests PRIVATE -ffp-contract=off)
target_compile_definitions(manifest_fixture_tests PRIVATE
    MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}"
    MANIFEST_MOD_PACK="${MANIFEST_PACK}"
    MANIFEST_FIXTURE_OVERLAY="${CMAKE_CURRENT_SOURCE_DIR}/overlays/quick-batches"
    MANIFEST_TRADER_OVERLAY="${CMAKE_CURRENT_SOURCE_DIR}/overlays/one-trader"
    MANIFEST_FREIGHTER_OVERLAY="${CMAKE_CURRENT_SOURCE_DIR}/overlays/freighter")
add_dependencies(manifest_fixture_tests manifest_validate_pack)
# Its longest cases are suites of their own, run beside the rest; the rest is every case they do
# not match, so a case added or renamed still runs.
set(MANIFEST_FIXTURE_CRUISE "*cruise fast-forwarded is the one stepped*")
set(MANIFEST_FIXTURE_ALONE "*no client trade alone*")
set(MANIFEST_FIXTURE_PRICES "*shortage pulls a cargo in*,*Eight days of two traders*")
set(MANIFEST_FIXTURE_BURN "*On the manned drive*")
set(MANIFEST_FIXTURE_TRAFFIC "*neighbouring stations of one orbit*")
set(MANIFEST_FIXTURE_GOVERNANCE "*governance fixture's project*")
set(MANIFEST_FIXTURE_DEVELOPMENT "*development fixture's homeworld*")
add_test(NAME fixture COMMAND manifest_fixture_tests
    "--test-case-exclude=${MANIFEST_FIXTURE_CRUISE},${MANIFEST_FIXTURE_ALONE},${MANIFEST_FIXTURE_PRICES},${MANIFEST_FIXTURE_BURN},${MANIFEST_FIXTURE_TRAFFIC},${MANIFEST_FIXTURE_GOVERNANCE},${MANIFEST_FIXTURE_DEVELOPMENT}")
add_test(NAME fixture_cruise COMMAND manifest_fixture_tests "--test-case=${MANIFEST_FIXTURE_CRUISE}")
add_test(NAME fixture_alone COMMAND manifest_fixture_tests "--test-case=${MANIFEST_FIXTURE_ALONE}")
add_test(NAME fixture_prices COMMAND manifest_fixture_tests "--test-case=${MANIFEST_FIXTURE_PRICES}")
add_test(NAME fixture_burn COMMAND manifest_fixture_tests "--test-case=${MANIFEST_FIXTURE_BURN}")
add_test(NAME fixture_traffic COMMAND manifest_fixture_tests "--test-case=${MANIFEST_FIXTURE_TRAFFIC}")
add_test(NAME fixture_governance COMMAND manifest_fixture_tests "--test-case=${MANIFEST_FIXTURE_GOVERNANCE}")
add_test(NAME fixture_development COMMAND manifest_fixture_tests "--test-case=${MANIFEST_FIXTURE_DEVELOPMENT}")

add_executable(manifest_data_tests DataTests.cpp)
target_link_libraries(manifest_data_tests PRIVATE Manifest::Game manifest_test_helpers manifest_warnings)
target_compile_definitions(manifest_data_tests PRIVATE DOCTEST_CONFIG_IMPLEMENT)
target_compile_options(manifest_data_tests PRIVATE -ffp-contract=off)
add_dependencies(manifest_data_tests manifest_mod)
add_test(NAME data COMMAND manifest_data_tests "${MANIFEST_MOD_DIRECTORY}")

add_test(NAME packaging COMMAND "${BASH_PROGRAM}" "${CMAKE_CURRENT_SOURCE_DIR}/packaging-refusals.sh"
    "${MANIFEST_DATA_SOURCE_DIR}")

add_executable(manifest_mod_tests ModTests.cpp)
target_link_libraries(manifest_mod_tests PRIVATE Jettison::Vfs yaml-cpp::yaml-cpp manifest_test_helpers doctest::doctest_with_main manifest_warnings)
target_compile_definitions(manifest_mod_tests PRIVATE
    MANIFEST_MOD_DIRECTORY="${MANIFEST_MOD_DIRECTORY}"
    MANIFEST_MOD_PACK="${MANIFEST_PACK}"
    MANIFEST_HAS_CLIENT=$<BOOL:${MANIFEST_BUILD_CLIENT}>)
add_dependencies(manifest_mod_tests manifest_validate_pack)
add_test(NAME mod COMMAND manifest_mod_tests)

# The SDK import boundary under a parent that provides the SDK (H01): configures only, no build.
add_test(NAME sdk_import COMMAND "${CMAKE_COMMAND}"
    "-DMANIFEST_ROOT=${PROJECT_SOURCE_DIR}" "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/sdk-import"
    "-DGENERATOR=${CMAKE_GENERATOR}" "-DCXX=${CMAKE_CXX_COMPILER}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/sdk-import/check.cmake")
set_target_properties(manifest_effects_tests manifest_fixture_tests manifest_data_tests manifest_mod_tests PROPERTIES FOLDER "Test")
