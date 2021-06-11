#
# libio - Input/Output library
#

add_library(io)

file(GLOB_RECURSE libio_headers "include/*.h")

target_sources(io
	PRIVATE
		canbus.c
		i2c.c
		spi.c
		${libwfb_headers}
	)

target_include_directories(io
	PRIVATE
		include
	PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:usr/include>
	)

target_link_libraries(io
	PUBLIC
		svc
	)
