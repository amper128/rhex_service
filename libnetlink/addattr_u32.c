/**
 * @file addattr_l.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Добавление атрибута к запросу netlink
 */

#include <netlink/netlink.h>

#include <log/log.h>

#include <private/nl.h>

int
netlink_addattr_u32(struct nlmsghdr *hdr, uint32_t maxlen, uint16_t type, uint32_t value)
{
	int result = 0;

	do {
		uint32_t len = RTA_LENGTH(sizeof(value));
		uint32_t msglen = NLMSG_ALIGN(hdr->nlmsg_len);
		msglen += RTA_ALIGN(len);

		if (msglen > maxlen) {
			log_err("addattr_l ERROR: message exceeded bound of %u", maxlen);
			result = -1;
			break;
		}

		struct rtattr *rta = nlmsg_tail(hdr);
		rta->rta_type = type;
		rta->rta_len = (uint16_t)len;
		memcpy(RTA_DATA(rta), &value, sizeof(value));

		hdr->nlmsg_len = NLMSG_ALIGN(hdr->nlmsg_len);
		hdr->nlmsg_len += RTA_ALIGN(len);
	} while (false);

	return result;
}
