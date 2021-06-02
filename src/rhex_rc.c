/**
 * @file rc.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Команды управления
 */

#include <log.h>
#include <netlink.h>
#include <rhex_rc.h>
#include <sharedmem.h>
#include <svc_context.h>
#include <wfb/wfb_rx.h>
#include <wfb/wfb_status.h>

static shm_t rc_shm;
static shm_t rc_status_shm;

static rc_data_t rc_data;
static wifibroadcast_rx_status_t_rc rc_status;

int
rc_init(void)
{
	shm_map_init("shm_rc", sizeof(rc_data_t));
	shm_map_init("shm_rc_status", sizeof(rc_status_t));

	return 0;
}

int
rc_main(void)
{
	log_inf("RX R/C Telemetry started\n");

	shm_map_open("shm_rc", &rc_shm);
	shm_map_open("shm_rc_status", &rc_status_shm);

	size_t i;

	int result;

	wfb_rx_t rc_rx = {
	    0,
	};

	if_desc_t if_list[4U];
	int if_count;

	if_count = nl_get_wifi_list(if_list);
	if (if_count < 0) {
		result = -1;
		log_err("cannot get wlan list");
		return result;
	}

	result = wfb_rx_init(&rc_rx, (size_t)if_count, if_list, 30);
	if (result != 0) {
		return result;
	}

	uint32_t last_seqno = 0U;

	while (svc_cycle()) {
		struct timeval to;
		to.tv_sec = 0;
		to.tv_usec = 1e5; // 100ms timeout
		fd_set readset;
		FD_ZERO(&readset);

		for (i = 0; i < rc_rx.count; ++i) {
			FD_SET(rc_rx.iface[i].selectable_fd, &readset);
		}

		int n =
		    select(30, &readset, NULL, NULL, &to); // TODO: check what the 30 does exactly

		if (n > 0) {
			rc_status.wifi_adapter_cnt = rc_rx.count;
			rc_status.last_update = time(NULL);
			for (i = 0; i < rc_rx.count; i++) {
				if (FD_ISSET(rc_rx.iface[i].selectable_fd, &readset)) {
					wfb_rx_packet_t rx_data = {
					    0,
					};

					if (wfb_rx_packet(&rc_rx.iface[i], &rx_data)) {
						struct _r {
							uint32_t seqno;
							int16_t res;
							int16_t axis[4];
							int16_t data[4];
							int8_t sq;
						};

						union {
							struct _r *r;
							uint8_t *u8;
						} r;

						r.u8 = rx_data.data;

						if ((uint32_t)(r.r->seqno - last_seqno) <
						    (UINT32_MAX / 2U)) {
							r.r->axis[0] -= 1500;
							r.r->axis[1] -= 1500;

							if ((uint32_t)(r.r->seqno - last_seqno) >
							    1U) {
								rc_status.lost_packet_cnt +=
								    (uint32_t)(r.r->seqno -
									       last_seqno);
							}

							rc_data.speed =
							    (float)r.r->axis[1] / 500.0f;
							rc_data.steering =
							    (float)r.r->axis[0] / 500.0f;
							size_t g;
							for (g = 0; g < 2; g++) {
								size_t bit;
								for (bit = 0; bit < 16; bit++) {
									if (r.r->data[g] &
									    (1U << bit)) {
										rc_data
										    .btn[(16 * g) +
											 bit] =
										    true;
									} else {
										rc_data
										    .btn[(16 * g) +
											 bit] =
										    false;
									}
								}
							}
							shm_map_write(&rc_shm, &rc_data,
								      sizeof(rc_data));

							rc_status.received_packet_cnt++;
						}

						rc_status.adapter[i].current_signal_dbm =
						    rx_data.dbm;
					}
				}
			}
		}

		shm_map_write(&rc_status_shm, &rc_status, sizeof(rc_status));
	}

	return result;
}
