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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "resampler.h"

#define LINEAR_DIV_SHIFT 19
#define LINEAR_DIV (1<<LINEAR_DIV_SHIFT)
#define MAX_CHNUM  (8)

struct resampler {
	size_t chnum;
	size_t in_rate;
	size_t out_rate;
	void (*process_s16)(struct resampler *rs, int16_t *dst, size_t dst_frames,
	                    const int16_t *src, size_t src_frames);

	int16_t old_sample[MAX_CHNUM];
	unsigned int pitch;
	unsigned int pitch_shift;
};

static void upsample_s16(struct resampler *rs, int16_t *dst, size_t dst_frames,
                         const int16_t *src, size_t src_frames)
{
	size_t ch;

	for (ch = 0; ch < rs->chnum; ++ch) {
		int16_t old_sample = 0;
		int16_t new_sample = rs->old_sample[ch];
		size_t j = 0;
		size_t i = 0;
		size_t pos = rs->pitch;
		const int16_t *p = src + ch;
		int16_t *q = dst + ch;
		while (j < dst_frames) {
			int old_weight, new_weight;
			if (pos >= rs->pitch) {
				pos -= rs->pitch;
				old_sample = new_sample;
				if (i < src_frames)
					new_sample = *p;
			}
			new_weight = (pos << (16 - rs->pitch_shift)) / (rs->pitch >> rs->pitch_shift);
			old_weight = 0x10000 - new_weight;
			*q = (old_sample * old_weight + new_sample * new_weight) >> 16;
			q += rs->chnum;
			++j;
			pos += LINEAR_DIV;
			if (pos >= rs->pitch) {
				p += rs->chnum;
				++i;
			}
		}
		rs->old_sample[ch] = new_sample;
	}
}

static void downsample_s16(struct resampler *rs, int16_t *dst, size_t dst_frames,
                           const int16_t *src, size_t src_frames)
{
	size_t ch;

	for (ch = 0; ch < rs->chnum; ++ch) {
		const int16_t *p = src + ch;
		int16_t *q = dst + ch;
		int16_t old_sample = 0;
		int16_t new_sample = 0;
		unsigned int pos = LINEAR_DIV - rs->pitch;
		size_t i = 0;
		size_t j = 0;
		while (i < src_frames) {
			new_sample = *p;
			p += rs->chnum;
			++i;
			pos += rs->pitch;
			if (pos >= LINEAR_DIV) {
				int old_weight, new_weight;
				pos -= LINEAR_DIV;
				old_weight = (pos << (32 - LINEAR_DIV_SHIFT)) / (rs->pitch >> (LINEAR_DIV_SHIFT - 16));
				new_weight = 0x10000 - old_weight;
				*q = (old_sample * old_weight + new_sample * new_weight) >> 16;
				q += rs->chnum;
				++j;
				if (j > dst_frames) {
					printf("dst_frames overflow\n");
					break;
				}
			}
			old_sample = new_sample;
		}
	}
}

int rs_adjust(struct resampler *rs, size_t out_rate, size_t in_rate)
{
	rs->pitch = (((uint64_t)out_rate * LINEAR_DIV) + (in_rate / 2)) / in_rate;

	if (rs->pitch >= LINEAR_DIV) {
		rs->pitch_shift = 0;
		while ((rs->pitch >> rs->pitch_shift) >= (1 << 16))
			++rs->pitch_shift;
	}

	rs->in_rate = in_rate;
	rs->out_rate = out_rate;
	rs->process_s16 = (in_rate < out_rate) ? upsample_s16 : downsample_s16;

	return 0;
}

struct resampler *rs_open(size_t chnum, size_t out_rate, size_t in_rate)
{
	struct resampler *rs = calloc(1, sizeof(struct resampler));
	if (!rs)
		return NULL;
	if (chnum > MAX_CHNUM) {
		goto error;
	}
	rs->chnum = chnum;
	rs_adjust(rs, out_rate, in_rate);

	return rs;

error:
	rs_close(rs);
	return NULL;
}

void rs_process(struct resampler *rs, int16_t *dst, size_t dst_frames,
                const int16_t *src, size_t src_frames)
{
	rs->process_s16(rs, dst, dst_frames, src, src_frames);
}

void rs_close(struct resampler *rs)
{
	free(rs);
}
