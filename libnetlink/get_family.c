/**
 * @file get_family.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Получение family ID по имени системы netlink
 */

#include <linux/genetlink.h>
#include <linux/rtnetlink.h>
#include <string.h>

#include <private/nl.h>

int
nl_get_family(const char family[], uint16_t *family_id)
{
	int result = 0;

	do {
		struct {
			struct nlmsghdr hdr;
			struct genlmsghdr msg;
			struct rtattr attr;
			char family[GENL_NAMSIZ];
		} req;

		memset(&req, 0U, sizeof(req));

		req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct genlmsghdr));
		req.hdr.nlmsg_flags = (NLM_F_REQUEST | NLM_F_ACK);
		req.hdr.nlmsg_type = GENL_ID_CTRL;
		req.hdr.nlmsg_pid = (uint32_t)getpid();

		req.msg.cmd = CTRL_CMD_GETFAMILY;
		req.msg.version = 1U;

		result = netlink_addattr_l(&req.hdr, sizeof(req), CTRL_ATTR_FAMILY_NAME, family,
					   strlen(family) + 1U);
		if (result) {
			break;
		}

		uint32_t seq_id;
		result = send_netlink_request(NETLINK_GENERIC, &req.hdr, &seq_id);
		if (result < 0) {
			break;
		}

		uint8_t *buf;
		result = read_netlink_recv(NETLINK_GENERIC, &buf, seq_id, false);
		if (result < 0) {
			break;
		}

		uint32_t msg_len = (uint32_t)result;
		result = 0;
		struct nlmsghdr *nlmsg_ptr;
		nlmsg_ptr = (struct nlmsghdr *)buf;
		while (NLMSG_OK(nlmsg_ptr, msg_len)) {
			if (nlmsg_ptr->nlmsg_type == NLMSG_DONE) {
				break;
			}

			if (nlmsg_ptr->nlmsg_type != GENL_ID_CTRL) {
				nlmsg_ptr = NLMSG_NEXT(nlmsg_ptr, msg_len);
				continue;
			}

			struct genlmsghdr *genl_ptr;

			genl_ptr = NLMSG_DATA(nlmsg_ptr);

			if (genl_ptr->cmd != CTRL_CMD_NEWFAMILY) {
				nlmsg_ptr = NLMSG_NEXT(nlmsg_ptr, msg_len);
				continue;
			}

			struct rtattr *attr_ptr;
			uint32_t attr_len;

			attr_ptr = IFLA_RTA(genl_ptr);
			attr_len = nlmsg_ptr->nlmsg_len - NLMSG_LENGTH(sizeof(struct genlmsghdr));

			while (RTA_OK(attr_ptr, attr_len)) {
				if (attr_ptr->rta_type == CTRL_ATTR_FAMILY_ID) {
					uint16_t *id_ptr = RTA_DATA(attr_ptr);
					*family_id = *id_ptr;
					break;
				}

				attr_ptr = RTA_NEXT(attr_ptr, attr_len);
				attr_len =
				    nlmsg_ptr->nlmsg_len - NLMSG_LENGTH(sizeof(struct genlmsghdr));
			}

			nlmsg_ptr = NLMSG_NEXT(nlmsg_ptr, msg_len);
		}

	} while (false);

	return result;
}
