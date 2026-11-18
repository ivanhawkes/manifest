# Configures the parent in this directory twice: with the complete SDK Manifest configures, and
# with only Jettison::Math it fails, naming everything else it takes. No fetch is allowed in either.
# cmake -DMANIFEST_ROOT=<source> -DWORK=<dir> -DGENERATOR=<name> -DCXX=<compiler> -P check.cmake
set(expected "The Jettison SDK is incomplete; missing: Jettison::Core, Jettison::Host, Jettison::Vfs, \
jettison_warnings, jettison_pack, EnTT::EnTT, yaml-cpp::yaml-cpp, doctest::doctest, \
doctest::doctest_with_main, Jettison::Devtools, jettison_add_pack().")
foreach(case IN ITEMS complete partial)
    set(partial OFF)
    if(case STREQUAL "partial")
        set(partial ON)
    endif()
    # The partial case asks for the console too, so its target is in the list.
    execute_process(
        COMMAND "${CMAKE_COMMAND}" --fresh -S "${CMAKE_CURRENT_LIST_DIR}" -B "${WORK}/${case}"
            -G "${GENERATOR}" "-DCMAKE_CXX_COMPILER=${CXX}" "-DMANIFEST_ROOT=${MANIFEST_ROOT}"
            -DSDK_IMPORT_PARTIAL=${partial} -DMANIFEST_BUILD_TESTS=ON -DMANIFEST_BUILD_TUI=${partial}
            -DFETCHCONTENT_FULLY_DISCONNECTED=ON
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output)
    string(REGEX REPLACE "[ \n]+" " " flat "${output}")
    if(case STREQUAL "complete" AND NOT result EQUAL 0)
        message(FATAL_ERROR "The complete SDK did not configure:\n${output}")
    endif()
    if(case STREQUAL "partial")
        string(FIND "${flat}" "${expected}" at)
        if(result EQUAL 0 OR at EQUAL -1)
            message(FATAL_ERROR "The partial SDK did not fail as expected (${result}):\n${output}")
        endif()
    endif()
    message(STATUS "${case}: as expected")
endforeach()
