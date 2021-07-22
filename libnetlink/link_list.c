/**
 * @file link_list.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Получение списка интерфейсов указанного типа
 */

#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <string.h>

#include <log/log.h>
#include <netlink/netlink.h>

#include <private/nl.h>

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
			uint8_t *local_buf;
			int r = netlink_recv(&local_buf, seq_id, false);
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
							result = (int)if_count;
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
nl_get_wlan_rt_list(if_desc_t if_list[])
{
	return nl_link_list(if_list, ARPHRD_IEEE80211_RADIOTAP);
}

int
nl_get_can_list(if_desc_t if_list[])
{
	return nl_link_list(if_list, ARPHRD_CAN);
}
