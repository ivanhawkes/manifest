add_executable(manifest main.cpp)
set_target_properties(manifest PROPERTIES FOLDER "Apps")
target_link_libraries(manifest PRIVATE Manifest::Host Jettison::Core manifest_warnings)

if(MANIFEST_BUILD_TESTS AND BUILD_TESTING)
    # Run from a directory with nothing of Manifest in it, given absolute paths, with no display:
    # nothing is found from the working directory or needs a window. The content is staged and
    # packed for the host suite (source/game/host).
    set(elsewhere "${CMAKE_CURRENT_BINARY_DIR}/elsewhere")
    file(MAKE_DIRECTORY "${elsewhere}")
    set(run "${CMAKE_COMMAND}" -E env --unset=DISPLAY --unset=WAYLAND_DISPLAY $<TARGET_FILE:manifest>)
    add_test(NAME cli_directory COMMAND ${run} --content "${MANIFEST_MOD_DIRECTORY}" --check-startup
        WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME cli_pack COMMAND ${run} --content "${PROJECT_BINARY_DIR}/manifest.pack" --check-startup
        WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME cli_missing_content COMMAND ${run} --content "${elsewhere}/none" --check-startup
        WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME cli_no_arguments COMMAND ${run} WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME cli_no_path COMMAND ${run} --check-startup --content WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME cli_no_check COMMAND ${run} --content "${MANIFEST_MOD_DIRECTORY}"
        WORKING_DIRECTORY "${elsewhere}")
    add_test(NAME cli_unknown_argument COMMAND ${run} --content "${MANIFEST_MOD_DIRECTORY}"
        --check-startup --serve WORKING_DIRECTORY "${elsewhere}")
    set_tests_properties(cli_no_arguments cli_no_path cli_no_check cli_unknown_argument
        PROPERTIES WILL_FAIL TRUE)
    # The failure is the one expected, not a crash: only a failed start-up logs its phase.
    set_tests_properties(cli_missing_content PROPERTIES
        PASS_REGULAR_EXPRESSION "Start-up failed in the mount phase, at ${elsewhere}/none")
endif()
