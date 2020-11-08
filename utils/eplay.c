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

#include <easoundlib.h>
#include <klogging.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "wav_header.h"

static int g_quit;

static void sigint_handler(int sig)
{
	if (sig == SIGINT) {
		g_quit = 1;
	}
}

int main(int argc, char *argv[])
{
	int ret = EXIT_FAILURE;
	struct pcm *pcm = NULL;
	struct pcm_config config;
	struct epcm *epcm = NULL;
	struct epcm_config econfig;
	FILE *file = NULL;
	struct wav_header header;
	unsigned int card = 0;
	unsigned int device = 0;
	unsigned int period_size = 1024;
	unsigned int period_count = 4;
	enum pcm_format format = PCM_FORMAT_S16_LE;
	size_t extended_buffer_ms = 0;
	char c = -1;
	char *buf = NULL;
	size_t buf_size = 0;
	size_t buf_frames = 0;
	size_t total_frames = 0;

	KLOG_SET_OPTIONS(KLOGGING_TO_STDOUT);
	KLOG_SET_LEVEL(KLOGGING_LEVEL_VERBOSE);

	KCONSOLE("eplay version: %s\n", VERSION);
	KCONSOLE("klogging version: %s\n", KVERSION());

	if (argc < 2) {
		KCONSOLE("Usage: eplay {file.wav} [-D card] [-d device] "
		         "[-p period_size] [-n n_periods] "
		         "[-e extended_buffer_ms]\n");
		goto error;
	}

	file = fopen(argv[1], "rb");
	if (!file) {
		KLOGE("Unable to open %s\n", argv[1]);
		goto error;
	}

	while ((c = getopt(argc, argv, "D:d:p:n:e:")) != -1) {
		switch (c) {
		case 'd':
			device = atoi(optarg);
			break;
		case 'D':
			card = atoi(optarg);
			break;
		case 'p':
			period_size = atoi(optarg);
			break;
		case 'n':
			period_count = atoi(optarg);
			break;
		case 'e':
			extended_buffer_ms = atoi(optarg);
			break;
		case '?':
			KLOGE("Unknown option: %c\n", c);
			goto error;
		}
	}

	if (fread(&header, sizeof(header), 1, file) != 1) {
		KLOGE("Unable to read riff/wave header\n");
		goto error;
	}
	if ((header.riff_id != ID_RIFF) ||
	    (header.riff_fmt != ID_WAVE)) {
		KLOGE("Not a riff/wave header\n");
		goto error;
	}

	switch (header.bits_per_sample) {
	case 32:
		format = PCM_FORMAT_S32_LE;
		break;
	case 24:
		format = PCM_FORMAT_S24_LE;
		break;
	case 16:
		format = PCM_FORMAT_S16_LE;
		break;
	default:
		KLOGE("%u bits is not supported\n", header.bits_per_sample);
		goto error;
	}
	memset(&config, 0, sizeof(config));
	config.channels = header.num_channels;
	config.rate = header.sample_rate;
	config.period_size = period_size;
	config.period_count = period_count;
	config.format = format;
	config.start_threshold = 0;
	config.stop_threshold = 0;
	config.silence_threshold = 0;

	memset(&econfig, 0, sizeof(econfig));
	econfig.ram_millisecs = extended_buffer_ms;

	fseek(file, sizeof(struct wav_header), SEEK_SET);

	signal(SIGINT, sigint_handler);

	epcm = epcm_open(card, device, PCM_OUT, &config, &econfig);
	if (!epcm) {
		KLOGE("Unable to epcm_open()\n");
		goto error;
	}
	pcm = epcm_base(epcm);

	buf_frames = pcm_get_buffer_size(pcm);
	buf_size = pcm_frames_to_bytes(pcm, buf_frames);
	buf = malloc(buf_size);
	if (!buf) {
		KLOGE("Failed to malloc(%d bytes)\n", buf_size);
		goto error;
	}
	total_frames = 0;
	while (!g_quit) {
		if (fread(buf, buf_size, 1, file) != 1) {
			epcm_drain(epcm);
			break;
		}
		if (epcm_write(epcm, buf, buf_size) == 0) {
			total_frames += buf_frames;
		} else {
			KLOGE("Error to write %u bytes\n", buf_size);
		}
	}

	ret = EXIT_SUCCESS;

error:
	epcm_close(epcm);
	free(buf);
	if (file)
		fclose(file);
	return ret;
}
