/****************************************************************************
 * Copyright (C) 2015 Muffinman
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
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <alsa/asoundlib.h>

#include "volume_mapping.h"

#include "common.h"
#include "mixer.h"


static int reinit(void);

static snd_mixer_t *snd_handle = NULL;
static snd_mixer_elem_t *snd_elem = NULL;
static int volumeNormalized = 0;

int initMixer(void) {
    common_data.reinitVolume = &reinit;
    snd_mixer_selem_id_t* snd_sid;
    snd_mixer_selem_id_alloca(&snd_sid);

    const char* mix_dev = "hw:Dummy";
    const char* mix_name = "Master";
    int mix_index = 0;
    int status = -2;
    
    if(config_lookup_string(&config, "mixer_device", &mix_dev) == CONFIG_TRUE) {
        syslog(LOG_INFO, "[OK] mixer_device: %s", mix_dev);
    }
    if(config_lookup_string(&config, "mixer_name", &mix_name) == CONFIG_TRUE) {
        syslog(LOG_INFO, "[OK] mixer_name: %s", mix_name);
    }
    if(config_lookup_int(&config, "mixer_index", &mix_index) == CONFIG_TRUE) {
        syslog(LOG_INFO, "[OK] mixer_index: %i", mix_index);
    }
    validateConfigBool(&config, "mixer_normalize", &volumeNormalized, 0);

    /* sets simple-mixer index and name */
    snd_mixer_selem_id_set_index(snd_sid, mix_index);
    snd_mixer_selem_id_set_name(snd_sid, mix_name);
    
    if((status = snd_mixer_open(&snd_handle, 0)) < 0) {
        syslog(LOG_ERR, "Failed to open mixer: %s (%i)", snd_strerror(status), status);
        return EXIT_FAILURE;
        pause();
    }
    if((status = snd_mixer_attach(snd_handle, mix_dev)) < 0) {
        syslog(LOG_ERR, "Failed to attach device to mixer: %s (%i)", 
            snd_strerror(status), status);
        return EXIT_FAILURE;
        pause();
    }
    if((status = snd_mixer_selem_register(snd_handle, NULL, NULL)) < 0) {
        syslog(LOG_ERR, "Failed to register mixer element class: %s (%i)", 
            snd_strerror(status), status);
        return EXIT_FAILURE;
        pause();
    }
    if((status = snd_mixer_load(snd_handle)) < 0) {
        syslog(LOG_ERR, "Failed to load mixer elements: %s (%i)", 
            snd_strerror(status), status);
        return EXIT_FAILURE;
        pause();
    }
    if((snd_elem = snd_mixer_find_selem(snd_handle, snd_sid)) == NULL) {
        syslog(LOG_ERR, "Failed to find mixer element");
        return EXIT_FAILURE;
        pause();
    }
    if(volumeNormalized) {
        common_data.alsa_volume_min = 0;
        common_data.alsa_volume_max = 100;
    }
    else {
        snd_mixer_selem_get_playback_volume_range(snd_elem, &common_data.alsa_volume_min, 
            &common_data.alsa_volume_max);
    }
    common_data.alsa_volume_range = common_data.alsa_volume_max - common_data.alsa_volume_min;
    syslog(LOG_DEBUG, "Alsa volume range, min, max: %i, %i, %i", common_data.alsa_volume_range, 
        common_data.alsa_volume_min, common_data.alsa_volume_max);
    
    return EXIT_SUCCESS;
}

static int reinit(void) {
    if(common_data.defaultExternalVolume < common_data.initial_volume_min)
        return EXIT_SUCCESS;
    
    double defaultVolume = common_data.defaultExternalVolume;
    common_data.volume->convertExternal2Mixer(&defaultVolume);
    setMixer((int)defaultVolume);
    
    return EXIT_SUCCESS;
}

int getMixer(long *volume) {
    int status = -2;

    if(volumeNormalized) {
        *volume = 100 * get_normalized_playback_volume(snd_elem, 0);
    }
    else {
        if((snd_mixer_selem_get_playback_volume(snd_elem, 0, volume)) < 0) {
            syslog(LOG_ERR, "Failed to get playback volume: %s (%i)", 
                snd_strerror(status), status);
            return EXIT_FAILURE;
        }
        /* make the value bound to 100 */  
        // perhaps bound to 1000 to increase precision? MPD 0-100, Airplay -30-0 (-144==mute)
        *volume = 100 * (*volume - common_data.alsa_volume_min) / common_data.alsa_volume_range;
    }

    common_data.volume->convertMixer2Internal(volume);
    
    return EXIT_SUCCESS;
}

int setMixer(int volume) {

    if(volumeNormalized) {
        if((set_normalized_playback_volume(snd_elem, 0.01 * volume, -1)) != 0) {
            syslog(LOG_WARNING, "Setting mixer failed");
            return EXIT_FAILURE;
            syslog(LOG_ERR, "Volume (%i)", volume);
        }
    }
    else {
        if((snd_mixer_selem_set_playback_volume_all(snd_elem, volume)) != 0) {
            syslog(LOG_WARNING, "Setting mixer failed");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

void *watchMixer(void *arg) {
    sigset_t *thread_sigmask = (sigset_t *)arg;
    if(pthread_sigmask(SIG_BLOCK, thread_sigmask, NULL) < 0) {
        syslog(LOG_ERR, "Failed to ignore signals in watchMixer: %i", errno);
        pthread_kill(mainThread, SIGTERM);
        pause();
    }
    
    long volume = -1;
    long volume_old = -1;
    int status = -2;
    int events = -1;
    
    // initialize volume at amp end under conditions
    if(!common_data.sync_2way && common_data.discrete_volume) {
    
        if(getMixer(&volume) == EXIT_FAILURE) {
            pthread_kill(mainThread, SIGTERM);
            pause();
        }
    
        if(common_data.process->compileVolumeCommand(&volume) == EXIT_FAILURE) {
            pthread_kill(mainThread, SIGTERM);
            pause();
        }
    }
    
    while(1) {
        /* !!!! uses poll(), see definition at line 639 of mixer.c. 
         * -> cancellation point !!!! */
        if((status = snd_mixer_wait(snd_handle, -1)) < 0) {
            syslog(LOG_ERR, "Failed waiting for mixer event: %s (%i)", 
                snd_strerror(status), status);
            pthread_kill(mainThread, SIGTERM);
            pause();
        }
        
        /* clears all pending mixer events */
        events = snd_mixer_handle_events(snd_handle);
        /* We're relying here on a stereo mixer from snd_dummy. This causes
         * mixer events to be doubled. Therefore check if the sound
         * level has changed and if not continue. */
        volume_old = volume;
        
        if(getMixer(&volume) == EXIT_FAILURE) {
            pthread_kill(mainThread, SIGTERM);
            pause();
        }
        if(volume == volume_old)
            continue;
        
        if(common_data.process->compileVolumeCommand(&volume) == EXIT_FAILURE) {
            pthread_kill(mainThread, SIGTERM);
            pause();
        }
        
        } /*  End infinite while loop */

    /* Shouldn't have gotten here */
    pthread_kill(mainThread, SIGTERM);
    pause();
} /* end watchMixer */

void deinitMixer(void) {
    /* Freeing resources */
    if(snd_handle != NULL)
        snd_mixer_close(snd_handle);
}