# The configuration records. Header only: Manifest::Content registers and loads them.
add_library(manifest_config INTERFACE)
add_library(Manifest::Config ALIAS manifest_config)
target_sources(manifest_config INTERFACE
	FILE_SET HEADERS BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
	FILES include/manifest/config/SimulationConfig.h)
