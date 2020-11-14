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

#ifndef __EASOUNDLIB_H__
#define __EASOUNDLIB_H__

#include <tinyalsa/asoundlib.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct epcm;
struct epcm_config {
	unsigned int ram_millisecs;
        int tuner;
};

struct epcm *epcm_open(unsigned int card,
                       unsigned int device,
                       unsigned int flags,
                       const struct pcm_config *config,
                       const struct epcm_config *econfig);

struct pcm *epcm_base(struct epcm *epcm);

int epcm_read(struct epcm *epcm, void *data, unsigned int count);

int epcm_write(struct epcm *epcm, const void *data, unsigned int count);

int epcm_drain(struct epcm *epcm);

int epcm_close(struct epcm *epcm);

#if defined(__cplusplus)
}
#endif

#endif
