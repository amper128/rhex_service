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


add_subdirectory(libsvc)
add_subdirectory(liblog)
add_subdirectory(libwfb)
add_subdirectory(libio)

add_subdirectory(rhex_air)
add_subdirectory(rhex_ground)
