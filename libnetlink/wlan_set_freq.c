/**
 * @file wlan_set_monitor.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Установка режима мониторинга для wlan
 */

#include <linux/nl80211.h>
#include <string.h>

#include <netlink/netlink.h>

#include <private/nl.h>

int
nl_wlan_set_freq(const if_desc_t *iface, uint32_t freq, uint32_t width, uint32_t ht)
{
	int result = 0;

	do {

		struct {
			struct nlmsghdr hdr;
			struct genlmsghdr msg;
			uint8_t data[40U];
		} req;

		memset(&req, 0U, sizeof(req));

		req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct genlmsghdr));
		req.hdr.nlmsg_flags = (NLM_F_REQUEST | NLM_F_ACK);

		req.msg.cmd = NL80211_CMD_SET_WIPHY;

		result = netlink_addattr_u32(&req.hdr, sizeof(req), NL80211_ATTR_IFINDEX,
					     (uint32_t)iface->ifi_index);
		if (result) {
			break;
		}

		result = netlink_addattr_u32(&req.hdr, sizeof(req), NL80211_ATTR_WIPHY_FREQ, freq);
		if (result) {
			break;
		}

		result =
		    netlink_addattr_u32(&req.hdr, sizeof(req), NL80211_ATTR_CHANNEL_WIDTH, width);
		if (result) {
			break;
		}

		result =
		    netlink_addattr_u32(&req.hdr, sizeof(req), NL80211_ATTR_WIPHY_CHANNEL_TYPE, ht);
		if (result) {
			break;
		}

		result =
		    netlink_addattr_u32(&req.hdr, sizeof(req), NL80211_ATTR_CENTER_FREQ1, freq);
		if (result) {
			break;
		}

		uint32_t seq_id;
		result = nl80211_request(&req.hdr, &seq_id);
		if (result < 0) {
			break;
		}

		uint8_t *buf;
		result = nl80211_recv(&buf, seq_id, true);
		if (result < 0) {
			break;
		}

		struct nlmsghdr *nlmsg_ptr;
		nlmsg_ptr = (struct nlmsghdr *)buf;
		result = netlink_check_error(nlmsg_ptr, (uint32_t)result);
	} while (false);

	return result;
}
