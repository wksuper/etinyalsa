/*
 * Copyright (c) 2020 Kui Wang
 *
 * This file is part of etinyalsa.
 *
 * etinyalsa is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * etinyalsa is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with etinyalsa; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

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
