project(prosense-obs-stitcher)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/prosense-obs-stitcher-config.h.in"
	"${CMAKE_BINARY_DIR}/plugins/prosense-obs-stitcher/config/prosense-obs-stitcher-config.h")

set(prosense-obs-stitcher_config_HEADERS
	"${CMAKE_BINARY_DIR}/plugins/prosense-obs-stitcher/config/prosense-obs-stitcher-config.h")

set(prosense-obs-stitcher_SOURCES
	prosense-obs-stitcher.c)

add_library(prosense-obs-stitcher MODULE
	${prosense-obs-stitcher_SOURCES}
	${prosense-obs-stitcher_config_HEADERS})
target_link_libraries(prosense-obs-stitcher
	libobs)

install_obs_plugin_with_data(prosense-obs-stitcher data)
