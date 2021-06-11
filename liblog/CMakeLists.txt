#
# liblog
#

add_library(log)

file(GLOB_RECURSE liblog_headers "include/*.h")

target_sources(log
	PRIVATE
		buffer.c
		create.c
		log.c
		print.c
		put_record.c
		${liblog_headers}
	)

target_include_directories(log
	PRIVATE
		include
	PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:usr/include>
	)
