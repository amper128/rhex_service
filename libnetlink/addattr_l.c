/**
 * @file addattr_l.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Добавление атрибута к запросу netlink
 */

#include <libnetlink.h>

#include <log/log.h>

#include <private/nl.h>

/* копия функции, чтобы не зависеть от библиотеки netlink */
int
netlink_addattr_l(struct nlmsghdr *hdr, uint32_t maxlen, uint16_t type, const void *attrdata,
		  uint32_t attrlen)
{
	int result = 0;

	do {
		uint32_t len = RTA_LENGTH(attrlen);
		uint32_t msglen = NLMSG_ALIGN(hdr->nlmsg_len);
		msglen += RTA_ALIGN(len);

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
