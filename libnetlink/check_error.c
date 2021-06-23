/**
 * @file check_error.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Проверка кода результата netlink запроса
 */

#include <private/nl.h>

int
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
