#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <pthread.h>
#include <stdint.h>

struct queue {
	char *ram;
	size_t ram_size;
	size_t appl_pos;
	size_t hw_pos;
	size_t data_size;
	int xrun;
	pthread_mutex_t mutex_for_hw_pos;
	pthread_cond_t cond;
	uint64_t written;
};

/* playback */
int queue_appl_write(struct queue *q, const char *buf, size_t bytes);
int queue_hw_read(struct queue *q, char *buf, size_t bytes);
static inline size_t queue_get_data_size_l(struct queue *q)
{
	return q->data_size;
}

/* capture */
int queue_appl_read(struct queue *q, char *buf, size_t bytes);
int queue_hw_write(struct queue *q, const char *buf, size_t bytes);

#endif
