#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <pthread.h>

struct queue {
	char *ram;
	size_t ram_size;
	size_t r_pos;
	size_t w_pos;
	pthread_mutex_t mutex_for_w_pos;
	pthread_cond_t cond;
};

int queue_write(struct queue *q, const char *buf, size_t bytes);
int queue_read(struct queue *q, char *buf, size_t bytes);
size_t queue_get_read_available(struct queue *q);

#endif
