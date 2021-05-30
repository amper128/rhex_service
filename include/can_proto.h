/**
 * @file can_proto.h
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2019
 * @brief Описание протокола обмена по CAN-шине
 */

#pragma once

#include <stdint.h>

/**
 * @brief Структура идентификатора CAN-сообщения
 *
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
|28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13|12|11|10|09|08|07|06|05|04|03|02|01|00|
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
|RS|RS|RS|ER|TY|        адресат        |      отправитель      |        команда        |
+--+--+--+--+--+-----------------------+-----------------------+-----------------------+
 *
 */
typedef struct {
	uint8_t cmd_id;	      /**< @brief id команды */
	uint8_t src_id;	      /**< @brief адрес отправителя сообщения */
	uint8_t dest_id;      /**< @brief адрес получателя сообщения */
	uint8_t msg_type : 1; /**< @brief тип сообщения - запрос/ответ */
	uint8_t msg_err : 1;  /**< @brief возврат ошибки */
	uint8_t __reserved : 3;
	uint8_t __not_used : 3;
} __attribute__((packed)) can_msg_t;

#define MSG_BROADCAST (0x0U) /**< @brief широковещательный адрес */

#define MSG_TYPE_REQUEST (0x0U)	 /**< @brief тип сообщения - запрос */
#define MSG_TYPE_RESPONSE (0x1U) /**< @brief тип сообщения - ответ */
#define MSG_TYPE_ERROR (0x1U)	 /**< @brief флаг ошибки */

/**
 * @brief Список ID команд, общие для всех модулей
 */
#define MSG_ID_GET_NAME1 (0x00u) /**< @brief получить первые 8 символов идентификатора */
#define MSG_ID_GET_NAME2                                                                                                                                        \
	(0x01u) /**< @brief получить вторые 8 символов идентификатора. Идентификатор представлен в \
		   виде 16 символов ANSI, если он короче 16 символов - окончание дополняется                \
		   символами '\0' */
#define MSG_ID_GET_VER (0x02u) /**< @brief версия аппаратуры [1 байт], версия ПО [3 байта] */
#define MSG_ID_GET_STATUS (0x11u) /**< @brief получить состояние модуля */
#define MSG_ID_SET_STATE (0x12u)  /**< @brief изменить состояние модуля */

/**
 * @brief Команды управления приводами
 */
#define MSG_ID_DRV_CALIBRATION_START (0x20U) /**< @brief запуск калибровки */
#define MSG_ID_DRV_LEG_MOTION (0x30U)	     /**< @brief движение привода */
#define MSG_ID_DRV_LEG_STAND (0x31U)	     /**< @brief парковка привода */

#define MOTION_TYPE_POS (0x0U) /**< @brief движение к указанной позиции */
#define MOTION_TYPE_ROLLING (0x1U) /**< @brief вращение с заданной скоростью */

/**
 * @brief Параметры команды движения
 */
struct motion_cmd_t {
	uint8_t drive;	     /**< @brief номер привода */
	uint8_t motion_type; /**< @brief тип движения */
	uint16_t position;   /**< @brief позиция */
	int16_t speed;	     /**< @brief скорость движения */
};
