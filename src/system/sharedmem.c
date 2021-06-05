/**
 * @file main.c
 * @author Алексей Хохлов <root@amper.me>
 * @copyright WTFPL License
 * @date 2020
 * @brief Функции работы с общей памятью
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include <log.h>
#include <sharedmem.h>

#define SHM_COPIES (4U)
#define SHM_MAGIC (0x53484D5F44415441ULL)
#define SHM_GUARD (0x53484D4755415244ULL)

#define SHM_START_IDX (0xFFFFFFFEU)

typedef struct {
	uint32_t index;
	uint32_t size;
	uint64_t offset;
} shm_slot_t;

typedef struct {
	uint64_t magic;
	uint32_t size;
	uint32_t copies;
	uint32_t index;

	shm_slot_t slot[SHM_COPIES];
} shm_header_t;

static inline size_t
align_size(size_t size)
{
	return (size + (sizeof(uint64_t *) - 1U)) & ~(sizeof(uint64_t *) - 1U);
}

static inline size_t
calc_shm_size(size_t copy_size, size_t copies)
{
	return (align_size(copy_size) * copies) + sizeof(shm_header_t);
}

int32_t
shm_map_init(const char name[], size_t size)
{
	int32_t result = 0;

	do {
		char shm_name[256];
		snprintf(shm_name, sizeof(shm_name), "/rhex_%s", name);
		int fd = shm_open(shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
		if (fd < 0) {
			log_err("shm_open() \"%s\" error", name);
			result = -1;
			break;
		}

		size_t map_size = calc_shm_size(size, SHM_COPIES);

		if (ftruncate(fd, map_size) == -1) {
			log_err("cannot ftruncate()");
			result = -1;
			break;
		}

		void *map = mmap(NULL, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		close(fd);
		if (map == MAP_FAILED) {
			result = -1;
			break;
		}

		shm_header_t *header = map;
		header->magic = SHM_MAGIC;
		header->size = size;
		header->copies = SHM_COPIES;
		header->index = SHM_START_IDX;

		uint32_t slot;
		for (slot = 0U; slot < SHM_COPIES; slot++) {
			header->slot[slot].index = SHM_START_IDX;
			header->slot[slot].size = 0U;

			header->slot[slot].offset = (align_size(size) * slot);
		}
	} while (false);

	return result;
}

int32_t
shm_map_open(const char name[], shm_t *shm)
{
	int32_t result = 0;

	do {
		char shm_name[256];
		snprintf(shm_name, sizeof(shm_name), "/rhex_%s", name);
		int fd = shm_open(shm_name, O_RDWR, S_IRUSR | S_IWUSR);
		if (fd < 0) {
			log_err("shm_open() \"%s\" error", name);
			result = -1;
			break;
		}

		void *map =
		    mmap(NULL, sizeof(shm_header_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (map == MAP_FAILED) {
			close(fd);
			result = -1;
			break;
		}

		shm_header_t header;
		memcpy(&header, map, sizeof(header));

		if (header.magic != SHM_MAGIC) {
			close(fd);
			log_err("invalid shm_magic");
			result = -1;
			break;
		}

		munmap(map, sizeof(shm_header_t));

		map = mmap(NULL, calc_shm_size(header.size, SHM_COPIES), PROT_READ | PROT_WRITE,
			   MAP_SHARED, fd, 0);
		close(fd);
		if (map == MAP_FAILED) {
			result = -1;
			break;
		}

		shm->guard = SHM_GUARD;
		shm->map = map;
		shm->size = header.size;
	} while (false);

	return result;
}

int32_t
shm_map_read(shm_t *shm, void **data)
{
	int32_t result = 0;

	if (shm->guard != SHM_GUARD) {
		log_err("shm guard error!");
		result = -1;
	} else {
		shm_header_t *hdr = shm->map;
		if (hdr == NULL) {
			log_err(NULL);
		}

		uint32_t index = hdr->index;

		size_t slot = index % SHM_COPIES;

		union {
			shm_header_t *h;
			uint64_t *u64;
		} p;
		p.h = &hdr[1];
		*data = &p.u64[(hdr->slot[slot].offset) / sizeof(uint64_t *)];
	}

	return result;
}

int32_t
shm_map_write(shm_t *shm, void *data, size_t size)
{
	int32_t result = 0;

	if (shm->guard != SHM_GUARD) {
		log_err("shm guard error!");
		result = -1;
	} else {
		shm_header_t *hdr = shm->map;

		uint32_t index = hdr->index;
		index++;
		size_t slot = index % SHM_COPIES;

		union {
			shm_header_t *h;
			uint64_t *u64;
		} p;
		p.h = &hdr[1];
		void *dst;
		dst = &p.u64[(hdr->slot[slot].offset) / sizeof(uint64_t *)];

		memcpy(dst, data, size);

		hdr->index = index;
	}

	return result;
}
