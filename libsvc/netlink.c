/**
 * @file netlink.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Функции работы с netlink
 */

#include <linux/if_arp.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/types.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <svc/netlink.h>
#include <svc/platform.h>

static int nl_socket = -1;
static uint32_t nl_seq_id = 0U;

#define BUF_SIZE (8192)
static uint8_t buf[BUF_SIZE];

static int
init_netlink()
{
	int fd;
	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (fd < 0) {
		return fd;
	}

	struct sockaddr_nl local;

	memset(&local, 0, sizeof(local));

	local.nl_family = AF_NETLINK;
	local.nl_pid = getpid();
	local.nl_groups = 0;

	int b;

	b = bind(fd, (struct sockaddr *)&local, sizeof(local));
	if (b < 0) {
		return fd;
	}

	nl_socket = fd;

	return 0;
}

static int
netlink_recv(uint8_t buf[], uint32_t seq_id, bool wait_confirm)
{
	int result = 0;
	do {
		if (nl_socket == -1) {
			result = -1;
			break;
		}

		struct iovec iov;
		memset(&iov, 0, sizeof(iov));
		iov.iov_base = buf;
		iov.iov_len = BUF_SIZE;

		struct sockaddr_nl kernel;
		memset(&kernel, 0, sizeof(kernel));
		kernel.nl_family = AF_NETLINK;

		struct msghdr msg;
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_name = &kernel;
		msg.msg_namelen = sizeof(kernel);

		int msg_len;
		msg_len = recvmsg(nl_socket, &msg, 0);

		if (msg_len <= 0) {
			break;
		}

		struct nlmsghdr *nlmsg_ptr;
		nlmsg_ptr = (struct nlmsghdr *)buf;

		if (nlmsg_ptr->nlmsg_type == NLMSG_ERROR) {
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

static int
netlink_request(struct nlmsghdr *hdr, uint32_t *seq_id)
{
	int result = 0;

	do {
		if (nl_socket == -1) {
			int r = init_netlink();
			if (r < 0) {
				result = r;
				break;
			}
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

		if (sendmsg(nl_socket, (struct msghdr *)&msg, 0) < 0) {
			result = -1;
		}
	} while (false);

	return result;
}

static int
nl_link_list(if_desc_t if_list[], unsigned short ifi_type)
{
	int result = 0;

	(void)if_list;

	do {
		struct {
			struct nlmsghdr hdr;
			struct rtgenmsg gen;
		} req;

		memset(&req, 0, sizeof(req));

		req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
		req.hdr.nlmsg_type = RTM_GETLINK;
		req.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
		req.hdr.nlmsg_pid = getpid();
		req.gen.rtgen_family = AF_PACKET;

		uint32_t seq_id;

		if (netlink_request(&req.hdr, &seq_id) < 0) {
			result = -1;
			break;
		}

		size_t if_count = 0U;

		while (result >= 0) {
			int msg_len;
			msg_len = netlink_recv(buf, seq_id, false);
			if (msg_len < 0) {
				result = -1;
				break;
			}

			struct nlmsghdr *nlmsg_ptr;
			nlmsg_ptr = (struct nlmsghdr *)buf;
			while (NLMSG_OK(nlmsg_ptr, msg_len)) {
				if (nlmsg_ptr->nlmsg_type == NLMSG_DONE) {
					break;
				}

				if (nlmsg_ptr->nlmsg_type != RTM_NEWLINK) {
					nlmsg_ptr = NLMSG_NEXT(nlmsg_ptr, msg_len);
					continue;
				}

				struct ifinfomsg *ifi_ptr;

				ifi_ptr = NLMSG_DATA(nlmsg_ptr);

				if (ifi_type != ARPHRD_VOID) {
					if (ifi_ptr->ifi_type != ifi_type) {
						nlmsg_ptr = NLMSG_NEXT(nlmsg_ptr, msg_len);
						continue;
					}
				}

				struct rtattr *attr_ptr;
				int attr_len;

				attr_ptr = IFLA_RTA(ifi_ptr);
				attr_len = nlmsg_ptr->nlmsg_len - NLMSG_LENGTH(sizeof(*ifi_ptr));

				while (RTA_OK(attr_ptr, attr_len)) {
					if (attr_ptr->rta_type == IFLA_IFNAME) {
						if (if_count < NL_MAX_IFACES) {
							strncpy(if_list[if_count].ifname,
								(char *)RTA_DATA(attr_ptr),
								IFNAMSIZ - 1U);
							if_list[if_count].ifi_index =
							    ifi_ptr->ifi_index;
							if_count++;
							result = if_count;
						}
					}

					attr_ptr = RTA_NEXT(attr_ptr, attr_len);
					attr_len =
					    nlmsg_ptr->nlmsg_len - NLMSG_LENGTH(sizeof(*ifi_ptr));
				}

				nlmsg_ptr = NLMSG_NEXT(nlmsg_ptr, msg_len);
			}

			if (nlmsg_ptr->nlmsg_type == NLMSG_DONE) {
				break;
			}
		}
	} while (false);

	return result;
}

int
nl_get_eth_list(if_desc_t if_list[])
{
	return nl_link_list(if_list, ARPHRD_ETHER);
}

int
nl_get_wifi_list(if_desc_t if_list[])
{
	return nl_link_list(if_list, ARPHRD_IEEE80211_RADIOTAP);
}

int
nl_get_can_list(if_desc_t if_list[])
{
	return nl_link_list(if_list, ARPHRD_CAN);
}
