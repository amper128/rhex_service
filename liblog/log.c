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
log_dbg(char *format, ...)
{
	va_list args;
	va_start(args, format);
	log_put_record(LOG_DBG, format, args);
	va_end(args);
}

void
log_inf(char *format, ...)
{
	va_list args;
	va_start(args, format);
	log_put_record(LOG_INF, format, args);
	va_end(args);
}

void
log_warn(char *format, ...)
{
	va_list args;
	va_start(args, format);
	log_put_record(LOG_WARN, format, args);
	va_end(args);
}

void
log_err(char *format, ...)
{
	va_list args;
	va_start(args, format);
	log_put_record(LOG_ERR, format, args);
	va_end(args);
}

void
log_exc(char *format, ...)
{
	va_list args;
	va_start(args, format);
	log_put_record(LOG_EXC, format, args);
	va_end(args);
}
