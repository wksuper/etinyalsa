#include "queue.h"
#include <string.h>
#include <klogging.h>

static inline size_t queue_get_appl_pos_l(struct queue *q)
{
	return q->appl_pos;
}

static inline size_t queue_get_empty_size_l(struct queue *q)
{
	return q->ram_size - queue_get_data_size_l(q);
}

int queue_hw_write(struct queue *q, const char *buf, size_t bytes)
{
	pthread_mutex_lock(&q->mutex_for_hw_pos);

	size_t w = q->hw_pos;
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

	if (q->data_size + bytes > q->ram_size) {
		q->appl_pos = w;
		q->data_size = q->ram_size;
		q->xrun = 1;
	} else {
		q->data_size += bytes;
	}
	q->hw_pos = w;
	KLOGV("queue:  +%7u bytes  [%10u / %10u] %3u%% %s\n",
	      bytes, q->data_size, q->ram_size, q->data_size * 100 / q->ram_size,
	      q->xrun ? "overrun" : "");

	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex_for_hw_pos);

	return 0;
}

int queue_appl_read(struct queue *q, char *buf, size_t bytes)
{
	size_t size = bytes;
	size_t avail = 0;
	size_t appl_pos = 0;

	while (size) {
		pthread_mutex_lock(&q->mutex_for_hw_pos);
		while ((avail = queue_get_data_size_l(q)) == 0) {
			pthread_cond_wait(&q->cond, &q->mutex_for_hw_pos);
		}
		appl_pos = queue_get_appl_pos_l(q);
		pthread_mutex_unlock(&q->mutex_for_hw_pos);

		const size_t actual_read = (size < avail ? size : avail);
		size_t left = actual_read;
		while (left) {
			size_t bytes_to_end = q->ram_size - appl_pos;
			if (left < bytes_to_end) {
				memcpy(buf, q->ram + appl_pos, left);
				buf += left;
				appl_pos += left;
				left = 0;
			} else {
				memcpy(buf, q->ram + appl_pos, bytes_to_end);
				buf += bytes_to_end;
				appl_pos = 0;
				left -= bytes_to_end;
			}
		}
		size -= actual_read;

		pthread_mutex_lock(&q->mutex_for_hw_pos);
		if (q->xrun) {
			/* Now, q->appl_pos == q->hw_pos,
			* q->data_size == q->ram_size
			* Reset the xrun, and need user to call
			* next queue_appl_read() immediately
			*/
			q->xrun = 0;
		} else {
			q->appl_pos = appl_pos;
			q->data_size -= actual_read;
		}
		KLOGV("queue:  -%7u bytes  [%10u / %10u] %3u%% %s\n",
		      bytes, q->data_size, q->ram_size, q->data_size * 100 / q->ram_size,
		      q->xrun ? "overrun" : "");
		pthread_mutex_unlock(&q->mutex_for_hw_pos);
	}

	return 0;
}

int queue_appl_write(struct queue *q, const char *buf, size_t bytes)
{
	size_t size = bytes;
	size_t empty = 0;
	size_t appl_pos = 0;

	while (size) {
		pthread_mutex_lock(&q->mutex_for_hw_pos);
		while ((empty = queue_get_empty_size_l(q)) == 0) {
			pthread_cond_wait(&q->cond, &q->mutex_for_hw_pos);
		}
		appl_pos = queue_get_appl_pos_l(q);
		pthread_mutex_unlock(&q->mutex_for_hw_pos);

		const size_t actual_write = (size < empty ? size : empty);
		size_t left = actual_write;
		while (left) {
			size_t bytes_to_end = q->ram_size - appl_pos;
			if (left < bytes_to_end) {
				memcpy(q->ram + appl_pos, buf, left);
				buf += left;
				appl_pos += left;
				left = 0;
			} else {
				memcpy(q->ram + appl_pos, buf, bytes_to_end);
				buf += bytes_to_end;
				appl_pos = 0;
				left -= bytes_to_end;
			}
		}
		size -= actual_write;

		pthread_mutex_lock(&q->mutex_for_hw_pos);
		if (q->xrun) {
			/* now, q->appl_pos == q->hw_pos,
			* q->data_size == 0
			* Reset the xrun, and need user to call
			* next queue_appl_write() immediately
			*/
			q->xrun = 0;

		} else {
			q->appl_pos = appl_pos;
			q->data_size += actual_write;
		}
		KLOGV("queue:  +%7u bytes  [%10u / %10u] %3u%%\n",
		      bytes, q->data_size, q->ram_size, q->data_size * 100 / q->ram_size);
		pthread_mutex_unlock(&q->mutex_for_hw_pos);
	}

	q->written += bytes;

	return 0;
}

int queue_hw_read(struct queue *q, char *buf, size_t bytes)
{
	pthread_mutex_lock(&q->mutex_for_hw_pos);

	size_t r = q->hw_pos;
	size_t size = bytes;
	while (size) {
		size_t bytes_to_end = q->ram_size - r;
		if (size < bytes_to_end) {
			memcpy(buf, q->ram + r, size);
			r += size;
			size = 0;
		} else {
			memcpy(buf, q->ram + r, bytes_to_end);
			buf += bytes_to_end;
			r = 0;
			size -= bytes_to_end;
		}
	}

	if (q->data_size < bytes) {
		q->appl_pos = r;
		q->data_size = 0;
		q->xrun = 1;
	} else {
		q->data_size -= bytes;
	}
	q->hw_pos = r;
	KLOGV("queue:  -%7u bytes  [%10u / %10u] %3u%% %s\n",
	      bytes, q->data_size, q->ram_size, q->data_size * 100 / q->ram_size,
	      q->xrun ? "underrun" : "");

	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex_for_hw_pos);

	return 0;
}
