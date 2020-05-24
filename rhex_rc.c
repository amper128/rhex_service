/**
 * @file rc.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Команды управления
 */

#include <rhex_rc.h>
#include <sharedmem.h>
#include <wfb_rx.h>

static shm_t rc_shm;
static rc_data_t rc_data;


int
rc_init(void)
{
	shm_map_init("shm_rc", sizeof(rc_data_t));

	return 0;
}

int
rc_main(void)
{
	printf("RX R/C Telemetry started\n");

	shm_map_open("shm_rc", &rc_shm);

	monitor_interface_t interfaces[8];
	int num_interfaces = 0;
	int i;
	int result;

	wfb_rx_t rc_rx = {0,};
	const char *if_list[] = {"00e08632035b"};
	result = wfb_rx_init(&rc_rx, 1U, if_list, 30);
	if (result != 0) {
		return result;
	}

	for (;;) {
		#if 0
		packetcounter_ts_now[i] = current_timestamp();
		if (packetcounter_ts_now[i] - packetcounter_ts_prev[i] > 1500) {
			packetcounter_ts_prev[i] = current_timestamp();
			for (i=0; i<num_interfaces; ++i) {
				packetcounter_last[i] = packetcounter[i];
				//packetcounter[i] = rx_status->adapter[i].received_packet_cnt;
				// fprintf(stderr,"counter:%d last:%d   ",packetcounter[i],packetcounter_last[i]);
				if (packetcounter[i] == packetcounter_last[i]) {
				//	rx_status->adapter[i].signal_good = 0;
					// fprintf(stderr,"signal_good[%d]:%d\n",i,rx_status->adapter[i].signal_good);
				} else {
				//	rx_status->adapter[i].signal_good = 1;
					// fprintf(stderr,"signal_good[%d]:%d\n",i,rx_status->adapter[i].signal_good);
				}
			}
		}

		packetcounter_ts_now_rc[i] = current_timestamp();
		if (packetcounter_ts_now_rc[i] - packetcounter_ts_prev_rc[i] > 220) {
			packetcounter_ts_prev_rc[i] = current_timestamp();
			for(i=0; i<num_interfaces; ++i) {
				packetcounter_last_rc[i] = packetcounter_rc[i];
				//packetcounter_rc[i] = rx_status_rc->adapter[i].received_packet_cnt;
				// fprintf(stderr,"counter:%d last:%d   ",packetcounter_rc[i],packetcounter_last_rc[i]);
				if (packetcounter_rc[i] == packetcounter_last_rc[i]) {
				//	rx_status_rc->adapter[i].signal_good = 0;
					// fprintf(stderr,"signal_good[%d]:%d\n",i,rx_status_rc->adapter[i].signal_good);
				} else {
				//	rx_status_rc->adapter[i].signal_good = 1;
					// fprintf(stderr,"signal_good[%d]:%d\n",i,rx_status_rc->adapter[i].signal_good);
				}
			}
		}
		#endif

		struct timeval to;
		to.tv_sec = 0;
		to.tv_usec = 1e5; // 100ms timeout
		fd_set readset;
		FD_ZERO(&readset);
//log_dbg("1");
		for (i = 0; i < rc_rx.count; ++i) {
//log_dbg("fd_set %i", rc_rx.iface[i].selectable_fd);
			FD_SET(rc_rx.iface[i].selectable_fd, &readset);
		}
//log_dbg("2");

		int n = select(30, &readset, NULL, NULL, &to); // TODO: check what the 30 does exactly
		//fprintf(stderr,"n: %d\n",n);
		if (n == 0) {
			continue;
		}
//log_dbg("3");

		for (i = 0; i < rc_rx.count; i++) {
			// if (n == 0) break;

			if (FD_ISSET(rc_rx.iface[i].selectable_fd, &readset)) {
				wfb_rx_packet_t rx_data = {0,};
				//printf("FD_ISSET\n");
				//process_packet(interfaces + i, i,serialport);
				if (wfb_rx_packet(&rc_rx.iface[i], &rx_data)) {
					struct _r {
						uint32_t seqno;
						int16_t rc1;
						int16_t rc2;
						int16_t rc3;
						int16_t rc4;
					};

					union {
						struct _r *r;
						uint8_t *u8;
					} r;

					r.u8 = rx_data.data;

					int p;

					r.r->rc2 -= 1500;
					r.r->rc3 -= 1500;

					//printf("recv rc -%idbm %u, rc2=%i, rc3=%i\n", rx_data.dbm, r.r->seqno, r.r->rc2, r.r->rc3);

					rc_data.speed = (float)r.r->rc2 / 500.0f;
					shm_map_write(&rc_shm, &rc_data, sizeof(rc_data));

					for (p = 0; p < rx_data.bytes; p++) {
						//printf("%02X ", rx_data.data[p]);
					}
					//puts("\n");
				}
			}
		}
	}

	return result;
}
