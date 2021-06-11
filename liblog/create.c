/**
 * @file create.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2021
 * @brief Создание журнала
 */

#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>

#include <log/log.h>

log_buffer_t *
log_create(const char name[])
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
