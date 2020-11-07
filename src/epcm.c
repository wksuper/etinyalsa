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
#include "queue.h"

struct epcm {
	struct pcm *pcm;

	/* extention */
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

	while (!epcm->stop) {
		if (pcm_read(pcm, buf, bytes) != 0) {
			KLOGE("Error to pcm_read(%u bytes)\n", bytes);
			continue;
		}
		if (queue_write(&epcm->q, buf, bytes) != 0) {
			KLOGE("Error to queue_write(%u bytes)\n", bytes);
		}
	}

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
	if (!epcm)
		goto error;

	epcm->pcm = pcm_open(card, device, flags, config);
	if (!epcm->pcm)
		goto error;

	if (!epcm->pcm) {
		KLOGE("Unable to open PCM device\n");
		goto error;
	}
	if (!pcm_is_ready(epcm->pcm)) {
		KLOGE("Unable to open PCM device (%s)\n", pcm_get_error(epcm->pcm));
		goto error;
	}

	q = &epcm->q;

	if (econfig->ram_millisecs) {
		q->ram_size = pcm_frames_to_bytes(epcm->pcm,
		                                  pcm_get_rate(epcm->pcm) * econfig->ram_millisecs / 1000);
		KLOGD("ram_size=%u\n", q->ram_size);
		q->ram = (char *)malloc(q->ram_size);
		if (!q->ram) {
			KLOGE("Failed to alloc memory\n");
			q->ram_size = 0;
		} else {
			q->r_pos = q->w_pos = 0;
			epcm->stop = 0;

			pthread_mutex_init(&q->mutex_for_w_pos, (const pthread_mutexattr_t *)NULL);
			pthread_cond_init(&q->cond, (const pthread_condattr_t *)NULL);

			if (pthread_create(&epcm->tid, NULL, pcm_streaming_thread, epcm)) {
				KLOGE("Failed to create pcm_streaming_thread\n");
				goto error;
			}
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
	return q->ram_size
	       ? queue_read(q, data, count)
	       : pcm_read(epcm->pcm, data, count);
}

int epcm_close(struct epcm *epcm)
{
	KLOGD("%s() enter\n", __FUNCTION__);
	if (epcm) {
		struct queue *q = &epcm->q;
		if (q->ram_size) {
			epcm->stop = 1;
			pthread_mutex_lock(&q->mutex_for_w_pos);
			pthread_cond_signal(&q->cond);
			pthread_mutex_unlock(&q->mutex_for_w_pos);
			pthread_join(epcm->tid, NULL);

			pthread_cond_destroy(&q->cond);
			pthread_mutex_destroy(&q->mutex_for_w_pos);
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
