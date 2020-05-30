/**
 * @file log.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции журналирования
 */

#include <stdio.h>

#include <log.h>
#include <platform.h>

#define PR_RED "\x1B[31m"
#define PR_GRN "\x1B[32m"
#define PR_YEL "\x1B[33m"
#define PR_BLU "\x1B[34m"
#define PR_MAG "\x1B[35m"
#define PR_CYN "\x1B[36m"
#define PR_WHT "\x1B[37m"
#define PR_RES "\x1B[0m"

enum log_level {
	LOG_DBG = 0, /**< @brief Уровень отладки */
	LOG_INF,     /**< @brief Уровень информации */
	LOG_WARN,    /**< @brief Уровень предупреждений */
	LOG_ERR,     /**< @brief Уровень ошибок */
	LOG_EXC      /**< @brief Уровень исключений */
};

static const struct {
	const char *color;
	const char *msg;
} levels[] = {
	[LOG_DBG] = {PR_WHT, "Debug:\t"},     [LOG_INF] = {PR_GRN, "Info:\t"},
	[LOG_WARN] = {PR_YEL, "Warning:\t"},  [LOG_ERR] = {PR_RED, "Error:\t"},
	[LOG_EXC] = {PR_RED, "Exception:\t"},
};

static inline void
msg_begin(enum log_level level)
{
	if (isatty(fileno(stderr))) {
		fputs(levels[level].color, stderr);
	}

	fputs(levels[level].msg, stderr);
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
log_dbg(char *format, ...)
{
	va_list args;
	va_start(args, format);
	msg_log(LOG_DBG, format, args);
	va_end(args);
}

void
log_inf(char *format, ...)
{
	va_list args;
	va_start(args, format);
	msg_log(LOG_INF, format, args);
	va_end(args);
}

void
log_warn(char *format, ...)
{
	va_list args;
	va_start(args, format);
	msg_log(LOG_WARN, format, args);
	va_end(args);
}

void
log_err(char *format, ...)
{
	va_list args;
	va_start(args, format);
	msg_log(LOG_ERR, format, args);
	va_end(args);
}

void
log_exc(char *format, ...)
{
	va_list args;
	va_start(args, format);
	msg_log(LOG_EXC, format, args);
	va_end(args);
}
