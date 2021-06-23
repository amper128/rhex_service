/**
 * @file link_updown.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief управление состоянием интерфейса
 */

#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <string.h>

#include <netlink/netlink.h>

#include <private/nl.h>

static int
nl_link_updown(const if_desc_t *iface, bool up)
{
	int result = 0;

	do {
		struct {
			struct nlmsghdr hdr;
			struct ifinfomsg ifi;
			struct rtattr attr;
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

		uint8_t *local_buf;
		result = netlink_recv(&local_buf, seq_id, true);
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
