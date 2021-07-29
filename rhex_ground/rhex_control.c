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

#include <private/mavlink_defs.h>
#include <private/rhex_control.h>

#define PORT (5761)

static wfb_tx_t rc_tx;

static uint8_t mavlink_tx_seq;

static void
mavlink_msg_buffer(mavlink_message_t *msg, uint8_t length, uint8_t crc_extra)
{
	uint8_t *buf = (uint8_t *)msg;
	uint8_t header_len = MAVLINK_CORE_HEADER_LEN + 1;

	msg->magic = MAVLINK_STX;
	msg->len = length;
	msg->incompat_flags = 0;
	msg->seq = mavlink_tx_seq++;

	uint16_t checksum = crc_calculate(&buf[1], header_len - 1);
	crc_accumulate_buffer(&checksum, _MAV_PAYLOAD(msg), msg->len);
	crc_accumulate(crc_extra, &checksum);

	buf[header_len + msg->len] = checksum & 0xFFU;
	buf[header_len + msg->len + 1U] = (checksum >> 8U) & 0xFFU;
}

static void
mavlink_ack_msg(int sock, uint16_t cmd, uint8_t system_id, uint8_t comp_id, MAV_CMD_ACK result)
{
	union {
		mavlink_message_t msg;
		uint8_t u8[sizeof(mavlink_message_t)];
	} tx_data;

	union {
		uint8_t *p8;
		mavlink_cmd_ack_t *msg;
	} ack_msg;

	ack_msg.p8 = (uint8_t *)tx_data.msg.payload64;

	memset(&tx_data, 0, sizeof(mavlink_message_t));

	tx_data.msg.msgid = MAVLINK_MSG_ID_COMMAND_ACK;
	tx_data.msg.sysid = system_id;
	tx_data.msg.compid = comp_id;
	ack_msg.msg->command = cmd;
	ack_msg.msg->result = result;

	mavlink_msg_buffer(&tx_data.msg, sizeof(mavlink_cmd_ack_t), MAVLINK_MSG_ID_COMMAND_ACK_CRC);

	int r;
	r = write(sock, tx_data.u8, 22U);
	(void)r;
}

static void
parse_msg(const mavlink_message_t *msg, int sock)
{
	uint8_t *d = (uint8_t *)msg->payload64;

	switch (msg->msgid) {
	case MAVLINK_MSG_ID_HEARTBEAT: {
		/*mavlink_heartbeat_t heartbeat;

		heartbeat.custom_mode = *(uint32_t *)(&d[0U]);
		heartbeat.type = d[4U];
		heartbeat.autopilot = d[5U];
		heartbeat.base_mode = d[6U];
		heartbeat.system_status = d[7U];
		heartbeat.mavlink_version = d[8U];

		log_dbg("hb: %08X %02X %02X %02X %02X %02X", heartbeat.custom_mode, heartbeat.type,
			heartbeat.autopilot, heartbeat.base_mode, heartbeat.system_status,
			heartbeat.mavlink_version);*/
		break;

	case MAVLINK_COMMAND_LONG: {
		union {
			const mavlink_longcmd_t *cmd;
			const uint8_t *p8;
		} cmd;

		cmd.p8 = d;

		switch (cmd.cmd->command) {
		case OPENHD_CMD_POWER_SHUTDOWN:
			log_err("shutdown %i:%i", cmd.cmd->target_system,
				cmd.cmd->target_component);
			mavlink_ack_msg(sock, cmd.cmd->command, cmd.cmd->target_system,
					cmd.cmd->target_component, MAV_CMD_ACK_OK);
			break;

		case OPENHD_CMD_POWER_REBOOT:
			log_err("reboot %i:%i", cmd.cmd->target_system, cmd.cmd->target_component);
			mavlink_ack_msg(sock, cmd.cmd->command, cmd.cmd->target_system,
					cmd.cmd->target_component, MAV_CMD_ACK_OK);
			break;

		default:
			log_dbg("long cmd: %i", cmd.cmd->command);
			break;
		}

		break;
	}
	}

	default:
		log_dbg("msg: %i %i %i %i", msg->len, msg->seq, msg->sysid, msg->msgid);
		break;
	}
}

int
rhex_control_init(void)
{
	return 0;
}

int
rhex_control_main(void)
{
	int result = 0;

	log_dbg("tx rc start");

	do {
		struct sockaddr_in ctl_sockaddr;
		int ctl_sock;
		socklen_t slen_rc = sizeof(ctl_sockaddr);
		ctl_sockaddr.sin_family = AF_INET;
		ctl_sockaddr.sin_port = htons(PORT);
		ctl_sockaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
		memset(ctl_sockaddr.sin_zero, '\0', sizeof(ctl_sockaddr.sin_zero));

		if ((ctl_sock = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1) {
			log_err("Could not create UDP socket!");
			break;
		}

		int32_t opt = 1;
		setsockopt(ctl_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

		if (bind(ctl_sock, (struct sockaddr *)&ctl_sockaddr, sizeof(struct sockaddr)) ==
		    -1) {
			log_err("bind()");
			break;
		}

		if (listen(ctl_sock, 1000000) < 0) {
			log_err("listen()");
			break;
		}

		result = wfb_tx_init(&rc_tx, 1, false);
		if (result != 0) {
			break;
		}

		// uint32_t seqno = 0U;

		int clients[32U];
		uint32_t cmask = 0U;
		struct sockaddr_in clientaddr;
		socklen_t addrlen = sizeof(clientaddr);

		while (svc_cycle()) {
			int c;
			c = accept(ctl_sock, &clientaddr, &addrlen);
			if (c > 0) {
				int cn = __builtin_ctz(~cmask);
				clients[cn] = c;
				cmask |= (1U << cn);

				log_dbg("accept(): fd=%i, cmask=%08X", c, cmask);
			}

			if (cmask > 0U) {
				int fdmax = 0;
				struct timeval to;
				to.tv_sec = 0;
				to.tv_usec = 1e5; // 100ms timeout
				fd_set readset;
				FD_ZERO(&readset);

				size_t i;
				for (i = 0U; i < 32U; i++) {
					if ((cmask & (1U << i)) != 0U) {
						FD_SET(clients[i], &readset);
						if (clients[i] > fdmax) {
							fdmax = clients[i];
						}
					}
				}

				result = select(fdmax + 1, &readset, NULL, NULL, &to);
				if (result > 0) {
					// log_dbg("select: %i", result);
					for (i = 0U; i < 32U; i++) {
						if ((cmask & (1U << i)) == 0U) {
							continue;
						}
						if (!FD_ISSET(clients[i], &readset)) {
							continue;
						}

						union {
							mavlink_message_t msg;
							uint8_t u8[sizeof(mavlink_message_t)];
						} rc_data;
						int data_len = recvfrom(
						    clients[i], rc_data.u8,
						    sizeof(mavlink_message_t), 0,
						    (struct sockaddr *)&ctl_sockaddr, &slen_rc);

						if (data_len < 0) {
							log_dbg("close %i, cmask=%08X", clients[i],
								cmask);
							close(clients[i]);
							cmask &= ~(1U << i);
							continue;
						}

						parse_msg(&rc_data.msg, clients[i]);
					}
				}
			}
		}
	} while (0);

	return result;
}
