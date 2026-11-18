# Cross-module suites belong here, not in the effects type module.
add_executable(manifest_effects_tests EffectsIntegrationTests.cpp)
target_link_libraries(manifest_effects_tests PRIVATE Manifest::Game manifest_test_helpers doctest::doctest_with_main manifest_warnings)
target_compile_options(manifest_effects_tests PRIVATE -ffp-contract=off)
add_test(NAME effects COMMAND manifest_effects_tests)

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
    MANIFEST_MOD_PACK="${PROJECT_BINARY_DIR}/manifest.pack")
add_dependencies(manifest_mod_tests manifest_pack)
add_test(NAME mod COMMAND manifest_mod_tests)

# The SDK import boundary under a parent that provides the SDK (H01): configures only, no build.
add_test(NAME sdk_import COMMAND "${CMAKE_COMMAND}"
    "-DMANIFEST_ROOT=${PROJECT_SOURCE_DIR}" "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/sdk-import"
    "-DGENERATOR=${CMAKE_GENERATOR}" "-DCXX=${CMAKE_CXX_COMPILER}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/sdk-import/check.cmake")
set_target_properties(manifest_effects_tests manifest_data_tests manifest_mod_tests PROPERTIES FOLDER "Test")
