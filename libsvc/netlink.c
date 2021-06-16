/**
 * @file netlink.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Функции работы с netlink
 */

#include <libnetlink.h>
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

#include <log/log.h>
#include <svc/netlink.h>
#include <svc/platform.h>

static int nl_socket = -1;
static uint32_t nl_seq_id = 0U;

#define BUF_SIZE (8192)
static uint8_t local_buf[BUF_SIZE];

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
	local.nl_pid = (uint32_t)getpid();
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

/* копия функции, чтобы не зависеть от библиотеки netlink */
static int
netlink_addattr_l(struct nlmsghdr *hdr, uint32_t maxlen, uint16_t type, const void *attrdata,
		  uint32_t attrlen)
{
	int result = 0;

	uint32_t len = RTA_LENGTH(attrlen);
	uint32_t msglen = NLMSG_ALIGN(hdr->nlmsg_len);
	msglen += RTA_ALIGN(len);

	do {
		if (msglen > maxlen) {
			log_err("addattr_l ERROR: message exceeded bound of %u", maxlen);
			result = -1;
			break;
		}

		struct rtattr *rta = NLMSG_TAIL(hdr);
		rta->rta_type = type;
		rta->rta_len = (uint16_t)len;
		if (attrlen > 0U) {
			memcpy(RTA_DATA(rta), attrdata, attrlen);
		}

		hdr->nlmsg_len = NLMSG_ALIGN(hdr->nlmsg_len);
		hdr->nlmsg_len += RTA_ALIGN(len);
	} while (false);

	return result;
}

static int
netlink_check_error(struct nlmsghdr *hdr, uint32_t msg_len)
{
	int result = 0;

	struct nlmsghdr *nlmsg_ptr = hdr;
	while (NLMSG_OK(nlmsg_ptr, msg_len)) {
		if (nlmsg_ptr->nlmsg_type != NLMSG_ERROR) {
			nlmsg_ptr = NLMSG_NEXT(nlmsg_ptr, msg_len);
			continue;
		}

		struct nlmsgerr *err = NLMSG_DATA(nlmsg_ptr);
		result = err->error;
		break;
	}

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
		req.hdr.nlmsg_pid = (uint32_t)getpid();
		req.gen.rtgen_family = AF_PACKET;

		uint32_t seq_id;

		if (netlink_request(&req.hdr, &seq_id) < 0) {
			result = -1;
			break;
		}

		size_t if_count = 0U;

		while (result >= 0) {
			int r = netlink_recv(local_buf, seq_id, false);
			if (r < 0) {
				result = -1;
				break;
			}

			uint32_t msg_len = (uint32_t)r;
			struct nlmsghdr *nlmsg_ptr;
			nlmsg_ptr = (struct nlmsghdr *)local_buf;
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
				uint32_t attr_len;

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

static int
nl_link_updown(const if_desc_t *iface, bool up)
{
	int result = 0;

	do {
		struct {
			struct nlmsghdr hdr;
			struct ifinfomsg ifi;
			char ifname[IFNAM_SIZE];
		} req;

		memset(&req, 0U, sizeof(req));

		req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
		req.hdr.nlmsg_flags = (NLM_F_REQUEST | NLM_F_ACK);
		req.hdr.nlmsg_type = RTM_SETLINK;

		req.ifi.ifi_change = IFF_UP;
		if (up) {
			req.ifi.ifi_flags = IFF_UP;
		}
		req.ifi.ifi_family = AF_UNSPEC;

		result = netlink_addattr_l(&req.hdr, sizeof(req), IFLA_IFNAME, iface->ifname,
					   IFNAM_SIZE);
		if (result) {
			break;
		}

		uint32_t seq_id;
		result = netlink_request(&req.hdr, &seq_id);
		if (result < 0) {
			break;
		}

		result = netlink_recv(local_buf, seq_id, true);
		if (result < 0) {
			break;
		}

		struct nlmsghdr *nlmsg_ptr;
		nlmsg_ptr = (struct nlmsghdr *)local_buf;
		result = netlink_check_error(nlmsg_ptr, (uint32_t)result);
	} while (false);

	return result;
}

int
nl_link_up(const if_desc_t *iface)
{
	return nl_link_updown(iface, true);
}

int
nl_link_down(const if_desc_t *iface)
{
	return nl_link_updown(iface, false);
}
