#include "queue.h"
#include <string.h>
#include <klogging.h>

static inline size_t queue_get_read_available_l(struct queue *q)
{
	return (q->w_pos >= q->r_pos)
	       ? (q->w_pos - q->r_pos)
	       : (q->ram_size - q->r_pos + q->w_pos);
}

size_t queue_get_read_available(struct queue *q)
{
	int ret;

	pthread_mutex_lock(&q->mutex_for_w_pos);
	ret = queue_get_read_available_l(q);
	pthread_mutex_unlock(&q->mutex_for_w_pos);

	return ret;
}

int queue_write(struct queue *q, const char *buf, size_t bytes)
{
	size_t w = q->w_pos;
	const char *p = buf;
	size_t size = bytes;
	while (size) {
		size_t bytes_to_end = q->ram_size - w;
		if (size < bytes_to_end) {
			memcpy(q->ram + w, p, size);
			w += size;
			size = 0;
		} else {
			memcpy(q->ram + w, p, bytes_to_end);
			p += bytes_to_end;
			w = 0;
			size -= bytes_to_end;
		}
	}

	pthread_mutex_lock(&q->mutex_for_w_pos);
	q->w_pos = w;
	size_t read_available = queue_get_read_available_l(q);
	KLOGV("queue:  +%7u bytes  [%10u / %10u] %3u%%\n",
	      bytes, read_available, q->ram_size, read_available * 100 / q->ram_size);
	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex_for_w_pos);

	return 0;
}

int queue_read(struct queue *q, char *buf, size_t bytes)
{
	size_t size = bytes;
	size_t read_available = 0;

	while (size) {
		pthread_mutex_lock(&q->mutex_for_w_pos);
		while ((read_available = queue_get_read_available_l(q)) == 0) {
			pthread_cond_wait(&q->cond, &q->mutex_for_w_pos);
		}
		pthread_mutex_unlock(&q->mutex_for_w_pos);

		const size_t actual_read = (size < read_available ? size : read_available);
		size_t left = actual_read;
		while (left) {
			size_t bytes_to_end = q->ram_size - q->r_pos;
			if (left < bytes_to_end) {
				memcpy(buf, q->ram + q->r_pos, left);
				buf += left;
				q->r_pos += left;
				left = 0;
			} else {
				memcpy(buf, q->ram + q->r_pos, bytes_to_end);
				buf += bytes_to_end;
				q->r_pos = 0;
				left -= bytes_to_end;
			}
		}
		size -= actual_read;
	}

	read_available = queue_get_read_available(q);
	KLOGV("queue:  -%7u bytes  [%10u / %10u] %3u%%\n",
	      bytes, read_available, q->ram_size, read_available * 100 / q->ram_size);

	return 0;
}
