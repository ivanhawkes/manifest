# Linked into each distributed executable (cmake/Distribution.cmake) as objects, not an archive, so
# that the static libstdc++, linked after every archive, finds the wrappers too.
add_library(manifest_glibc_floor OBJECT GlibcFloor.cpp)
target_link_libraries(manifest_glibc_floor PRIVATE manifest_warnings)
# One for each function GlibcFloor.cpp binds.
target_link_options(manifest_glibc_floor INTERFACE
    "LINKER:--wrap=remainder,--wrap=sqrtf,--wrap=__inet_pton_chk")
