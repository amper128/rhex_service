## define rhex_ground service

file(GLOB_RECURSE rhex_ground_headers "include/*.h")

add_executable(rhex_ground
	main.c
	qgc_forward.c
	rhex_control.c
	rhex_telemetry_rx.c
	rhex_tx_rc.c
	rssi_rx.c
	video.c
	${rhex_ground_headers}
	)

target_include_directories(rhex_ground
	PRIVATE
		include
	)

target_link_libraries(rhex_ground
	-lm
	-lrt
	svc
	log
	wfb
	io
	netlink
	)
