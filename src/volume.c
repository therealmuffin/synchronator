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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <syslog.h>
#include <libconfig.h>

#include "synchronator.h"
#include "common.h"
#include "verifyConfig.h"
#include "volume.h"


extern volumeCurve_t volumeCurveLinear;
extern volumeCurve_t volumeCurveLog;

static int initVolume(void);
static int processVolume(double *volumeExternal);
static void deinitVolume(void);
    
static volume_functions_t volume;
#ifdef TIME_DEFINED_TIMEOUT
    static struct timespec timestampCurrent;
#endif // #ifdef TIME_DEFINED_TIMEOUT

static volumeCurve_t *listVolumeCurves[] = {
    &volumeCurveLinear,
    &volumeCurveLog,
    NULL
};

volume_functions_t *getVolume(const char **name) {
    volumeCurve_t **curve;
    
    volume.init = &initVolume;
    volume.process = &processVolume;
    volume.deinit = &deinitVolume;

    // default to first interface
    if (!*name)
        *name = listVolumeCurves[0]->name;

    for (curve=listVolumeCurves; *curve; curve++)
        if (!strcasecmp(*name, (*curve)->name)) {
            volume.curve = (*curve)->name;
            volume.regenerateMultipliers = (*curve)->regenerateMultipliers;
            volume.convertExternal2Mixer = (*curve)->convertExternal2Mixer;
            volume.convertMixer2Internal = (*curve)->convertMixer2Internal;
            volume.convertInternal2External = (*curve)->convertInternal2External;
            return &volume;
        }

    return NULL;
}

static int initVolume(void) {
    /* Initialize some values*/
    common_data.volume_level_status = -1;
#ifdef TIME_DEFINED_TIMEOUT 
    common_data.timestampLastRX = common_data.timestampLastTX = (struct timespec) { 0, 0 };
#else
    common_data.volume_out_timeout = common_data.volume_in_timeout = 0;
#endif // #ifdef TIME_DEFINED_TIMEOUT
    common_data.alsa_volume_max = common_data.alsa_volume_min = 0;

    config_setting_t *conf_setting = NULL;
    int int_setting = -1;

    /* Default values. Will change as needed */
    common_data.volume_min = 0;
    common_data.volume_max = 100;
    
    if(config_lookup(&config, "volume") == NULL) {
        syslog(LOG_ERR, "[Error] Category 'volume' is not present");
        return EXIT_FAILURE;
    }
    validateConfigBool(&config, "volume.discrete", &common_data.discrete_volume, 1);
    
    if(!common_data.discrete_volume) {
        if(strcmp("linear", common_data.volume->curve) != 0)
            syslog(LOG_ERR, "[Error] Volume curve must be 'linear' when using relative volume control");
        if(validateConfigInt(&config, "volume.multiplier", &common_data.nd_vol_multiplier, -1, 1, 100, 1) == EXIT_FAILURE)
            return EXIT_FAILURE;
        validateConfigInt(&config, "volume.range", &common_data.volumeMutationRange, -1, 0, 1000, 0);
    }
    if(common_data.discrete_volume && validateConfigInt(&config, "volume.min", 
    &common_data.volume_min, -1, 0, 0, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;    
    if(common_data.discrete_volume && validateConfigInt(&config, "volume.max", 
    &common_data.volume_max, -1, 0, 0, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    if(common_data.mod->volumeResponse) {
        if(common_data.mod->volumeResponse->init() == EXIT_FAILURE)
            return EXIT_FAILURE;
    }
        
    common_data.initial_volume_min = common_data.volume_min;
    common_data.initial_volume_max = common_data.volume_max;
    
    return EXIT_SUCCESS;
}

static int processVolume(double *volumeExternal) {
    // If true, volume hasn't been fully initialized yet.
    if(common_data.alsa_volume_max == common_data.alsa_volume_min)
        return EXIT_SUCCESS;
    
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    if(pthread_mutex_trylock(&lockProcess) != 0) {
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        syslog(LOG_DEBUG, "Skipping incoming volume command");
        return EXIT_SUCCESS;
    }
    
#ifdef TIME_DEFINED_TIMEOUT
    int timeout;
    clock_gettime(CLOCK_MONOTONIC, &timestampCurrent);
    if((timeout = timestampCurrent.tv_sec - common_data.timestampLastTX.tv_sec) <= DEFAULT_PROCESS_TIMEOUT_IN) {
        syslog(LOG_DEBUG, "Incoming volume level processing timeout (completed): %i/%i", timeout, DEFAULT_PROCESS_TIMEOUT_IN);
#else
    if(common_data.volume_in_timeout > 0) {
        common_data.volume_in_timeout--;
        syslog(LOG_DEBUG, "Incoming volume level processing timeout: %i", common_data.volume_in_timeout);
#endif // #ifdef TIME_DEFINED_TIMEOUT
    
        pthread_mutex_unlock(&lockProcess);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        return EXIT_SUCCESS;
    }
    
    /* add adjustment if input and output volume range is different
     * eg cp800: (receiverVolume + 94) * 0.926 (according to control4 driver). */
     
    double volume_level = *volumeExternal;
    if(common_data.mod->volumeResponse)
        common_data.mod->volumeResponse->process(&volume_level);
    
    /* Adjust the volume ranges according to current amplifier volume level */
    if(volume_level > common_data.volume_max || common_data.volume_max != common_data.initial_volume_max 
    && volume_level < common_data.volume_max) {
        if(volume_level > common_data.initial_volume_max)
            common_data.volume_max = (int)ceil(volume_level);
        else
            common_data.volume_max = common_data.initial_volume_max;

        common_data.volume->regenerateMultipliers();
        
        syslog(LOG_DEBUG, "Volume upper range has been adjusted: %i -> %i", 
        common_data.initial_volume_max, common_data.volume_max);
    }
    else if(volume_level < common_data.volume_min || common_data.volume_min != common_data.initial_volume_min 
    && volume_level > common_data.volume_min) {
        if(volume_level < common_data.initial_volume_min)
            common_data.volume_min = (int)floor(volume_level);
        else
            common_data.volume_min = common_data.initial_volume_min;

        common_data.volume->regenerateMultipliers();
        
        syslog(LOG_DEBUG, "Volume level lower range has been adjusted: %i -> %i",
        common_data.initial_volume_min, common_data.volume_min);
    }
    else if((int)volume_level == (int)common_data.volume_level_status) {
#ifndef TIME_DEFINED_TIMEOUT
        if(common_data.volume_out_timeout > 0)
            common_data.volume_out_timeout--;
#endif // #ifndef TIME_DEFINED_TIMEOUT
            
        pthread_mutex_unlock(&lockProcess);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

        return EXIT_SUCCESS;
    }
    
    common_data.volume_level_status = volume_level;
    common_data.volume->convertExternal2Mixer(&volume_level);
    
    setMixer((int)volume_level); // removed ceil(), caused erange due to rounding errors (if necessary convert to ceil(float) to eliminate small rounding errors?).
    
#ifdef TIME_DEFINED_TIMEOUT
    clock_gettime(CLOCK_MONOTONIC , &common_data.timestampLastRX);
#else
    common_data.volume_out_timeout = DEFAULT_PROCESS_TIMEOUT_OUT;
#endif // #ifdef TIME_DEFINED_TIMEOUT
    
    syslog(LOG_DEBUG, "Volume level mutation (ext. initiated): ext. (int.): %.2f (%.2f)", common_data.volume_level_status, volume_level);
    
    pthread_mutex_unlock(&lockProcess);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    return EXIT_SUCCESS;
} /* end processVolume */


static void deinitVolume(void) {
    if(common_data.mod->volumeResponse != NULL)
        common_data.mod->volumeResponse->deinit();
}
