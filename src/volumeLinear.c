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


#include "volume.h"
#include "common.h"

static void help(void);
static void regenerateMultipliers(void);
static void convertExternal2Mixer(double *volume);
static void convertMixer2Internal(long *volume);
static void convertInternal2External(long *volumeInternal, double *volumeExternal);
static void convertExternal2Internal(double *volumeExternal, long *volumeInternal);

volumeCurve_t volumeCurveLinear = {
    .name = "linear",
    .help = &help,
    .regenerateMultipliers = &regenerateMultipliers,
    .convertExternal2Mixer = &convertExternal2Mixer,
    .convertMixer2Internal = &convertMixer2Internal,
    .convertInternal2External = &convertInternal2External,
    .convertExternal2Internal = &convertExternal2Internal
    
};

static double multiplierExtToMixer;
static double multiplierIntToExt;

static void help(void) {
    printf("Linear\n"
          );
}

static void regenerateMultipliers(void) {
    multiplierIntToExt = ((double)common_data.volume_max - (double)common_data.volume_min) / 100.0f;
    multiplierExtToMixer = ((double)common_data.volume_max - (double)common_data.volume_min) / (double)common_data.alsa_volume_range;
}

static void convertExternal2Mixer(double *volume) {
//    double volume_multiplier = ((double)common_data.volume_max - (double)common_data.volume_min) / (double)common_data.alsa_volume_range;
    *volume = ((*volume - (double)common_data.volume_min) / multiplierExtToMixer) + (double)common_data.alsa_volume_min;
}

static void convertMixer2Internal(long *volume) {
 // mixer == internal
}

static void convertInternal2External(long *volumeInternal, double *volumeExternal) {
    *volumeExternal = (*volumeInternal * multiplierIntToExt) + common_data.volume_min;
}

static void convertExternal2Internal(double *volumeExternal, long *volumeInternal) {
    *volumeInternal = (*volumeExternal - common_data.volume_min) / multiplierIntToExt;
}