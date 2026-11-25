# The executables a distribution carries (docs/host.md, "The distribution"), each built into
# <build>/bin, beside the content in <build>/share/manifest, as the bundle lays them out, so a build
# tree finds its content as a bundle does. With MANIFEST_DISTRIBUTION each is linked to run on x86-64
# Linux with glibc 2.39 or newer and nothing else: libstdc++ and libgcc linked in, the system's loader,
# no RUNPATH, and the symbols glibc gave newer versions after 2.39 bound to the older
# (source/apps/distribution). scripts/check-distribution.sh checks the result.
option(MANIFEST_DISTRIBUTION "Link the distributed executables to run outside the build's toolchain" OFF)
set(MANIFEST_LOADER /lib64/ld-linux-x86-64.so.2)
if(MANIFEST_DISTRIBUTION)
    find_program(MANIFEST_PATCHELF patchelf REQUIRED)
endif()

function(manifest_distributed target)
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
    if(NOT MANIFEST_DISTRIBUTION)
        return()
    endif()
    target_link_libraries(${target} PRIVATE manifest_glibc_floor)
    target_link_options(${target} PRIVATE -static-libstdc++ -static-libgcc
        "LINKER:--dynamic-linker=${MANIFEST_LOADER}")
    # Nix's compiler wrapper gives every executable a RUNPATH into its store, which no link option
    # takes back.
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${MANIFEST_PATCHELF}" --remove-rpath "$<TARGET_FILE:${target}>"
        VERBATIM)
endfunction()
