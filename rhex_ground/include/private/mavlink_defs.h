/**
 * @file mavlink_defs.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Заголовки Mavlink чтобы не тащить его всего
 */

#pragma once

#include <stdint.h>

#define MAVLINK_STX 253

#define MAVLINK_MAX_PAYLOAD_LEN (255U) ///< Maximum payload length

#define MAVLINK_CORE_HEADER_LEN (9U) ///< Length of core header (of the comm. layer)

#define MAVLINK_NUM_CHECKSUM_BYTES (2U)

#define MAVLINK_SIGNATURE_BLOCK_LEN (13U)

#define MAVLINK_MSG_ID_HEARTBEAT (0U)
#define MAVLINK_COMMAND_LONG (76U)
#define MAVLINK_MSG_ID_COMMAND_ACK (77U)

#define MAVLINK_MSG_ID_COMMAND_ACK_CRC (143U)

#define _MAV_PAYLOAD(msg) ((const char *)(&((msg)->payload64[0])))

#define X25_INIT_CRC 0xffffU

typedef enum OPENHD_CMD {
	OPENHD_CMD_GET_CAMERA_SETTINGS =
	    11200, /* Get Open.HD camera settings |Reserved (all remaining params)|  */
	OPENHD_CMD_SET_CAMERA_SETTINGS = 11201, /* Set Open.HD camera settings |Brightness level|
						   Contrast level| Saturation level|  */
	OPENHD_CMD_SET_GPIOS = 11300,	   /* Set Open.HD GPIO state |Pin bitpattern to set|  */
	OPENHD_CMD_GET_GPIOS = 11301,	   /* Get Open.HD GPIO state | */
	OPENHD_CMD_POWER_SHUTDOWN = 11400, /* Safe shutdown target system | */
	OPENHD_CMD_POWER_REBOOT = 11401,   /* Reboot target system | */
	OPENHD_CMD_GET_STATUS_MESSAGES =
	    11500, /* Get all status messages that have been stored by the target component | */
	OPENHD_CMD_GET_VERSION = 11501, /* Get the Open.HD version of the target system | */
	OPENHD_CMD_GET_FREQS = 11600,	/* Get system frequency | */
	OPENHD_CMD_SET_FREQS = 11601,	/* Set system frequency |Wifi Frequency|  */
	OPENHD_CMD_ENUM_END = 11602,	/*  | */
} OPENHD_CMD;

typedef enum MAV_CMD_ACK {
	MAV_CMD_ACK_OK = 1, /* Command / mission item is ok. |Reserved (default:0)| Reserved
			       (default:0)| Reserved (default:0)| Reserved (default:0)| Reserved
			       (default:0)| Reserved (default:0)| Reserved (default:0)|  */
	MAV_CMD_ACK_ERR_FAIL =
	    2, /* Generic error message if none of the other reasons fails or if no detailed error
		  reporting is implemented. |Reserved (default:0)| Reserved (default:0)| Reserved
		  (default:0)| Reserved (default:0)| Reserved (default:0)| Reserved (default:0)|
		  Reserved (default:0)|  */
	MAV_CMD_ACK_ERR_ACCESS_DENIED =
	    3, /* The system is refusing to accept this command from this source / communication
		  partner. |Reserved (default:0)| Reserved (default:0)| Reserved (default:0)|
		  Reserved (default:0)| Reserved (default:0)| Reserved (default:0)| Reserved
		  (default:0)|  */
	MAV_CMD_ACK_ERR_NOT_SUPPORTED =
	    4, /* Command or mission item is not supported, other commands would be accepted.
		  |Reserved (default:0)| Reserved (default:0)| Reserved (default:0)| Reserved
		  (default:0)| Reserved (default:0)| Reserved (default:0)| Reserved (default:0)|  */
	MAV_CMD_ACK_ERR_COORDINATE_FRAME_NOT_SUPPORTED =
	    5, /* The coordinate frame of this command / mission item is not supported. |Reserved
		  (default:0)| Reserved (default:0)| Reserved (default:0)| Reserved (default:0)|
		  Reserved (default:0)| Reserved (default:0)| Reserved (default:0)|  */
	MAV_CMD_ACK_ERR_COORDINATES_OUT_OF_RANGE =
	    6, /* The coordinate frame of this command is ok, but he coordinate values exceed the
		  safety limits of this system. This is a generic error, please use the more
		  specific error messages below if possible. |Reserved (default:0)| Reserved
		  (default:0)| Reserved (default:0)| Reserved (default:0)| Reserved (default:0)|
		  Reserved (default:0)| Reserved (default:0)|  */
	MAV_CMD_ACK_ERR_X_LAT_OUT_OF_RANGE =
	    7, /* The X or latitude value is out of range. |Reserved (default:0)| Reserved
		  (default:0)| Reserved (default:0)| Reserved (default:0)| Reserved (default:0)|
		  Reserved (default:0)| Reserved (default:0)|  */
	MAV_CMD_ACK_ERR_Y_LON_OUT_OF_RANGE =
	    8, /* The Y or longitude value is out of range. |Reserved (default:0)| Reserved
		  (default:0)| Reserved (default:0)| Reserved (default:0)| Reserved (default:0)|
		  Reserved (default:0)| Reserved (default:0)|  */
	MAV_CMD_ACK_ERR_Z_ALT_OUT_OF_RANGE =
	    9, /* The Z or altitude value is out of range. |Reserved (default:0)| Reserved
		  (default:0)| Reserved (default:0)| Reserved (default:0)| Reserved (default:0)|
		  Reserved (default:0)| Reserved (default:0)|  */
	MAV_CMD_ACK_ENUM_END = 10, /*  | */
} MAV_CMD_ACK;

typedef struct __attribute__((packed)) __mavlink_message {
	uint8_t magic;		///< protocol magic marker
	uint8_t len;		///< Length of payload
	uint8_t incompat_flags; ///< flags that must be understood
	uint8_t compat_flags;	///< flags that can be ignored if not understood
	uint8_t seq;		///< Sequence of packet
	uint8_t sysid;		///< ID of message sender system/aircraft
	uint8_t compid;		///< ID of the message sender component
	uint32_t msgid : 24;	///< ID of message in payload
	uint64_t payload64[(MAVLINK_MAX_PAYLOAD_LEN + MAVLINK_NUM_CHECKSUM_BYTES + 7) / 8];
	uint8_t ck[2]; ///< incoming checksum bytes
	uint8_t signature[MAVLINK_SIGNATURE_BLOCK_LEN];
} mavlink_message_t;

typedef struct __attribute__((packed)) __mavlink_heartbeat_t {
	uint32_t custom_mode; /*<  A bitfield for use for autopilot-specific flags*/
	uint8_t type; /*<  Vehicle or component type. For a flight controller component the vehicle
			 type (quadrotor, helicopter, etc.). For other components the component type
			 (e.g. camera, gimbal, etc.). This should be used in preference to component
			 id for identifying the component type.*/
	uint8_t autopilot;     /*<  Autopilot type / class. Use MAV_AUTOPILOT_INVALID for components
				  that are not flight controllers.*/
	uint8_t base_mode;     /*<  System mode bitmap.*/
	uint8_t system_status; /*<  System status flag.*/
	uint8_t mavlink_version; /*<  MAVLink version, not writable by user, gets added by protocol
				    because of magic data type: uint8_t_mavlink_version*/
} mavlink_heartbeat_t;

typedef struct __attribute__((packed)) __mavlink_longcmd_t {
	float param1;
	float param2;
	float param3;
	float param4;
	float param5;
	float param6;
	float param7;
	uint16_t command;
	uint8_t target_system;
	uint8_t target_component;
	uint8_t confirmation;
} mavlink_longcmd_t;

typedef struct __attribute__((packed)) __mavlink_cmd_ack_t {
	uint16_t command;
	uint8_t result;
	uint8_t progress;
	int32_t result_param2;
	uint8_t target_system;
	uint8_t target_component;
} mavlink_cmd_ack_t;

/**
 * @brief Accumulate the X.25 CRC by adding one char at a time.
 *
 * The checksum function adds the hash of one char at a time to the
 * 16 bit checksum (uint16_t).
 *
 * @param data new char to hash
 * @param crcAccum the already accumulated checksum
 **/
static inline void
crc_accumulate(uint8_t data, uint16_t *crcAccum)
{
	/*Accumulate one byte of data into the CRC*/
	uint8_t tmp;

	tmp = data ^ (uint8_t)(*crcAccum & 0xffU);
	tmp ^= (tmp << 4U);
	*crcAccum = (*crcAccum >> 8U) ^ (tmp << 8U) ^ (tmp << 3U) ^ (tmp >> 4U);
}

/**
 * @brief Initiliaze the buffer for the X.25 CRC
 *
 * @param crcAccum the 16 bit X.25 CRC
 */
static inline void
crc_init(uint16_t *crcAccum)
{
	*crcAccum = X25_INIT_CRC;
}

/**
 * @brief Calculates the X.25 checksum on a byte buffer
 *
 * @param  pBuffer buffer containing the byte array to hash
 * @param  length  length of the byte array
 * @return the checksum over the buffer bytes
 **/
static inline uint16_t
crc_calculate(const uint8_t *pBuffer, uint16_t length)
{
	uint16_t crcTmp;
	crc_init(&crcTmp);
	while (length--) {
		crc_accumulate(*pBuffer++, &crcTmp);
	}
	return crcTmp;
}

/**
 * @brief Accumulate the X.25 CRC by adding an array of bytes
 *
 * The checksum function adds the hash of one char at a time to the
 * 16 bit checksum (uint16_t).
 *
 * @param data new bytes to hash
 * @param crcAccum the already accumulated checksum
 **/
static inline void
crc_accumulate_buffer(uint16_t *crcAccum, const char *pBuffer, uint16_t length)
{
	const uint8_t *p = (const uint8_t *)pBuffer;
	while (length--) {
		crc_accumulate(*p++, crcAccum);
	}
}
