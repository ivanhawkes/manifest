add_executable(manifest main.cpp)
set_target_properties(manifest PROPERTIES FOLDER "Apps")
target_link_libraries(manifest PRIVATE
    Manifest::Host Manifest::Remote Jettison::Core Jettison::Sim Jettison::Time manifest_warnings)
manifest_distributed(manifest)

# The server's CLI for its token file: issues and revokes bearer tokens, keeping their digests.
add_executable(manifest-token token.cpp)
set_target_properties(manifest-token PROPERTIES FOLDER "Apps")
target_link_libraries(manifest-token PRIVATE Manifest::Host Manifest::Remote Jettison::Net manifest_warnings)
manifest_distributed(manifest-token)

# The content validator: the host's options and loading, with no world.
add_executable(manifest-validate validate.cpp)
set_target_properties(manifest-validate PROPERTIES FOLDER "Apps")
target_link_libraries(manifest-validate PRIVATE Manifest::Host Jettison::Core manifest_warnings)
manifest_distributed(manifest-validate)

if(MANIFEST_BUILD_TESTS AND BUILD_TESTING)
    # Run from a directory with nothing of Manifest in it, given absolute paths, with no display:
    # nothing is found from the working directory or needs a window. The content is staged and
    # packed for the host suite (source/game/host).
    set(elsewhere "${CMAKE_CURRENT_BINARY_DIR}/elsewhere")
    file(MAKE_DIRECTORY "${elsewhere}")
    set(env "${CMAKE_COMMAND}" -E env --unset=DISPLAY --unset=WAYLAND_DISPLAY)
    set(run ${env} $<TARGET_FILE:manifest>)
    set(validate ${env} $<TARGET_FILE:manifest-validate>)
    set(directory --content "${MANIFEST_MOD_DIRECTORY}" --scenario trading)
    set(pack --content "${MANIFEST_PACK}" --scenario trading)
    set(overlays "${PROJECT_SOURCE_DIR}/source/game/host/tests/overlays")

    add_test(NAME cli_directory COMMAND ${run} ${directory} --check-startup
        WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME cli_pack COMMAND ${run} ${pack} --check-startup WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME cli_missing_content COMMAND ${run} --content "${elsewhere}/none" --scenario trading
        --check-startup WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME cli_no_arguments COMMAND ${run} WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME cli_no_path COMMAND ${run} --check-startup --scenario trading --content
        WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME cli_no_scenario COMMAND ${run} --content "${MANIFEST_MOD_DIRECTORY}" --check-startup
        WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME cli_no_check COMMAND ${run} ${directory} WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME cli_unknown_argument COMMAND ${run} ${directory} --check-startup --serve
        WORKING_DIRECTORY "${elsewhere}")
    # The defaults: the content beside the executables, and with no HOME no place to write.
    add_test(NAME cli_default_content COMMAND ${run} --scenario trading --check-startup
        WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME validate_default_content COMMAND ${validate} --scenario trading
        WORKING_DIRECTORY "${elsewhere}")
    set(homeless ${env} --unset=HOME --unset=XDG_DATA_HOME --unset=XDG_CONFIG_HOME
        --unset=XDG_STATE_HOME --unset=XDG_CACHE_HOME)
    add_test(NAME cli_serve_no_home COMMAND ${homeless} $<TARGET_FILE:manifest> ${directory} --serve
        WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME token_no_home COMMAND ${homeless} $<TARGET_FILE:manifest-token>
        issue administrator Operator WORKING_DIRECTORY "${elsewhere}")
    # The build tree is laid out as a bundle, and run as one (scripts/bundle-smoke.sh), in a new HOME.
    add_test(NAME bundle_fresh_home COMMAND "${CMAKE_COMMAND}" -E rm -rf "${elsewhere}/home")
    set_tests_properties(bundle_fresh_home PROPERTIES FIXTURES_SETUP fresh_home)
    add_test(NAME bundle_smoke COMMAND "${CMAKE_COMMAND}" -E env "HOME=${elsewhere}/home"
        "${BASH_PROGRAM}" "${PROJECT_SOURCE_DIR}/scripts/bundle-smoke.sh" "${PROJECT_BINARY_DIR}"
        WORKING_DIRECTORY "${elsewhere}")
    set_tests_properties(bundle_smoke PROPERTIES FIXTURES_REQUIRED fresh_home)
    # A world is its build's (D-111), so one left by an earlier build is cleared first.
    add_test(NAME cli_serve_fresh_world COMMAND "${CMAKE_COMMAND}" -E rm -rf "${elsewhere}/world")
    set_tests_properties(cli_serve_fresh_world PROPERTIES FIXTURES_SETUP fresh_world)
    add_test(NAME cli_serve_missing_tokens COMMAND ${run} ${directory} --serve
        --world "${elsewhere}/world" --tokens "${elsewhere}/none.yaml"
        WORKING_DIRECTORY "${elsewhere}")
    set_tests_properties(cli_serve_missing_tokens PROPERTIES FIXTURES_REQUIRED fresh_world)
    add_test(NAME cli_serve COMMAND "${BASH_PROGRAM}" "${CMAKE_CURRENT_SOURCE_DIR}/serve-smoke.sh"
        $<TARGET_FILE:manifest> $<TARGET_FILE:manifest-token> "${MANIFEST_MOD_DIRECTORY}"
        WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME token_revoke_none COMMAND $<TARGET_FILE:manifest-token>
        --tokens "${elsewhere}/none.yaml" revoke principal Ada WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME token_no_arguments COMMAND $<TARGET_FILE:manifest-token>
        WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME cli_broken_overlay COMMAND ${run} ${directory} --overlay "${overlays}/blank-age"
        --check-startup WORKING_DIRECTORY "${elsewhere}")

    # The validator agrees with the host: the same options pass, and fail with the same report.
    add_test(NAME validate_directory COMMAND ${validate} ${directory} WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME validate_pack COMMAND ${validate} ${pack} WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME validate_overlay COMMAND ${validate} ${directory} --overlay "${overlays}/copper"
        WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME validate_missing_content COMMAND ${validate} --content "${elsewhere}/none"
        --scenario trading WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME validate_broken_overlay COMMAND ${validate} ${directory}
        --overlay "${overlays}/blank-age" WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME validate_unknown_scenario COMMAND ${validate} --content "${MANIFEST_MOD_DIRECTORY}"
        --scenario elsewhere WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME validate_no_arguments COMMAND ${validate} WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME validate_unknown_argument COMMAND ${validate} ${directory} --check-startup
        WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME validate_no_manifest_path COMMAND ${validate} ${directory} --manifest
        WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME validate_unwritable_manifest COMMAND ${validate} ${directory}
        --manifest "${elsewhere}/none/trading.manifest" WORKING_DIRECTORY "${elsewhere}")

    set_tests_properties(cli_no_arguments cli_no_path cli_no_scenario cli_no_check
        cli_unknown_argument token_no_arguments validate_no_arguments validate_unknown_argument
        validate_no_manifest_path
        PROPERTIES WILL_FAIL TRUE)
    # Each failure is the one expected, not a crash: only a failed start-up logs its phase.
    set(missing "Start-up failed in the mount phase, at ${elsewhere}/none")
    set(blank "age-stone.yaml: Manifest.Age 'AgeStone': displayName is required \\(from manifest, \
BlankAge \\(patch\\)\\)")
    set_tests_properties(cli_missing_content validate_missing_content
        PROPERTIES PASS_REGULAR_EXPRESSION "${missing}")
    set_tests_properties(cli_default_content PROPERTIES PASS_REGULAR_EXPRESSION
        "ready, from ${MANIFEST_PACK}, scenario trading")
    set_tests_properties(validate_default_content PROPERTIES PASS_REGULAR_EXPRESSION
        "mount 1: manifest, from ${MANIFEST_PACK}.*valid: scenario trading")
    set_tests_properties(cli_serve_no_home token_no_home PROPERTIES PASS_REGULAR_EXPRESSION
        "XDG_[A-Z]+_HOME is unset or relative, and so is HOME")
    set_tests_properties(cli_broken_overlay validate_broken_overlay
        PROPERTIES PASS_REGULAR_EXPRESSION "${blank}")
    set_tests_properties(cli_serve_missing_tokens
        PROPERTIES PASS_REGULAR_EXPRESSION "the token file ${elsewhere}/none.yaml cannot be read")
    set_tests_properties(token_revoke_none PROPERTIES PASS_REGULAR_EXPRESSION "principal Ada has no token")
    set_tests_properties(validate_unknown_scenario
        PROPERTIES PASS_REGULAR_EXPRESSION "there is no scenario 'elsewhere'")
    set_tests_properties(validate_unwritable_manifest PROPERTIES PASS_REGULAR_EXPRESSION
        "the manifest could not be written to ${elsewhere}/none/trading.manifest")
    set_tests_properties(validate_directory validate_pack PROPERTIES PASS_REGULAR_EXPRESSION
        "mount 1: manifest, from .*valid: scenario trading, game mode manifest, content [0-9a-f]+")
    set_tests_properties(validate_overlay PROPERTIES PASS_REGULAR_EXPRESSION
        "mount 2: Copper, from ${overlays}/copper.*valid: scenario trading")
endif()
