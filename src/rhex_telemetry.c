/**
 * @file rhex_telemetry.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Телеметрия
 */

#include <string.h>

#include <crc.h>
#include <gps.h>
#include <log.h>
#include <sensors.h>
#include <sharedmem.h>
#include <telemetry.h>
#include <timerfd.h>
#include <wfb_tx.h>

static shm_t gps_shm;
static shm_t sensors_shm;
static wfb_tx_t telemetry_tx;

#define X1E7 (10000000)

static void
read_gps_status(vector_telemetry_t *vot)
{
	gps_status_t *gps_status;
	void *p;

	shm_map_read(&gps_shm, &p);
	gps_status = p;

	float conv;
	conv = gps_status->latitude;
	vot->LatitudeX1E7 = (int32_t)(conv * X1E7);

	conv = gps_status->longitude;
	vot->LongitudeX1E7 = (int32_t)(conv * X1E7);

	conv = gps_status->speed;
	vot->GroundspeedKPHX10 = (int16_t)(conv * 10.0);

	conv = gps_status->course;
	vot->CourseDegreesX10 = (int16_t)(conv * 10.0);

	conv = gps_status->altitude;
	vot->GPSAltitudecm = (int32_t)(conv * 100.0);

	conv = gps_status->hdop;
	vot->HDOPx10 = (uint8_t)(conv * 10.0);

	vot->SatsInUse = gps_status->sats_use;
}

static void
read_sensors_status(vector_telemetry_t *vot)
{
	union {
		sensors_status_t *s;
		void *p;
	} p;

	shm_map_read(&sensors_shm, &p.p);

	float conv;
	conv = p.s->angle_x;
	vot->PitchDegrees = (int16_t)(conv * 10.0);
	conv = p.s->angle_y;
	vot->RollDegrees = (int16_t)(conv * 10.0);
	conv = p.s->angle_z;
	vot->YawDegrees = (int16_t)(conv * 10.0);

	conv = p.s->vbat;
	vot->PackVoltageX100 = (int16_t)(conv * 100.0);
}

int
telemetry_init(void)
{
	shm_map_init("shm_gps", sizeof(gps_status_t));
	shm_map_init("shm_sensors", sizeof(sensors_status_t));

	return 0;
}

int
telemetry_main(void)
{
	int result = 0;

	do {
		result = shm_map_open("shm_gps", &gps_shm);
		if (result != 0) {
			break;
		}
		result = shm_map_open("shm_sensors", &sensors_shm);
		if (result != 0) {
			break;
		}

		// FIXME: get a list of interfaces
		const char *if_list[] = {"00e08632035b"};
		result = wfb_tx_init(&telemetry_tx, 1U, if_list, 1);
		if (result != 0) {
			break;
		}

		int timerfd;
		timerfd = timerfd_init(100ULL * TIME_MS, 100ULL * TIME_MS);
		if (timerfd < 0) {
			result = -1;
			break;
		}

		vector_telemetry_t vot;
		memset((uint8_t *)&vot, 0, sizeof(vot));
		vot.StartCode = VOT_SC;

		uint32_t seqno = 0U;

		while (wait_cycle(timerfd)) {
			read_gps_status(&vot);
			read_sensors_status(&vot);

			vot.CRC =
			    vt_crc16((uint8_t *)&vot, offsetof(vector_telemetry_t, CRC), 0xFFFFU);

			wfb_tx_send(&telemetry_tx, seqno++, (uint8_t *)&vot, sizeof(vot));
		}
	} while (0);

	return result;
}
