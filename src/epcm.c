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

struct epcm {
	struct pcm *pcm;

	/* extention */
	char *ram;
	size_t ram_size;
	size_t r_pos;
	size_t w_pos;
	pthread_mutex_t mutex_for_w_pos;
	pthread_cond_t cond;
	pthread_t tid;
	int stop;
};

struct pcm *epcm_base(struct epcm *epcm)
{
	return epcm->pcm;
}

static size_t ram_get_read_available(struct epcm *epcm)
{
	return (epcm->w_pos >= epcm->r_pos)
	       ? (epcm->w_pos - epcm->r_pos)
	       : (epcm->ram_size - epcm->r_pos + epcm->w_pos);
}

static void *pcm_streaming_thread(void *data)
{
	KLOGD("%s() enter\n", __FUNCTION__);

	struct epcm *epcm = (struct epcm *)data;
	const size_t tmp_size = pcm_frames_to_bytes(epcm->pcm, pcm_get_buffer_size(epcm->pcm));
	char *tmp = (char *)malloc(tmp_size);

	while (!epcm->stop) {
		if (pcm_read(epcm->pcm, tmp, tmp_size) != 0) {
			KLOGE("Error to pcm_read(%u bytes)\n", tmp_size);
			continue;
		}
		size_t w = epcm->w_pos;
		char *p = tmp;
		size_t size = tmp_size;
		while (size) {
			size_t bytes_to_end = epcm->ram_size - w;
			if (size < bytes_to_end) {
				memcpy(epcm->ram + w, p, size);
				w += size;
				size = 0;
			} else {
				memcpy(epcm->ram + w, p, bytes_to_end);
				p += bytes_to_end;
				w = 0;
				size -= bytes_to_end;
			}
		}

		pthread_mutex_lock(&epcm->mutex_for_w_pos);
		epcm->w_pos = w;
		size_t read_available = ram_get_read_available(epcm);
		KLOGV("write  to queue: %u bytes, queue status: [%10u / %10u] %3u%%\n",
		      tmp_size, read_available, epcm->ram_size, read_available * 100 / epcm->ram_size);
		pthread_cond_signal(&epcm->cond);
		pthread_mutex_unlock(&epcm->mutex_for_w_pos);
	}

	free(tmp);
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

	if (econfig->ram_millisec) {
		epcm->ram_size = pcm_frames_to_bytes(epcm->pcm,
		                                     pcm_get_rate(epcm->pcm) * econfig->ram_millisec / 1000);
		KLOGD("ram_size=%u\n", epcm->ram_size);
		epcm->ram = (char *)malloc(epcm->ram_size);
		if (!epcm->ram) {
			KLOGE("Failed to alloc memory\n");
			epcm->ram_size = 0;
		} else {
			epcm->r_pos = epcm->w_pos = 0;
			epcm->stop = 0;

			pthread_mutex_init(&epcm->mutex_for_w_pos, (const pthread_mutexattr_t *)NULL);
			pthread_cond_init(&epcm->cond, (const pthread_condattr_t *)NULL);

			if (pthread_create(&epcm->tid, NULL, pcm_streaming_thread, epcm)) {
				KLOGE("Failed to create pcm_streaming_thread\n");
				goto error;
			}
		}
	} else {
		epcm->ram = NULL;
		epcm->ram_size = 0;
	}

	KLOGD("%s() leave\n", __FUNCTION__);
	return epcm;

error:
	epcm_close(epcm);
	KLOGD("%s() leave\n", __FUNCTION__);
	return NULL;
}

static int read_from_queue(struct epcm *epcm, void *data, unsigned int count)
{
	char *dest = (char *)data;
	const size_t requested_bytes = count;
	size_t read_available = 0;

	while (count) {
		pthread_mutex_lock(&epcm->mutex_for_w_pos);
		while ((read_available = ram_get_read_available(epcm)) == 0) {
			pthread_cond_wait(&epcm->cond, &epcm->mutex_for_w_pos);
		}
		pthread_mutex_unlock(&epcm->mutex_for_w_pos);

		const size_t actual_read = (count < read_available ? count : read_available);
		size_t left = actual_read;
		while (left) {
			size_t bytes_to_end = epcm->ram_size - epcm->r_pos;
			if (left < bytes_to_end) {
				memcpy(dest, epcm->ram + epcm->r_pos, left);
				dest += left;
				epcm->r_pos += left;
				left = 0;
			} else {
				memcpy(dest, epcm->ram + epcm->r_pos, bytes_to_end);
				dest += bytes_to_end;
				epcm->r_pos = 0;
				left -= bytes_to_end;
			}
		}
		count -= actual_read;
	}
	const size_t total_read = dest - (char *)data;
	read_available = ram_get_read_available(epcm);
	KLOGV("read from queue: %u bytes, queue status: [%10u / %10u] %3u%%\n",
	      total_read, read_available, epcm->ram_size, read_available * 100 / epcm->ram_size);
	return (total_read == requested_bytes) ? 0 : -1;
}

int epcm_read(struct epcm *epcm, void *data, unsigned int count)
{
	return epcm->ram_size
	       ? read_from_queue(epcm, data, count)
	       : pcm_read(epcm->pcm, data, count);
}

int epcm_close(struct epcm *epcm)
{
	KLOGD("%s() enter\n", __FUNCTION__);
	if (epcm) {
		if (epcm->ram_size) {
			epcm->stop = 1;
			pthread_mutex_lock(&epcm->mutex_for_w_pos);
			pthread_cond_signal(&epcm->cond);
			pthread_mutex_unlock(&epcm->mutex_for_w_pos);
			pthread_join(epcm->tid, NULL);
			// td_dump_to_file(epcm); // flush the remaining data in the ram

			pthread_cond_destroy(&epcm->cond);
			pthread_mutex_destroy(&epcm->mutex_for_w_pos);
			free(epcm->ram);
			epcm->ram = NULL;
			epcm->ram_size = 0;
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
