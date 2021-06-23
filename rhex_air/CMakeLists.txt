## define rhex_air service

file(GLOB_RECURSE rhex_air_headers "include/*.h")

add_executable(rhex_air_service
	sensors/gps.c
	sensors/ina226.c
	sensors/minmea.c
	sensors/sensors.c
	control/camera.c
	control/crc.c
	control/motion.c
	control/rhex_rc.c
	control/rhex_telemetry.c
	control/rssi_tx.c
	main.c
	${rhex_air_headers}
	)

target_include_directories(rhex_air_service
	PRIVATE
		include
	)

target_link_libraries(rhex_air_service
	-lm
	-lrt
	svc
	log
	wfb
	io
	netlink
	)
