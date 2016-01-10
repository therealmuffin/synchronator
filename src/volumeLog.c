/****************************************************************************
 * Copyright (C) 2016 Muffinman
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 ****************************************************************************/

//alsamixer uses:
// http://git.alsa-project.org/?p=alsa-utils.git;a=blob;f=alsamixer/volume_mapping.c
/*
  18  * The functions in this file map the value ranges of ALSA mixer controls onto
  19  * the interval 0..1.
  20  *
  21  * The mapping is designed so that the position in the interval is proportional
  22  * to the volume as a human ear would perceive it (i.e., the position is the
  23  * cubic root of the linear sample multiplication factor).  For controls with
  24  * a small range (24 dB or less), the mapping is linear in the dB values so
  25  * that each step has the same size visually.  Only for controls without dB
  26  * information, a linear mapping of the hardware volume register values is used
  27  * (this is the same algorithm as used in the old alsamixer).
  28  *
  29  * When setting the volume, 'dir' is the rounding direction:
  30  * -1/0/1 = down/nearest/up.
  31  */

#include <math.h>
#include "volume.h"
#include "common.h"

static void help(void);
static void regenerateMultipliers(void);
static void convertExternal2Mixer(double *volume);
static void convertMixer2Internal(long *volume);

volumeCurve_t volumeCurveLog = {
    .name = "log",
    .help = &help,
    .regenerateMultipliers = &regenerateMultipliers,
    .convertExternal2Mixer = &convertExternal2Mixer,
    .convertMixer2Internal = &convertMixer2Internal
    
};

static double multiplierNormalize;
static double multiplierExtToMixer;

static void help(void) {
    printf("Log\n"
          );
}


// perhaps long int and increase range to 100000 to allow for 
static void regenerateMultipliers(void) {
    common_data.multiplierIntToDevice = ((float)common_data.volume_max - (float)common_data.volume_min) / (100-0);
    multiplierExtToMixer = ((double)common_data.volume_max - (double)common_data.volume_min) / (double)common_data.alsa_volume_range;
    multiplierNormalize = ((double)common_data.volume_max - (double)common_data.volume_min) / (100-0);
}

static void convertExternal2Mixer(double *volume) {
    *volume = ((*volume - (double)common_data.volume_min) / multiplierNormalize);
    *volume = pow(10,*volume/50); // 50 vs 84,9485
    
    if(*volume<0)
        *volume = 0;

    *volume = (*volume / multiplierExtToMixer) + (double)common_data.alsa_volume_min;
}

static void convertMixer2Internal(long *volume) {
    if(*volume <= 1) {
        *volume = 0;
        return;
    }
    
    *volume = 50*log10(*volume);
}