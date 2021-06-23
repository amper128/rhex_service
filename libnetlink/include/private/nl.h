/**
 * @file private/nl.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Основные функции работы с netlink
 */

#pragma once

#include <linux/genetlink.h>
#include <linux/netlink.h>

#include <svc/platform.h>

int netlink_request(struct nlmsghdr *hdr, uint32_t *seq_id);

int nl80211_request(struct nlmsghdr *hdr, uint32_t *seq_id);

int send_netlink_request(int type, struct nlmsghdr *hdr, uint32_t *seq_id);

int read_netlink_recv(int type, uint8_t **buf, uint32_t seq_id, bool wait_confirm);

int netlink_recv(uint8_t **buf, uint32_t seq_id, bool wait_confirm);

int nl80211_recv(uint8_t **buf, uint32_t seq_id, bool wait_confirm);

int netlink_addattr_l(struct nlmsghdr *hdr, uint32_t maxlen, uint16_t type, const void *attrdata,
		      uint32_t attrlen);

int netlink_addattr_u32(struct nlmsghdr *hdr, uint32_t maxlen, uint16_t type, uint32_t value);

int netlink_check_error(struct nlmsghdr *hdr, uint32_t msg_len);

int nl_get_family(const char family[], uint16_t *family_id);

/**
 * gennlmsg_data - head of message payload
 * @gnlh: genetlink message header
 */
static inline void *
genlmsg_data(const struct genlmsghdr *gnlh)
{
	return ((unsigned char *)gnlh + GENL_HDRLEN);
}
