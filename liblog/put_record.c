/**
 * @file log/put_record.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Добавление записи в лог
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <log/log.h>
#include <private/format.h>
#include <private/print.h>
#include <private/record.h>

#define MSG_BUFF_LEN (512U)

static char tmp_msg_buff[MSG_BUFF_LEN];

static inline uint64_t
log_get_time(void)
{
	struct timespec ts;
	uint64_t result = 0ULL;

	if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
		result = ts.tv_nsec + (ts.tv_sec * TIME_S);
	}

	return result;
}

void
log_put_record(enum log_level level, const char format[], va_list args)
{
	log_buffer_t *log = get_log_buffer();
	if (log) {
		uint64_t tm = log_get_time();
		uint32_t len;
		uint32_t offset = 0;

		len = (uint32_t)vsnprintf(tmp_msg_buff, MSG_BUFF_LEN, format, args);

		struct log_record_t *record;

		while ((len - offset) > LOG_ITEM_MAXMSG) {
			record = &log->records[log->head];
			record->level = level;
			record->date = tm;
			record->msg_len = LOG_ITEM_MAXMSG;
			record->log_continue = LOG_ITEM_CONTINUE;
			memcpy(record->msg, &tmp_msg_buff[offset], LOG_ITEM_MAXMSG);
			offset += LOG_ITEM_MAXMSG;
			log->head = (log->head + 1U) % LOG_BUFFER_SIZE;
		}

		record = &log->records[log->head];
		record->level = level;
		record->date = tm;
		record->msg_len = len - offset;
		record->log_continue = LOG_ITEM_END;
		memcpy(record->msg, &tmp_msg_buff[offset], len - offset);
		log->head = (log->head + 1U) % LOG_BUFFER_SIZE;
	} else {
		msg_print(level, format, args);
	}
}
