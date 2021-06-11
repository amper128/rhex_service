/**
 * @file buffer.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Буфер журнала
 */

#include <log/log.h>
#include <svc/svc.h>

#include <private/record.h>

static log_buffer_t *log_buffer = NULL;

log_buffer_t *
get_log_buffer(void)
{
	return log_buffer;
}

void
log_init(void)
{
	const svc_context_t *ctx = get_svc_context();
	log_buffer = ctx->log_buffer;
}
