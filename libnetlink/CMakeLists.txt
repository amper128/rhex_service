#
# libnetlink
#

add_library(netlink)

file(GLOB_RECURSE libnetlink_headers "include/*.h")

target_sources(netlink
	PRIVATE
		netlink_base.c
		addattr_l.c
		addattr_u32.c
		check_error.c
		get_family.c
		nl80211.c
		link_updown.c
		link_list.c
		get_wlan_list.c
		wlan_set_monitor.c
		${libnetlink_headers}
	)

target_include_directories(netlink
	PRIVATE
		include
	PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:usr/include>
	)
