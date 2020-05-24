/**
 * @file motion.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции планирования движения
 */

#include <can_proto.h>
#include <canbus.h>
#include <log.h>
#include <motion.h>
#include <timerfd.h>

#include <math.h>
#include <string.h>

/*
 * Position data:
 *
 *       __ 576 __
 *      /         \
 *    /             \
 *   |               |
 *  384      O       0
 *   |               |
 *    \             /
 *      \__     __/
 *          192
 *
 */

static int drv_ready[2U] = {0, 0};
//static int drv_park[2U] = {0, 0};

struct motion_data_t {
	uint16_t min_val;
	uint16_t max_val;
};

#define FULL_CIRCLE (768.0f)
#define TICKS (100)
#define MAX_SPEED (1536)

static struct motion_data_t down_phase_start = {256U, 320U};
static struct motion_data_t down_phase_end = {128U, 64U};

static float m_phase[2] = {0.25f, 0.75f};

static inline float
fdiff(float v1, float v2)
{
	float d = fabs(v2 - v1);
	if (d > 384.0f) {
		d = 768.0f - d;
	}

	return d;
}

static void
make_step(float speed)
{
	if (speed > 1.0f) {
		speed = 1.0f;
	}

	speed = speed * 1.01f;

	float phase_start = (float)down_phase_start.min_val + (((float)down_phase_start.max_val - (float)down_phase_start.min_val) * speed);
	float phase_end   = (float)down_phase_end.min_val   + (((float)down_phase_end.max_val   - (float)down_phase_end.min_val)   * speed);

	static float pos[2] = {192.0f, 192.0f + 384.0f};

	uint32_t d;
	for (d = 0U; d < 2U; d++) {
		m_phase[d] += (speed * 1.7f) / (float)TICKS;
		if (m_phase[d] > 1.0f) {
			m_phase[d] -= 1.0f;
		}
		if (m_phase[d] < 0.0f) {
			m_phase[d] += 1.0f;
		}

		float phase_pos;

//		if ((pos[d] < phase_start) && (pos[d] > phase_end)) {
		if (m_phase[d] > 0.5f) {
			phase_pos = (2.0f * (m_phase[d] - 0.5f)) * (phase_start - phase_end);
		} else {
			phase_pos = (2.0f * (m_phase[d] - 0.5f)) * (FULL_CIRCLE - (phase_start - phase_end));
		}

		phase_pos += phase_end;

		phase_pos = 384.0f - phase_pos;
		//float m_speed;
		//m_speed = fabs((float)MAX_SPEED * speed);
		float m_speed = fabs(fdiff(phase_pos, pos[d])) * 100.0f;
		if (m_speed > 1536) {
			m_speed = 1536;
		}
//		fprintf(stderr, "speed: %.1f\n", m_speed);
		pos[d] = phase_pos;

		if (pos[d] > 768.0f) {
			pos[d] -= 768.0f;
		}
		if (pos[d] < -768.0f) {
			pos[d] += 768.0f;
		}

		struct can_packet_t msg = {0, };
		struct motion_cmd_t data = {0, };

		data.drive = (uint8_t)d;
		data.motion_type = MOTION_TYPE_POS;
		data.position = (uint16_t)pos[d];
		data.speed = (uint16_t)m_speed;

		msg.msg.src_id = 0x10;
		msg.msg.dest_id = 2U;
		msg.msg.cmd_id = MSG_ID_DRV_LEG_MOTION;
		msg.len = sizeof(data);
		memcpy(msg.data, &data, sizeof(data));
		send_can_msg(&msg);
	}
}

static void
start_msg(void)
{
	uint8_t d;

	for (d = 1U; d <= 3U; d++) {
		struct can_packet_t msg = {0,};

		msg.msg.dest_id = d;
		msg.msg.cmd_id = MSG_ID_GET_STATUS;
		msg.msg.src_id = 0x10;
		msg.len = 0U;

		send_can_msg(&msg);
	}
}

static void
parse_msg(const struct can_packet_t *msg)
{
	log_inf("recv: from=%X, dest=%x, cmd=%x, data_len=%u", msg->msg.src_id, msg->msg.dest_id, msg->msg.cmd_id, msg->len);

	if (msg->msg.msg_type == 1U) {
		if (msg->msg.cmd_id == MSG_ID_GET_STATUS) {
			struct can_packet_t msg2 = {0,};

			uint8_t dest = msg->msg.src_id;
			uint8_t m = 0U;

			msg2.msg.dest_id = dest;
			msg2.msg.cmd_id = MSG_ID_DRV_CALIBRATION_START;
			msg2.msg.src_id = 0x10;
			msg2.len = 0x01;
			msg2.data[0] = m;

			send_can_msg(&msg2);

			m = 1U;
			msg2.data[0] = m;

			send_can_msg(&msg2);
		}

		if (msg->msg.cmd_id == MSG_ID_DRV_CALIBRATION_START) {
			log_inf("calibration %u done", msg->data[0]);

			struct can_packet_t msg2 = {0,};

			msg2.msg.dest_id = msg->msg.src_id;
			msg2.msg.cmd_id = MSG_ID_DRV_LEG_STAND;
			msg2.msg.src_id = 0x10;
			msg2.len = 0x01;
			msg2.data[0] = msg->data[0] & 0x1U;

			send_can_msg(&msg2);

			drv_ready[0] = 1;

		}

		if (msg->msg.cmd_id == MSG_ID_DRV_LEG_STAND) {
			log_inf("parking %u done", msg->msg.src_id);
			drv_ready[0] = 1;
		}
	}
}

static void
do_motion()
{
	struct can_packet_t msg;

	if (read_can_msg(&msg)) {
		parse_msg(&msg);
	}

	if (drv_ready[0] > 0) {
		if (drv_ready[0] == 100) {
			make_step(0.3f);
			//fprintf(stderr, "100\n");
			//usleep(10000000ULL);
		}

		if (drv_ready[0] >= 500) {
			make_step(0.5f);
		}

		if ((drv_ready[0] % 100) == 0) {
			log_dbg("M: %.1f, %.1f", m_phase[0], m_phase[1]);
		}
		drv_ready[0]++;
	}
}

int
motion_init(void)
{
	/* do nothing */

	return 0;
}

int
motion_main(void)
{
	int timerfd;

	if (can_init() < 0) {
		return 1;
	}

	timerfd = timerfd_init(10ULL * TIME_MS, 10ULL * TIME_MS);
	if (timerfd < 0) {
		return 1;
	}

//	start_msg();

	drv_ready[0] = 1;


	while (wait_cycle(timerfd)) {
		do_motion();
	}

	return 0;
}
