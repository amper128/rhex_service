/**
 * @file motion.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции планирования движения
 */

#include <canbus.h>
#include <log.h>
#include <motion.h>
#include <rhex_rc.h>
#include <sharedmem.h>
#include <svc_context.h>
#include <timerfd.h>

#include <math.h>
#include <string.h>

static shm_t rc_shm;

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

typedef enum {
	MOTION_STATE_OFF,
	MOTION_STATE_CALIBRATING,
	MOTION_STATE_PARKING,
	MOTION_STATE_BEGIN_WALKING_FRONT,
	MOTION_STATE_BEGIN_WALKING_BACK,
	MOTION_STATE_WALKING_FRONT,
	MOTION_STATE_WALKING_BACK,
	MOTION_STATE_STOP_WALKING_FRONT,
	MOTION_STATE_STOP_WALKING_BACK,
	MOTION_STATE_TURNING_LEFT,
	MOTION_STATE_TURNING_RIGHT,
	MOTION_STATE_WALKING_FL,
	MOTION_STATE_WALKING_FR,
	MOTION_STATE_WALKING_BL,
	MOTION_STATE_WALKING_BR
} motion_state_t;

typedef enum { LEG_LEFT, LEG_RIGHT } leg_side_d;

typedef enum { DRV_STATE_OFF, DRV_STATE_READY, DRV_STATE_CALIBRATING } drv_state_t;

typedef struct {
	drv_state_t state;
	leg_side_d side;
	float pos;
	float phase;
	float phase_offset;
} drv_status_t;

static motion_state_t motion_state = MOTION_STATE_OFF;
static motion_state_t tgt_motion_state = MOTION_STATE_OFF;

static drv_status_t drv_status[6U];

struct motion_data_t {
	uint16_t min_val;
	uint16_t max_val;
};

#define FULL_CIRCLE (768.0f)
#define TICKS (100)
#define MAX_SPEED (1536)

#define PARKING_PHASE (0.25f)

static struct motion_data_t down_phase_start = {310U, 360U};
static struct motion_data_t down_phase_end = {140U, 100U};

static inline float
fdiff(float v1, float v2)
{
	float d = fabs(v2 - v1);
	if (d > 384.0f) {
		d = 768.0f - d;
	}

	return d;
}

static inline float
flimit(float val, float max, float min)
{
	float result = val;

	if (result > max) {
		result = max;
	}
	if (result < min) {
		result = min;
	}

	return result;
}

static void
make_step(float speed, float steering)
{
	speed = flimit(speed, 1.0f, -1.0f);
	steering = flimit(steering, 1.0f, -1.0f);

	float phase_start =
	    (float)down_phase_start.min_val +
	    (((float)down_phase_start.max_val - (float)down_phase_start.min_val) * speed);
	float phase_end = (float)down_phase_end.min_val +
			  (((float)down_phase_end.max_val - (float)down_phase_end.min_val) * speed);

	static float global_phase = 0.25f;

	/* замедление в случае поворота */
	float turn_factor = 0.0f;
	float slowdown = 1.0f;
	if (fabs(steering) > 0.0f) {
		if (fabs(steering / speed) < 2.0f) {
			turn_factor = (steering / speed) / 2.0f;
			slowdown = 1 + fabs(turn_factor);
		}
	}
	turn_factor = steering;

	static const float turn_sd_max = 0.5f;
	float sd_start_l = 0.0f;
	float sd_end_l = 0.5f;
	float sd_start_r = 0.0f;
	float sd_end_r = 0.5f;

	if (turn_factor > 0.0f) {
		sd_start_r = (turn_factor * turn_sd_max) / 2.0f;
		sd_end_r = 0.5f - sd_start_r;
	} else {
		sd_start_l = (-turn_factor * turn_sd_max) / 2.0f;
		sd_end_l = 0.5f - sd_start_l;
	}

	/* крутим фазу движения */
	global_phase += (speed / slowdown) / (float)TICKS;
	if (global_phase > 1.0f) {
		global_phase -= 1.0f;
	}
	if (global_phase < 0.0f) {
		global_phase += 1.0f;
	}

	size_t d;
	for (d = 0U; d < 6U; d++) {
		float leg_phase = -(global_phase + drv_status[d].phase_offset) + 0.5f;
		/* ограничиваем от -0.5 до 0.5 */
		if (leg_phase > 0.5f) {
			leg_phase -= 1.0f;
		}
		if (leg_phase < -0.5f) {
			leg_phase += 1.0f;
		}

		float lp1 = leg_phase;
		/* смещение фазы для поворота */
		if (drv_status[d].side == LEG_LEFT) {
			if (lp1 > 0.0f) {
				leg_phase = 2.0f * lp1 * (sd_end_l - sd_start_l);
			} else {
				leg_phase = 2.0f * lp1 * (1.0f - (sd_end_l - sd_start_l));
			}
			leg_phase += sd_start_l;
		} else {
			if (lp1 > 0.0f) {
				leg_phase = 2.0f * lp1 * (sd_end_r - sd_start_r);
			} else {
				leg_phase = 2.0f * lp1 * (1.0f - (sd_end_r - sd_start_r));
			}
			leg_phase += sd_start_r;
		}

		/* ограничиваем от -0.5 до 0.5 */
		if (leg_phase > 0.5f) {
			leg_phase -= 1.0f;
		}
		if (leg_phase < -0.5f) {
			leg_phase += 1.0f;
		}

		/* смещение нижней и верхней фаз шагания */
		float phase_pos;
		if (leg_phase > 0.0f) {
			phase_pos = (2.0f * leg_phase) * (phase_start - phase_end);
		} else {
			phase_pos = (2.0f * leg_phase) * (FULL_CIRCLE - (phase_start - phase_end));
		}

		phase_pos += phase_end;

		if (phase_pos > 768.0f) {
			phase_pos -= 768.0f;
		}
		if (phase_pos < 0.0f) {
			phase_pos += 768.0f;
		}

		float m_speed = fabs(fdiff(phase_pos, drv_status[d].pos)) * 105.0f;
		if (m_speed > 1536) {
			m_speed = 1536;
		}
		drv_status[d].pos = phase_pos;

		struct can_packet_t msg = {
		    0,
		};
		struct motion_cmd_t data = {
		    0,
		};

		/* команды шагания */
		data.drive = (uint8_t)(d & 0x1U);
		data.motion_type = MOTION_TYPE_POS;
		if (d == 5U) {
			// FIXME: два задних инвертированы
			data.position = 768U - (uint16_t)drv_status[d].pos;
		} else if (d == 4U) {
			data.position = 768U - (uint16_t)drv_status[d].pos;
		} else {
			data.position = (uint16_t)drv_status[d].pos;
		}
		data.speed = (uint16_t)m_speed;

		msg.msg.src_id = 0x10;
		msg.msg.dest_id = (uint8_t)((d / 2U) + 1U);
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
		struct can_packet_t msg = {
		    0,
		};

		msg.msg.dest_id = d;
		msg.msg.cmd_id = MSG_ID_GET_STATUS;
		msg.msg.src_id = 0x10;
		msg.len = 0U;

		send_can_msg(&msg);
	}
}

static void
parking_all(void)
{
	uint8_t d;

	for (d = 0U; d < 3U; d++) {
		struct can_packet_t msg = {
		    0,
		};

		msg.msg.dest_id = d + 1U;
		msg.msg.cmd_id = MSG_ID_DRV_LEG_STAND;
		msg.msg.src_id = 0x10;
		msg.len = 0x01;
		msg.data[0] = 0x0U;

		send_can_msg(&msg);

		msg.data[0] = 0x1U;
		send_can_msg(&msg);

		drv_status[d * 2U].pos = 192.0f;
		drv_status[(d * 2U) + 1U].pos = 192.0f;

		drv_status[d * 2U].phase = PARKING_PHASE;
		drv_status[(d * 2U) + 1U].phase = PARKING_PHASE;
	}
}

static void
calibrate_all(void)
{
	uint8_t d;

	for (d = 1U; d <= 3U; d++) {
		struct can_packet_t msg = {
		    0,
		};

		uint8_t m = 0U;

		msg.msg.dest_id = d;
		msg.msg.cmd_id = MSG_ID_DRV_CALIBRATION_START;
		msg.msg.src_id = 0x10;
		msg.len = 0x01;
		msg.data[0] = m;

		send_can_msg(&msg);

		m = 1U;
		msg.data[0] = m;

		send_can_msg(&msg);
	}
}

static inline bool
all_drv_ready(void)
{
	bool result = true;
	size_t i;

	for (i = 0U; i < 6U; i++) {
		if (drv_status[i].state != DRV_STATE_READY) {
			result = false;
			break;
		}
	}

	return result;
}

static void
parse_msg(const struct can_packet_t *msg)
{
	log_inf("recv: from=%X, dest=%x, cmd=%x, data_len=%u", msg->msg.src_id, msg->msg.dest_id,
		msg->msg.cmd_id, msg->len);

	if (msg->msg.msg_type == MSG_TYPE_RESPONSE) {
		switch (msg->msg.cmd_id) {
		case MSG_ID_GET_STATUS: {
			log_inf("DRV STATUS");

			uint8_t b = msg->msg.src_id - 1U;
			drv_status[b * 2U].state = DRV_STATE_READY;
			drv_status[(b * 2U) + 1U].state = DRV_STATE_READY;

			// FIXME
			if (all_drv_ready()) {
				motion_state = MOTION_STATE_WALKING_FRONT;
				tgt_motion_state = MOTION_STATE_WALKING_FRONT;
				make_step(0.1f, 0.0f);
				usleep(1000000);
			}

			break;
		}

		case MSG_ID_DRV_CALIBRATION_START: {
			log_inf("calibration %u done", msg->data[0]);

			uint8_t b = msg->msg.src_id - 1U;
			uint8_t m = msg->data[0] & 0x1U;
			drv_status[(b * 2U) + m].state = DRV_STATE_READY;

			if (all_drv_ready()) {
				tgt_motion_state = MOTION_STATE_PARKING;
			}

			break;
		}

		case MSG_ID_DRV_LEG_STAND: {
			log_inf("parking %u done", msg->msg.src_id);

			uint8_t b = msg->msg.src_id - 1U;
			uint8_t m = msg->data[0] & 0x1U;
			drv_status[(b * 2U) + m].state = DRV_STATE_READY;

			break;
		}
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

	void *p;
	shm_map_read(&rc_shm, &p);
	rc_data_t *rc_data = p;

	if (tgt_motion_state != motion_state) {
		if (all_drv_ready()) {
			switch (tgt_motion_state) {
			case MOTION_STATE_PARKING:
				parking_all();
				motion_state = MOTION_STATE_PARKING;
				break;

			case MOTION_STATE_CALIBRATING:
				calibrate_all();
				motion_state = MOTION_STATE_CALIBRATING;
				break;

			case MOTION_STATE_BEGIN_WALKING_FRONT:
			case MOTION_STATE_BEGIN_WALKING_BACK:
			case MOTION_STATE_WALKING_FRONT:
			case MOTION_STATE_WALKING_BACK:
			case MOTION_STATE_STOP_WALKING_FRONT:
			case MOTION_STATE_STOP_WALKING_BACK:
			case MOTION_STATE_TURNING_LEFT:
			case MOTION_STATE_TURNING_RIGHT:
			case MOTION_STATE_WALKING_FL:
			case MOTION_STATE_WALKING_FR:
			case MOTION_STATE_WALKING_BL:
			case MOTION_STATE_WALKING_BR:

			case MOTION_STATE_OFF:
			default:
				/* do nothing */
				break;
			}
		}
	}

	switch (motion_state) {
	case MOTION_STATE_OFF:
		/* waiting for a new state */
		break;

	case MOTION_STATE_PARKING:
		/* parking */
		break;

	case MOTION_STATE_CALIBRATING:
		/* waiting for end of calibrating */
		break;

	case MOTION_STATE_BEGIN_WALKING_FRONT:
	case MOTION_STATE_BEGIN_WALKING_BACK:
	case MOTION_STATE_WALKING_FRONT:
	case MOTION_STATE_WALKING_BACK:
	case MOTION_STATE_STOP_WALKING_FRONT:
	case MOTION_STATE_STOP_WALKING_BACK:
	case MOTION_STATE_TURNING_LEFT:
	case MOTION_STATE_TURNING_RIGHT:
	case MOTION_STATE_WALKING_FL:
	case MOTION_STATE_WALKING_FR:
	case MOTION_STATE_WALKING_BL:
	case MOTION_STATE_WALKING_BR:
		if (all_drv_ready()) {
			make_step(rc_data->speed, rc_data->steering);
		}
		break;
	}
}

int
motion_init(void)
{
	/*
	 *  M0 -^- M1
	 *     | |
	 * M2 ----- M3
	 *     | |
	 *  M4 --- M5
	 */

	drv_status[0].side = LEG_LEFT;
	drv_status[0].phase_offset = 0.0f;
	drv_status[1].side = LEG_RIGHT;
	drv_status[1].phase_offset = 0.5f;
	drv_status[2].side = LEG_LEFT;
	drv_status[2].phase_offset = 0.5f;
	drv_status[3].side = LEG_RIGHT;
	drv_status[3].phase_offset = 0.0f;
	drv_status[4].side = LEG_RIGHT;
	drv_status[4].phase_offset = 0.5f;
	drv_status[5].side = LEG_LEFT;
	drv_status[5].phase_offset = 0.0f;

	size_t i;
	for (i = 0U; i < 6U; i++) {
		drv_status[i].state = DRV_STATE_OFF;
	}

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

	shm_map_open("shm_rc", &rc_shm);

	start_msg();

	while (wait_cycle(timerfd)) {
		if (!svc_cycle()) {
			break;
		}
		do_motion();
	}

	return 0;
}
