#
# libsvc
#

add_library(svc)

file(GLOB_RECURSE libsvc_headers "include/*.h")

target_sources(svc
	PRIVATE
		log.c
		logger.c
		netlink.c
		sharedmem.c
		svc.c
		timerfd.c
		${libsvc_headers}
	)

target_include_directories(svc
	PRIVATE
		include
	PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:usr/include>
	)
