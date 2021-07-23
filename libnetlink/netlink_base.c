/**
 * @file netlink_base.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Основные функции работы с netlink
 */

#include <linux/rtnetlink.h>
#include <string.h>
#include <sys/socket.h>

#include <log/log.h>

#include <private/nl.h>

static int nl_socket = -1;
static int gennl_socket = -1;
static uint32_t nl_seq_id = 0U;

#define BUF_SIZE (8192)
static uint8_t local_buf[BUF_SIZE];

static int
init_netlink(int type)
{
	int sock;

	do {
		sock = socket(AF_NETLINK, SOCK_RAW, type);
		if (sock < 0) {
			break;
		}

		struct sockaddr_nl local;

		memset(&local, 0, sizeof(local));

		local.nl_family = AF_NETLINK;
		local.nl_pid = (uint32_t)getpid();

		int b;

		b = bind(sock, (struct sockaddr *)&local, sizeof(local));
		if (b < 0) {
			close(sock);
			sock = 1;
			break;
		}

		uint32_t opt = BUF_SIZE;
		setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt));
		setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));
		opt = 1U;
		setsockopt(sock, SOL_NETLINK, NETLINK_EXT_ACK, &opt, sizeof(opt));

		switch (type) {
		case NETLINK_GENERIC:
			gennl_socket = sock;
			break;

		case NETLINK_ROUTE:
		default:
			nl_socket = sock;
			break;
		}
	} while (false);

	return sock;
}

int
read_netlink_recv(int type, uint8_t **buf, uint32_t seq_id, bool wait_confirm)
{
	int result = 0;
	do {
		int sock = nl_socket;
		if (type == NETLINK_GENERIC) {
			sock = gennl_socket;
		}

		if (sock == -1) {
			result = -1;
			break;
		}

		struct iovec iov;
		memset(&iov, 0, sizeof(iov));
		iov.iov_base = local_buf;
		iov.iov_len = BUF_SIZE;
		*buf = local_buf;

		struct sockaddr_nl kernel;
		memset(&kernel, 0, sizeof(kernel));
		kernel.nl_family = AF_NETLINK;
		kernel.nl_pid = (uint32_t)getpid();
		kernel.nl_groups = UINT32_MAX;

		struct msghdr msg;
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_name = &kernel;
		msg.msg_namelen = sizeof(kernel);

		int msg_len;
		msg_len = recvmsg(sock, &msg, 0);

		if (msg_len <= 0) {
			break;
		}

		struct nlmsghdr *nlmsg_ptr;
		nlmsg_ptr = (struct nlmsghdr *)local_buf;

		if (nlmsg_ptr->nlmsg_type == NLMSG_ERROR) {
			struct nlmsgerr *err = NLMSG_DATA(nlmsg_ptr);
			result = err->error;
			break;
		}

		if (nlmsg_ptr->nlmsg_seq != seq_id) {
			continue;
		}

		if (nlmsg_ptr->nlmsg_type == NLMSG_DONE) {
			break;
		}

		result = msg_len;

		if (!wait_confirm) {
			break;
		}
	} while (true);

	return result;
}

int
send_netlink_request(int type, struct nlmsghdr *hdr, uint32_t *seq_id)
{
	int result = 0;

	do {
		int sock = nl_socket;
		if (type == NETLINK_GENERIC) {
			sock = gennl_socket;
		}

		if (sock == -1) {
			int r = init_netlink(type);
			if (r < 0) {
				result = r;
				break;
			}
			sock = r;
		}

		struct sockaddr_nl kernel;
		struct msghdr msg;
		struct iovec iov;

		struct {
			struct nlmsghdr hdr;
			struct rtgenmsg gen;
		} request;

		memset(&kernel, 0, sizeof(kernel));
		memset(&msg, 0, sizeof(msg));
		memset(&request, 0, sizeof(request));

		kernel.nl_family = AF_NETLINK;

		hdr->nlmsg_seq = nl_seq_id;
		*seq_id = nl_seq_id;
		iov.iov_base = hdr;
		iov.iov_len = hdr->nlmsg_len;

		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_name = &kernel;
		msg.msg_namelen = sizeof(kernel);

		if (sendmsg(sock, (struct msghdr *)&msg, 0) < 0) {
			result = -1;
		}
	} while (false);

	return result;
}

int
netlink_request(struct nlmsghdr *hdr, uint32_t *seq_id)
{
	return send_netlink_request(NETLINK_ROUTE, hdr, seq_id);
}

int
netlink_recv(uint8_t **buf, uint32_t seq_id, bool wait_confirm)
{
	return read_netlink_recv(NETLINK_ROUTE, buf, seq_id, wait_confirm);
}
