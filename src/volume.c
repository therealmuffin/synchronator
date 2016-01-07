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
#include <syslog.h>
#include <math.h> // used for ceilf()

/* External libraries  */
#include <libconfig.h>
#include <pthread.h>

/* Local headers*/
#include "synchronator.h"
#include "common.h"
#include "verifyConfig.h"
#include "volume.h"

extern volumeCurve_t volumeCurveLinear;
extern volumeCurve_t volumeCurveLog;

static volumeCurve_t *listVolumeCurves[] = {
	&volumeCurveLinear,
	&volumeCurveLog,
    NULL
};


static volumeCurve_t *getVolumeCurve(const char *name) {
    volumeCurve_t **curve;

    // default to first interface
    if (!name)
        return listVolumeCurves[0];

    for (curve=listVolumeCurves; *curve; curve++)
        if (!strcasecmp(name, (*curve)->name))
            return *curve;

    return NULL;
}

int initVolume(void) {
    /* Initialize some values*/
    common_data.volume_level_status = -1;
    common_data.volume_out_timeout = common_data.volume_in_timeout = 0;
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
    if(!common_data.discrete_volume && validateConfigInt(&config, "volume.multiplier", 
    &common_data.nd_vol_multiplier, -1, 1, 100, 1) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    if(common_data.discrete_volume && validateConfigInt(&config, "volume.min", 
    &common_data.volume_min, -1, 0, 0, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;    
    if(common_data.discrete_volume && validateConfigInt(&config, "volume.max", 
    &common_data.volume_max, -1, 0, 0, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;
	
    if(common_data.discrete_volume) {
    	validateConfigInt(&config, "volume.response.pre_offset", &common_data.responsePreOffset, -1, 0, 0, 0);
    	validateConfigInt(&config, "volume.response.post_offset", &common_data.responsePostOffset, -1, 0, 0, 0);
    	validateConfigDouble(&config, "volume.response.multiplier", &common_data.responseMultiplier, -1, 0, 0, 1);
    	validateConfigBool(&config, "volume.response.invert_multiplier", &int_setting, 0);
    	if(int_setting)
    		common_data.responseMultiplier = 1/common_data.responseMultiplier;
    	if(common_data.responsePreOffset || common_data.responsePostOffset || common_data.responseMultiplier != 1)
    		common_data.responseDivergent = 1;
    }
        
    common_data.initial_volume_min = common_data.volume_min;
    common_data.initial_volume_max = common_data.volume_max;
    
    /* Set and initialise communication interface */
	const char *volumeCurve = NULL;
	config_lookup_string(&config, "volume.curve", &volumeCurve);
    if(!(common_data.volumeCurve = getVolumeCurve(volumeCurve))) {
    	syslog(LOG_ERR, "[Error] Volume curve is not recognised: %s", volumeCurve);
        return EXIT_FAILURE;
    };
       
}

int processVolume(double *action_lookup) {
    // If true, volume hasn't been fully initialized yet.
    if(common_data.alsa_volume_max == common_data.alsa_volume_min)
        return EXIT_SUCCESS;
    
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    if(pthread_mutex_trylock(&lockProcess) != 0) {
    	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        syslog(LOG_DEBUG, "Skipping incoming volume command");
        return EXIT_SUCCESS;
    }
    
    if(common_data.volume_in_timeout > 0) {
        common_data.volume_in_timeout--;
        
        pthread_mutex_unlock(&lockProcess);
    	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        syslog(LOG_DEBUG, "Incoming volume level processing timeout: %i", common_data.volume_in_timeout);
        return EXIT_SUCCESS;
    }
    
    /* add adjustment if input and output volume range is different
     * eg cp800: (receiverVolume + 94) * 0.926 (according to control4 driver). */
     
    double volume_level = *action_lookup;
	if(common_data.responseDivergent)
		volume_level = (volume_level + common_data.responsePreOffset) * common_data.responseMultiplier + common_data.responsePostOffset;
    
    /* Adjust the volume ranges according to current amplifier volume level */
    if(volume_level > common_data.volume_max || common_data.volume_max != common_data.initial_volume_max 
    && volume_level < common_data.volume_max) {
        if(volume_level > common_data.initial_volume_max)
            common_data.volume_max = (int)ceil(volume_level);
        else
            common_data.volume_max = common_data.initial_volume_max;

        common_data.volumeCurve->regenerateMultipliers();
        
        syslog(LOG_DEBUG, "Volume upper range has been adjusted: %i -> %i", 
        common_data.initial_volume_max, common_data.volume_max);
    }
    else if(volume_level < common_data.volume_min || common_data.volume_min != common_data.initial_volume_min 
    && volume_level > common_data.volume_min) {
        if(volume_level < common_data.initial_volume_min)
            common_data.volume_min = (int)floor(volume_level);
        else
            common_data.volume_min = common_data.initial_volume_min;

        common_data.volumeCurve->regenerateMultipliers();
        
        syslog(LOG_DEBUG, "Volume level lower range has been adjusted: %i -> %i",
        common_data.initial_volume_min, common_data.volume_min);
    }
	else if((int)volume_level == (int)common_data.volume_level_status) {
        if(common_data.volume_out_timeout > 0)
            common_data.volume_out_timeout--;
            
        pthread_mutex_unlock(&lockProcess);
       	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

        return EXIT_SUCCESS;
    }
    
    common_data.volume_level_status = volume_level;
    common_data.volumeCurve->convertExternal2Mixer(&volume_level);
    
    if(setMixer((int)volume_level) != 0) // removed ceil(), caused erange due to rounding errors (if necessary convert to ceil(float) to eliminate small rounding errors?).
        syslog(LOG_WARNING, "Setting mixer failed");
    common_data.volume_out_timeout = DEFAULT_PROCESS_TIMEOUT_OUT;
    
    syslog(LOG_DEBUG, "Volume multiplier, max and min: %.4f, %ld, and %ld", common_data.multiplierExtToMixer, common_data.volume_max, common_data.volume_min);
    syslog(LOG_DEBUG, "Volume level mutation (ext. initiated): ext. (int.): %.2f (%.2f)", common_data.volume_level_status, volume_level);
    
    pthread_mutex_unlock(&lockProcess);
   	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    return EXIT_SUCCESS;
} /* end processVolume */


void deinitVolume() {

}
