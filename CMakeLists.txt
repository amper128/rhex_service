cmake_minimum_required(VERSION 3.12)

project(rhex C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS YES)

set(OPTIMIZATION_FLAGS
	-O2
	-march=native
	-mtune=native
	#-mfpu=vfp
	#-mfloat-abi=hard
	)

set(GENERIC_DEFINES
	-D_DEFAULT_SOURCE
	-D_GNU_SOURCE
	-D_POSIX_SOURCE
	)

set(GENERIC_C_FLAGS
	-std=c${CMAKE_C_STANDARD}
	-Wall
	-Wextra
	-Werror
	)

set(COMPILER_FLAGS
	${GENERIC_DEFINES}
	${OPTIMIZATION_FLAGS}
	${GENERIC_C_FLAGS}
	)

string (REPLACE ";" " " COMPILER_FLAGS "${COMPILER_FLAGS}")
set(CMAKE_C_FLAGS ${COMPILER_FLAGS})


## define rhex_air service

file(GLOB_RECURSE rhex_air_headers "include/*.h")

add_executable(rhex_air_service
	src/system/crc.c src/system/i2c.c src/system/log.c
	src/system/logger.c src/system/netlink.c src/system/sharedmem.c
	src/system/spi.c src/system/svc.c src/system/timerfd.c
	src/radiotap/radiotap_rc.c
	src/wfb/fec.c src/wfb/wfb_rx.c src/wfb/wfb_tx.c src/wfb/wfb_tx_rawsock.c
	src/sensors/gps.c src/sensors/ina226.c src/sensors/minmea.c src/sensors/sensors.c
	src/control/camera.c src/control/canbus.c src/control/motion.c
	src/control/rhex_rc.c src/control/rhex_telemetry.c src/control/rssi_tx.c
	src/main.c
	${rhex_air_headers}
	)

target_include_directories(rhex_air_service
	PRIVATE
		include
	)

target_link_libraries(rhex_air_service
	-lm
	-lrt
	-lpcap
	)
