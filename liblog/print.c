/**
 * @file print.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Функции печати логов
 */

#include <stdio.h>

#include <log/read.h>
#include <private/format.h>
#include <private/print.h>

#define PR_RED "\x1B[31m"
#define PR_GRN "\x1B[32m"
#define PR_YEL "\x1B[33m"
#define PR_BLU "\x1B[34m"
#define PR_MAG "\x1B[35m"
#define PR_CYN "\x1B[36m"
#define PR_WHT "\x1B[37m"
#define PR_RES "\x1B[0m"

static const struct {
	const char *color;
	const char *msg;
} levels[] = {
    [LOG_DBG] = {PR_WHT, "dbg"}, [LOG_INF] = {PR_GRN, "inf"}, [LOG_WARN] = {PR_YEL, "wrn"},
    [LOG_ERR] = {PR_RED, "err"}, [LOG_EXC] = {PR_RED, "exc"},
};

static inline void
msg_begin(enum log_level level)
{
	if (isatty(fileno(stderr))) {
		fputs(levels[level].color, stderr);
	}

	fprintf(stderr, "%s: ", levels[level].msg);
}

static inline void
msg_end(void)
{
	if (isatty(fileno(stderr))) {
		fputs(PR_RES, stderr);
	}

	fputc('\n', stderr);
}

void
msg_print(enum log_level level, const char format[], va_list args)
{
	msg_begin(level);
	vfprintf(stderr, format, args);
	msg_end();
}

void
log_print(const char name[], log_buffer_t *log)
{
	do {
		if (log == NULL) {
			break;
		}

		if (log->head == log->tail) {
			break;
		}

		struct log_record_t *record = &log->records[log->tail];
		fprintf(stderr, "[%10s ] ", name);
		msg_begin(record->level);

		do {
			fwrite(record->msg, record->msg_len, 1U, stderr);
			if (record->log_continue == LOG_ITEM_END) {
				break;
			}
			log->tail = (log->tail + 1U) % LOG_BUFFER_SIZE;
			record = &log->records[log->tail];
		} while (log->head != log->tail);

		log->tail = (log->tail + 1U) % LOG_BUFFER_SIZE;

		msg_end();
	} while (log->head != log->tail);
}
