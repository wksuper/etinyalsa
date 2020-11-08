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

#include <klogging.h>
#include <pthread.h>
#include <easoundlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "queue.h"

enum epcm_direction {
	EPCM_OUT = 0,
	EPCM_IN  = 1
};

struct epcm {
	struct pcm *pcm;

	/* extention */
	enum epcm_direction dir;
	struct queue q;
	pthread_t tid;
	int stop;
};

struct pcm *epcm_base(struct epcm *epcm)
{
	return epcm->pcm;
}

static void *pcm_streaming_thread(void *data)
{
	KLOGD("%s() enter\n", __FUNCTION__);

	struct epcm *epcm = (struct epcm *)data;
	struct pcm *pcm = epcm->pcm;
	const size_t bytes = pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
	char *buf = (char *)malloc(bytes);

	if (!buf) {
		KLOGE("Failed to alloc %u bytes\n", bytes);
		goto error;
	}

	while (!epcm->stop) {
		if (epcm->dir == EPCM_IN) {
			if (pcm_read(pcm, buf, bytes) != 0) {
				KLOGE("Error to pcm_read(%u bytes)\n", bytes);
				continue;
			}
			if (queue_hw_write(&epcm->q, buf, bytes) != 0) {
				KLOGE("Error to queue_hw_write(%u bytes)\n", bytes);
			}
		} else {
			if (queue_hw_read(&epcm->q, buf, bytes) != 0) {
				KLOGE("Error to queue_hw_read(%u bytes)\n", bytes);
				continue;
			}

			if (pcm_write(pcm, buf, bytes) != 0) {
				KLOGE("Error to pcm_write(%u bytes)\n", bytes);
			}
		}
	}

error:
	free(buf);
	KLOGD("%s() leave\n", __FUNCTION__);
	return NULL;
}

struct epcm *epcm_open(unsigned int card,
                       unsigned int device,
                       unsigned int flags,
                       const struct pcm_config *config,
                       const struct epcm_config *econfig)
{
	KLOGD("%s() enter\n", __FUNCTION__);

	struct queue *q = NULL;
	struct epcm *epcm = (struct epcm *)calloc(1, sizeof(struct epcm));
	if (!epcm) {
		KLOGE("Failed to alloc struct epcm\n");
		goto error;
	}

	epcm->pcm = pcm_open(card, device, flags, config);
	if (!epcm->pcm) {
		KLOGE("Unable to open PCM device\n");
		goto error;
	}
	if (!pcm_is_ready(epcm->pcm)) {
		KLOGE("Unable to open PCM device (%s)\n", pcm_get_error(epcm->pcm));
		goto error;
	}

	epcm->dir = !!(flags & (0x1u << 28));

	q = &epcm->q;

	if (econfig->ram_millisecs) {
		size_t ram_frames = (uint64_t)pcm_get_rate(epcm->pcm)
		                    * (uint64_t)econfig->ram_millisecs / (uint64_t)1000;
		if (ram_frames < pcm_get_buffer_size(epcm->pcm) * 3) {
			KLOGE("Too small RAM size\n");
			goto error;
		}
		q->ram_size = pcm_frames_to_bytes(epcm->pcm, ram_frames);
		KLOGD("ram_size=%u\n", q->ram_size);
		q->ram = (char *)malloc(q->ram_size);
		if (!q->ram) {
			KLOGE("Failed to alloc memory\n");
			q->ram_size = 0;
		} else {
			q->hw_pos = q->appl_pos = 0;
			q->data_size = 0;
			q->written = 0;

			epcm->stop = 0;

			pthread_mutex_init(&q->mutex_for_hw_pos, (const pthread_mutexattr_t *)NULL);
			pthread_cond_init(&q->cond, (const pthread_condattr_t *)NULL);
		}
	} else {
		q->ram = NULL;
		q->ram_size = 0;
	}

	KLOGD("%s() leave\n", __FUNCTION__);
	return epcm;

error:
	epcm_close(epcm);
	KLOGD("%s() leave\n", __FUNCTION__);
	return NULL;
}

int epcm_read(struct epcm *epcm, void *data, unsigned int count)
{
	struct queue *q = &epcm->q;

	if (q->ram_size) {
		if (!epcm->tid) {
			int ret;

			if (ret = pthread_create(&epcm->tid, NULL, pcm_streaming_thread, epcm)) {
				KLOGE("Failed to create pcm_streaming_thread\n");
				return ret;
			}
		}

		return queue_appl_read(q, data, count);
	} else {
		return pcm_read(epcm->pcm, data, count);
	}
}

int epcm_write(struct epcm *epcm, const void *data, unsigned int count)
{
	struct queue *q = &epcm->q;

	if (q->ram_size) {
		/* The start_threshold is set to 2 * pcm_get_buffer_size().
		 * The reason is the first pcm_get_buffer_size() will be written
		 * to kernel buffer immediately, and the second pcm_get_buffer_size()
		 * will be read out of queue also immediately. After the two
		 * pcm_get_buffer_size(), kernel is consuming the data at the speed of
		 * sampling rate.
		 */
		if (!epcm->tid &&
		    pcm_bytes_to_frames(epcm->pcm, q->written) >
		    2 * pcm_get_buffer_size(epcm->pcm)) {
			int ret;
			if (ret = pthread_create(&epcm->tid, NULL, pcm_streaming_thread, epcm)) {
				KLOGE("Failed to create pcm_streaming_thread\n");
				return ret;
			}
		}

		return queue_appl_write(q, data, count);
	} else {
		return pcm_write(epcm->pcm, data, count);
	}
}

int epcm_drain(struct epcm *epcm)
{
	struct queue *q = &epcm->q;

	if (q->ram_size) {
		const struct pcm_config *config = pcm_get_config(epcm->pcm);
		size_t data_size;

		while ((data_size = queue_get_data_size_l(q)) > 0) {
			KLOGV("epcm: Draining... (data_size=%u)\n", data_size);
			/* sleep 1 period time */
			usleep((uint64_t)config->period_size
			       * (uint64_t)1000000 / (uint64_t)config->rate);
		}
		KLOGD("epcm: Drained\n");
	}

	return 0;
}

int epcm_close(struct epcm *epcm)
{
	KLOGD("%s() enter\n", __FUNCTION__);
	if (epcm) {
		struct queue *q = &epcm->q;
		if (q->ram_size) {
			epcm->stop = 1;
			pthread_mutex_lock(&q->mutex_for_hw_pos);
			pthread_cond_signal(&q->cond);
			pthread_mutex_unlock(&q->mutex_for_hw_pos);
			pthread_join(epcm->tid, NULL);

			pthread_cond_destroy(&q->cond);
			pthread_mutex_destroy(&q->mutex_for_hw_pos);
			free(q->ram);
			q->ram = NULL;
			q->ram_size = 0;
		} else {
			/* bypass mode */
		}

		if (epcm->pcm) {
			pcm_close(epcm->pcm);
			epcm->pcm = NULL;
		}

		free(epcm);
		epcm = NULL;
	}
	KLOGD("%s() leave\n", __FUNCTION__);

	return 0;
}
