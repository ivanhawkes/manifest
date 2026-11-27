# Jettison feature-base, pinned rather than following main or a moving branch.
# Updating this commit and the development shell is one SDK change. Fetch the engine
# first: EnTT, yaml-cpp and doctest must be its targets, never second copies here.
set(MANIFEST_JETTISON_REVISION 165fadae596e79c541b1ee19d4470b726088ae44)
set(JETTISON_SOURCE_DIR "" CACHE PATH "Use a local Jettison checkout instead of the pinned revision")
if(NOT TARGET Jettison::Math)
    if(JETTISON_SOURCE_DIR)
        FetchContent_Declare(jettison SOURCE_DIR "${JETTISON_SOURCE_DIR}" EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(jettison
            GIT_REPOSITORY https://github.com/ivanhawkes/jettison.git
            GIT_TAG ${MANIFEST_JETTISON_REVISION}
            EXCLUDE_FROM_ALL)
    endif()
    block()
        set(JETTISON_BUILD_TESTS OFF)
        set(JETTISON_PLANNED_DEPENDENCIES OFF)
        set(JETTISON_INSTALL OFF)
        FetchContent_MakeAvailable(jettison)
    endblock()
endif()

# Everything Manifest takes from the SDK, however it arrived: a parent's partial SDK fails here,
# naming what is missing, instead of at whichever target first links it.
set(manifest_sdk_required Jettison::Ai Jettison::Core Jettison::Dock Jettison::Host
    Jettison::Math Jettison::Net Jettison::Physics Jettison::Platform Jettison::Profile Jettison::Reflect
    Jettison::Serialise Jettison::Sim
    Jettison::Spatial Jettison::Time Jettison::Transit Jettison::Vfs jettison_warnings
    jettison_pack EnTT::EnTT yaml-cpp::yaml-cpp)
if(MANIFEST_BUILD_TESTS AND BUILD_TESTING)
    list(APPEND manifest_sdk_required doctest::doctest doctest::doctest_with_main)
endif()
# The development console takes the engine's, which is built with its presentation; the TUI is on it.
if(MANIFEST_BUILD_TUI)
    list(APPEND manifest_sdk_required Jettison::Devtools)
endif()
set(manifest_sdk_missing "")
foreach(target IN LISTS manifest_sdk_required)
    if(NOT TARGET ${target})
        list(APPEND manifest_sdk_missing ${target})
    endif()
endforeach()
if(NOT COMMAND jettison_add_pack)
    list(APPEND manifest_sdk_missing "jettison_add_pack()")
endif()
if(manifest_sdk_missing)
    list(JOIN manifest_sdk_missing ", " manifest_sdk_missing)
    message(FATAL_ERROR "The Jettison SDK is incomplete; missing: ${manifest_sdk_missing}. "
        "Provide the whole SDK (add_subdirectory of a Jettison checkout), or none, so Manifest "
        "fetches its pin.")
endif()

# FTXUI is only a development frontend; it adds no ABI to the shipped mod or core.
# The engine has no terminal UI dependency. Keep Manifest's existing pin.
if(MANIFEST_BUILD_TUI)
    block()
        set(BUILD_SHARED_LIBS OFF)
        set(FTXUI_BUILD_EXAMPLES OFF)
        set(FTXUI_BUILD_DOCS OFF)
        set(FTXUI_BUILD_TESTS OFF)
        set(FTXUI_ENABLE_INSTALL OFF)
        FetchContent_Declare(ftxui
            GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
            GIT_TAG v7.0.3
            GIT_SHALLOW TRUE
            SYSTEM
            EXCLUDE_FROM_ALL)
        FetchContent_MakeAvailable(ftxui)
    endblock()
endif()
