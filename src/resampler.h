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

#ifndef __RESAMPLER_H__
#define __RESAMPLER_H__

#include <stddef.h>
#include <stdint.h>

struct resampler;

struct resampler *rs_open(size_t chnum, size_t out_rate, size_t in_rate);
void rs_process(struct resampler *rs, int16_t *dst, size_t dst_frames,
                const int16_t *src, size_t src_frames);
int rs_adjust(struct resampler *rs, size_t out_rate, size_t in_rate);
void rs_close(struct resampler *rs);

#endif
