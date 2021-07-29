/**
 * @file rhex_tx_rc.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Отправка RC команд
 */

#include <arpa/inet.h>
#include <string.h>

#include <log/log.h>
#include <svc/sharedmem.h>
#include <svc/svc.h>
#include <wfb/wfb_status.h>
#include <wfb/wfb_tx.h>

#include <private/rhex_tx_rc.h>

#define PORT (5565)

static wfb_tx_t rc_tx;

int
rhex_tx_rc_init(void)
{
	shm_map_init("shm_tx_status", sizeof(wifibroadcast_tx_status_t));

	return 0;
}

int
rhex_tx_rc_main(void)
{
	int result = 0;

	log_dbg("tx rc start");

	do {
		struct sockaddr_in rc_sockaddr;
		int rc_sock;
		socklen_t slen_rc = sizeof(rc_sockaddr);
		rc_sockaddr.sin_family = AF_INET;
		rc_sockaddr.sin_port = htons(PORT);
		rc_sockaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
		memset(rc_sockaddr.sin_zero, '\0', sizeof(rc_sockaddr.sin_zero));

		if ((rc_sock = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
			log_err("Could not create UDP socket!");
			break;
		}

		if (bind(rc_sock, (struct sockaddr *)&rc_sockaddr, sizeof(struct sockaddr)) == -1) {
			log_err("bind()");
			break;
		}

		result = wfb_tx_init(&rc_tx, 1, false);
		if (result != 0) {
			break;
		}

		uint32_t seqno = 0U;

		while (svc_cycle()) {
			struct timeval to;
			to.tv_sec = 0;
			to.tv_usec = 1e5; // 100ms timeout
			fd_set readset;
			FD_ZERO(&readset);
			FD_SET(rc_sock, &readset);

			result = select(rc_sock + 1, &readset, NULL, NULL, &to);

			if (result > 0) {
				uint8_t rc_data[512];
				int data_len = recvfrom(rc_sock, rc_data, 512, 0,
							(struct sockaddr *)&rc_sockaddr, &slen_rc);

				wfb_tx_send(&rc_tx, seqno++, (uint8_t *)&rc_data, data_len);
			}
		}
	} while (0);

	return result;
}
