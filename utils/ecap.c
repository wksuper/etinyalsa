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

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

#define FORMAT_PCM 1

struct wav_header {
	uint32_t riff_id;
	uint32_t riff_sz;
	uint32_t riff_fmt;
	uint32_t fmt_id;
	uint32_t fmt_sz;
	uint16_t audio_format;
	uint16_t num_channels;
	uint32_t sample_rate;
	uint32_t byte_rate;
	uint16_t block_align;
	uint16_t bits_per_sample;
	uint32_t data_id;
	uint32_t data_sz;
};


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
	unsigned int channels = 2;
	unsigned int rate = 48000;
	unsigned int bits = 16;
	unsigned int frames;
	unsigned int period_size = 1024;
	unsigned int period_count = 4;
	enum pcm_format format = PCM_FORMAT_S16_LE;
	size_t extended_buffer_ms = 0;
	char c = -1;
	char *buf = NULL;
	size_t read_size = 0;
	size_t read_frames = 0;
	size_t total_frames = 0;

	KLOG_SET_OPTIONS(KLOGGING_TO_STDOUT);
	KLOG_SET_LEVEL(KLOGGING_LEVEL_VERBOSE);

	KCONSOLE("ecap version: %s\n", VERSION);
	KCONSOLE("klogging version: %s\n", KVERSION());

	if (argc < 2) {
		KCONSOLE("Usage: ecap {file.wav} [-D card] [-d device] [-c channels] "
		         "[-r rate] [-b bits] [-p period_size] [-n n_periods] "
			 "[-e extended_buffer_ms]\n");
		goto error;
	}

	file = fopen(argv[1], "wb");
	if (!file) {
		KLOGE("Unable to open %s\n", argv[1]);
		goto error;
	}

	while ((c = getopt(argc, argv, "D:d:c:r:b:p:n:e:")) != -1) {
		switch (c) {
		case 'd':
			device = atoi(optarg);
			break;
		case 'c':
			channels = atoi(optarg);
			break;
		case 'r':
			rate = atoi(optarg);
			break;
		case 'b':
			bits = atoi(optarg);
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
			KCONSOLE("Unknown option: %c\n", c);
			goto error;
		}
	}

	header.riff_id = ID_RIFF;
	header.riff_sz = 0;
	header.riff_fmt = ID_WAVE;
	header.fmt_id = ID_FMT;
	header.fmt_sz = 16;
	header.audio_format = FORMAT_PCM;
	header.num_channels = channels;
	header.sample_rate = rate;
	switch (bits) {
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
		KCONSOLE("%u bits is not supported\n", bits);
		goto error;
	}
	header.bits_per_sample = pcm_format_to_bits(format);
	header.byte_rate = (header.bits_per_sample / 8) * channels * rate;
	header.block_align = channels * (header.bits_per_sample / 8);
	header.data_id = ID_DATA;

	memset(&config, 0, sizeof(config));
	config.channels = channels;
	config.rate = rate;
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

	epcm = epcm_open(card, device, PCM_IN, &config, &econfig);
	if (!epcm) {
		KLOGE("Unable to epcm_open()\n");
		goto error;
	}
	pcm = epcm_base(epcm);

	read_frames = pcm_get_buffer_size(pcm);
	read_size = pcm_frames_to_bytes(pcm, read_frames);
	buf = malloc(read_size);
	if (!buf) {
		KLOGE("Failed to malloc(%d bytes)\n", read_size);
		goto error;
	}
	total_frames = 0;
	while (!g_quit) {
		if (epcm_read(epcm, buf, read_size) == 0) {
			fwrite(buf, read_size, 1, file);
			total_frames += read_frames;
		} else {
			KLOGE("Error to read %u bytes\n", read_size);
		}
	}

        header.data_sz = total_frames * header.block_align;
        header.riff_sz = header.data_sz + sizeof(header) - 8;
        fseek(file, 0, SEEK_SET);
        fwrite(&header, sizeof(struct wav_header), 1, file);

	ret = EXIT_SUCCESS;

error:
	epcm_close(epcm);
	free(buf);
	if (file)
		fclose(file);
	return ret;
}
