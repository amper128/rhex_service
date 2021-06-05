/**
 * @file logger.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Работа с логами
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#include <log.h>
#include <logger.h>
#include <svc_context.h>
#include <timerfd.h>

#define PR_RED "\x1B[31m"
#define PR_GRN "\x1B[32m"
#define PR_YEL "\x1B[33m"
#define PR_BLU "\x1B[34m"
#define PR_MAG "\x1B[35m"
#define PR_CYN "\x1B[36m"
#define PR_WHT "\x1B[37m"
#define PR_RES "\x1B[0m"

#define MSG_BUFF_LEN (512U)

static char tmp_msg_buff[MSG_BUFF_LEN];

static log_buffer_t *log_buffer = NULL;

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

static inline void
msg_log(enum log_level level, const char format[], va_list args)
{
	msg_begin(level);
	vfprintf(stderr, format, args);
	msg_end();
}

void
logger_add(const char name[])
{
	(void)name;
	//
}

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
	if (log_buffer) {
		uint64_t tm = log_get_time();
		uint32_t len;
		uint32_t offset = 0;

		len = (uint32_t)vsnprintf(tmp_msg_buff, MSG_BUFF_LEN, format, args);

		struct log_record_t *record;

		while ((len - offset) > LOG_ITEM_MAXMSG) {
			record = &log_buffer->records[log_buffer->head];
			record->level = level;
			record->date = tm;
			record->msg_len = LOG_ITEM_MAXMSG;
			record->log_continue = LOG_ITEM_CONTINUE;
			memcpy(record->msg, &tmp_msg_buff[offset], LOG_ITEM_MAXMSG);
			offset += LOG_ITEM_MAXMSG;
			log_buffer->head = (log_buffer->head + 1U) % LOG_BUFFER_SIZE;
		}

		record = &log_buffer->records[log_buffer->head];
		record->level = level;
		record->date = tm;
		record->msg_len = len - offset;
		record->log_continue = LOG_ITEM_END;
		memcpy(record->msg, &tmp_msg_buff[offset], len - offset);
		log_buffer->head = (log_buffer->head + 1U) % LOG_BUFFER_SIZE;
	} else {
		msg_log(level, format, args);
	}
}

log_buffer_t *
logger_create(const char name[])
{
	log_buffer_t *log = NULL;

	do {
		char shm_name[256];
		snprintf(shm_name, sizeof(shm_name), "/rhex_log_%s", name);
		int fd = shm_open(shm_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		if (fd < 0) {
			log_err("shm_open() \"%s\" error", name);
			break;
		}

		if (ftruncate(fd, sizeof(log_buffer_t)) == -1) {
			log_err("cannot ftruncate()");
			close(fd);
			break;
		}

		void *map =
		    mmap(NULL, sizeof(log_buffer_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		close(fd);
		if (map == MAP_FAILED) {
			break;
		}

		log = (log_buffer_t *)map;

		log->head = LOG_BUFFER_SIZE - 2U;
		log->tail = LOG_BUFFER_SIZE - 2U;
	} while (false);

	return log;
}

void
logger_init(void)
{
	const svc_context_t *ctx = get_svc_context();
	log_buffer = ctx->log_buffer;
}

log_buffer_t *
get_log_reader(const char name[])
{
	log_buffer_t *result = NULL;

	do {
		char shm_name[256];
		snprintf(shm_name, sizeof(shm_name), "/rhex_log_%s", name);
		int fd = shm_open(shm_name, O_RDWR, S_IRUSR | S_IWUSR);
		if (fd < 0) {
			log_err("shm_open() \"%s\" error", name);
			break;
		}

		void *map =
		    mmap(NULL, sizeof(log_buffer_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (map == MAP_FAILED) {
			close(fd);
			break;
		}

		result = map;
	} while (false);

	return result;
}

void
log_reader_print(const char name[], log_buffer_t *log)
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
