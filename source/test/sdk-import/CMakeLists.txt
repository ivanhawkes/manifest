cmake_minimum_required(VERSION 3.28)

# A parent project that provides the SDK itself, as a host game would, with empty stand-ins for the
# engine's targets: configuring Manifest under it is the test. SDK_IMPORT_PARTIAL provides only
# Jettison::Math, the target that stops Manifest fetching its pin.
project(ManifestSdkImport LANGUAGES C CXX)
option(SDK_IMPORT_PARTIAL "Provide only Jettison::Math" OFF)

set(provided Jettison::Math)
if(NOT SDK_IMPORT_PARTIAL)
    list(APPEND provided Jettison::Core Jettison::Dock Jettison::Host Jettison::Net Jettison::Reflect
        Jettison::Serialise Jettison::Sim Jettison::Time
        Jettison::Vfs Jettison::Devtools jettison_warnings
        EnTT::EnTT yaml-cpp::yaml-cpp doctest::doctest doctest::doctest_with_main)
    add_custom_target(jettison_pack)
    function(jettison_add_pack target)
        add_custom_target(${target})
    endfunction()
endif()
foreach(target IN LISTS provided)
    string(MAKE_C_IDENTIFIER "${target}" name)
    add_library(stand_in_${name} INTERFACE)
    add_library(${target} ALIAS stand_in_${name})
endforeach()

add_subdirectory("${MANIFEST_ROOT}" manifest)
