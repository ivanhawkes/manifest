# Jettison feature-base, pinned rather than following main or a moving branch.
# Updating this commit and the development shell is one SDK change. Fetch the engine
# first: EnTT, yaml-cpp and doctest must be its targets, never second copies here.
set(MANIFEST_JETTISON_REVISION 66a9ce588546c99bea6a0156cd11b059d2c29b5e)
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

# The Japanese table's font (docs/client.md, "Languages"): Noto's own published subset of Noto Sans JP,
# so nothing is subset here and no font is committed (SIL OFL 1.1, whose licence is fetched with it and
# travels with the bundle). Chosen over subsetting the full font because the shell's fonts take a
# ready file and no font tool is in the development shell. The pack stages it under fonts/.
if(MANIFEST_BUILD_CLIENT)
    set(MANIFEST_NOTO_CJK_TAG Sans2.004)
    set(MANIFEST_NOTO_CJK_URL https://raw.githubusercontent.com/notofonts/noto-cjk/${MANIFEST_NOTO_CJK_TAG})
    FetchContent_Declare(notosansjp-font
        URL ${MANIFEST_NOTO_CJK_URL}/Sans/SubsetOTF/JP/NotoSansJP-Regular.otf
        URL_HASH SHA256=dff723ba59d57d136764a04b9b2d03205544f7cd785a711442d6d2d085ac5073
        DOWNLOAD_NO_EXTRACT TRUE)
    FetchContent_Declare(notosansjp-licence
        URL ${MANIFEST_NOTO_CJK_URL}/LICENSE
        URL_HASH SHA256=6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2
        DOWNLOAD_NO_EXTRACT TRUE)
    FetchContent_MakeAvailable(notosansjp-font notosansjp-licence)
    set(MANIFEST_NOTO_SANS_JP "${notosansjp-font_SOURCE_DIR}/NotoSansJP-Regular.otf" PARENT_SCOPE)
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
# The graphical client is the engine's shell with Manifest composed into it.
if(MANIFEST_BUILD_CLIENT)
    list(APPEND manifest_sdk_required Jettison::App Jettison::Ui jettison_assets)
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
