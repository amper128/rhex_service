#
# libwfb - WiFiBroadcast
#

add_library(wfb)

file(GLOB_RECURSE libwfb_headers "include/*.h")

target_sources(wfb
	PRIVATE
		fec.c
		radiotap.c
		radiotap_rc.c
		wfb_rx.c
		wfb_tx.c
		wfb_rx_rawsock.c
		wfb_tx_rawsock.c
		${libwfb_headers}
	)

target_include_directories(wfb
	PRIVATE
		include
	PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:usr/include>
	)

target_link_libraries(wfb
	PRIVATE
		-lpcap
		svc
	)
