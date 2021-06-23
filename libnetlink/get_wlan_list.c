/**
 * @file get_wlan_list.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Получение списка беспроводных интерфейсов (аналог iw dev)
 */

#include <libnetlink.h>
#include <linux/if_arp.h>
#include <linux/nl80211.h>

#include <log/log.h>
#include <netlink/netlink.h>

#include <private/nl.h>

int
nl_get_wlan_list(if_desc_t if_list[])
{
	int result = 0;

	do {
		struct {
			struct nlmsghdr hdr;
			struct genlmsghdr msg;
		} req;

		memset(&req, 0, sizeof(req));

		req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct genlmsghdr));
		req.hdr.nlmsg_flags = (NLM_F_REQUEST | NLM_F_ACK | NLM_F_DUMP);

		req.msg.cmd = NL80211_CMD_GET_INTERFACE;

		uint32_t seq_id;

		if (nl80211_request(&req.hdr, &seq_id) < 0) {
			result = -1;
			break;
		}

		size_t if_count = 0U;

		while (result >= 0) {
			uint8_t *buf;
			int r = nl80211_recv(&buf, seq_id, false);
			if (r < 0) {
				result = -1;
				break;
			}

			uint32_t msg_len = (uint32_t)r;
			struct nlmsghdr *nlmsg_ptr;
			nlmsg_ptr = (struct nlmsghdr *)buf;
			while (NLMSG_OK(nlmsg_ptr, msg_len)) {
				if (nlmsg_ptr->nlmsg_type == NLMSG_DONE) {
					break;
				}

				struct genlmsghdr *msg_ptr;

				msg_ptr = NLMSG_DATA(nlmsg_ptr);

				if (msg_ptr->cmd != NL80211_CMD_NEW_INTERFACE) {
					nlmsg_ptr = NLMSG_NEXT(nlmsg_ptr, msg_len);
					continue;
				}

				struct rtattr *attr_ptr;
				uint32_t attr_len;

				attr_ptr = genlmsg_data(msg_ptr);
				attr_len = nlmsg_ptr->nlmsg_len - NLMSG_LENGTH(sizeof(*msg_ptr));

				int ifindex;

				while (RTA_OK(attr_ptr, attr_len)) {
					if (attr_ptr->rta_type == NL80211_ATTR_IFINDEX) {
						ifindex = *((int *)RTA_DATA(attr_ptr));
					}
					if (attr_ptr->rta_type == NL80211_ATTR_IFNAME) {
						if (if_count < NL_MAX_IFACES) {
							strncpy(if_list[if_count].ifname,
								(char *)RTA_DATA(attr_ptr),
								IFNAMSIZ - 1U);
							if_list[if_count].ifi_index = ifindex;
							if_count++;
							result = if_count;
						}
					}

					attr_ptr = RTA_NEXT(attr_ptr, attr_len);
					attr_len =
					    nlmsg_ptr->nlmsg_len - NLMSG_LENGTH(sizeof(*msg_ptr));
				}

				nlmsg_ptr = NLMSG_NEXT(nlmsg_ptr, msg_len);
			}

			if (nlmsg_ptr->nlmsg_type == NLMSG_DONE) {
				break;
			}

			if (nlmsg_ptr->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *err = NLMSG_DATA(nlmsg_ptr);
				result = err->error;
				if (result) {
					break;
				}
			}
		}
	} while (false);

	return result;
}
