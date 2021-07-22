/**
 * @file nl80211.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Основные функции работы с netlink
 */

#include <linux/genetlink.h>
#include <linux/if_arp.h>
#include <linux/netlink.h>
#include <linux/nl80211.h>
#include <linux/rtnetlink.h>
#include <linux/types.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <log/log.h>
#include <netlink/netlink.h>

#include <private/nl.h>

static int nl80211fam = -1;

int
nl80211_request(struct nlmsghdr *hdr, uint32_t *seq_id)
{
	int result;

	do {
		if (nl80211fam == -1) {
			uint16_t fam;
			result = nl_get_family("nl80211", &fam);
			if (result) {
				break;
			}

			nl80211fam = (int)fam;
		}

		hdr->nlmsg_type = nl80211fam;

		result = send_netlink_request(NETLINK_GENERIC, hdr, seq_id);
	} while (false);

	return result;
}

int
nl80211_recv(uint8_t **buf, uint32_t seq_id, bool wait_confirm)
{
	return read_netlink_recv(NETLINK_GENERIC, buf, seq_id, wait_confirm);
}
