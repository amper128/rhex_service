/**
 * @file canbus.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции работы с CAN шиной
 */

#include <fcntl.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <string.h>
#include <sys/ioctl.h>

#include <can_proto.h>
#include <canbus.h>
#include <log.h>
#include <netlink.h>
#include <platform.h>

static int can_sock = -1;

int
can_init(void)
{
	int sock = -1;

	struct sockaddr_can addr;

	do {
		if_desc_t can_list[4U];
		int can_count;

		can_count = nl_get_can_list(can_list);
		if (can_count < 0) {
			log_err("cannot get can list");
			break;
		}

		if (can_count == 0) {
			log_err("cannot find can interfaces");
			break;
		}

		if ((sock = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
			log_err("cannot create can socket");
			break;
		}

		addr.can_family = AF_CAN;
		addr.can_ifindex = can_list[0U].ifi_index;

		if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			log_err("bind");
			close(sock);
			sock = -1;
			break;
		}

		/* Change the socket into non-blocking state */
		fcntl(sock, F_SETFL, O_NONBLOCK);

		can_sock = sock;
	} while (0);

	return sock;
}

int
read_can_msg(struct can_packet_t *msg)
{
	int result = 0;

	if (can_sock == -1) {
		log_err("Canbus not initialized!");
	} else {
		struct can_frame frame;

		int r = read(can_sock, &frame, sizeof(struct can_frame));

		while (r > 0) {
			if ((size_t)r < sizeof(struct can_frame)) {
				log_err("read: incomplete CAN frame");
				break;
			}

			if (!(frame.can_id & CAN_EFF_FLAG)) {
				/* skip non-ext frame */
				break;
			}

			union {
				canid_t *can_id;
				can_msg_t *id;
			} id;
			id.can_id = &frame.can_id;

			memcpy(&msg->msg, id.id, sizeof(can_msg_t));
			msg->len = frame.can_dlc;
			memcpy(msg->data, frame.data, msg->len);

			result = 1;

			break;
		}
	}

	return result;
}

int
send_can_msg(struct can_packet_t *msg)
{
	int result = 0;

	if (can_sock == -1) {
		log_err("Canbus not initialized!");
	} else {
		struct can_frame frame;

		union {
			canid_t *can_id;
			can_msg_t *id;
		} id;

		id.can_id = &frame.can_id;
		memcpy(id.id, &msg->msg, sizeof(can_msg_t));
		frame.can_id |= CAN_EFF_FLAG;

		frame.can_dlc = msg->len;

		memcpy(frame.data, msg->data, msg->len);

		if (write(can_sock, &frame, sizeof(struct can_frame)) == sizeof(frame)) {
			result = 1;
		} else {
			log_err("Cannot write CAN frame");
		}
	}

	return result;
}
