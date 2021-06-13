/**
 * @file log.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции журналирования
 */

#include <log/log.h>
#include <private/record.h>

void
log_dbg(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	log_put_record(LOG_DBG, format, args);
	va_end(args);
}

void
log_inf(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	log_put_record(LOG_INF, format, args);
	va_end(args);
}

void
log_warn(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	log_put_record(LOG_WARN, format, args);
	va_end(args);
}

void
log_err(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	log_put_record(LOG_ERR, format, args);
	va_end(args);
}

void
log_exc(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	log_put_record(LOG_EXC, format, args);
	va_end(args);
}
