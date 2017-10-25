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
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#ifndef TIME_DEFINED_TIMEOUT
    #include <unistd.h> // nanosleep only?
#endif
#include "synchronator.h"
#include "common.h"
#include "mixer.h"
#include "smoothVolume.h"

// create files volumeProcessLinear.c, volumeProcess
static int init(int (*setDescreteVolume)(double *externalVolume), char *volMin, char *volPlus);
static int reinit(void);
static int process(long *volumeInternal);
static void deinit(void);
static void *setExternalVolume(void *unused);
static int resetVolume(void);

smoothVolume_t smoothVolume = {
    .name = "smooth_volume",
    .init = &init,
    .deinit = &deinit,
    .process = &process
};

typedef struct {
    char *volMin;
    char *volPlus;
    int volMinLen;
    int volPlusLen;
    
    int (*setDescreteVolume)(double *volumeExternal);
    
    float volumeMutation;
    int volumeMutationRange;
    
    int resetInterval; // seconds
#ifdef TIME_DEFINED_TIMEOUT
    struct timespec timeout; //milliseconds
#else
    useconds_t timeout;
#endif
} localSettings_t;

static localSettings_t localSettings;
static pthread_t setExternalVolumeThread, thisThread;
static pthread_attr_t thread_attr;
static sigset_t thread_sigmask;
#ifdef TIME_DEFINED_TIMEOUT
    static struct timespec timestampLastMute;
    static struct timespec timestampCurrent;
#endif
static int resetInProgress;
static int reinitInProgress;
static double defaultVolume;
static double actualVolume;
static double requestedVolume;

static int init(int (*setDescreteVolume)(double *externalVolume), char *volMin, char *volPlus) {
    timestampLastMute = (struct timespec) { 0, 0 };
    resetInProgress = reinitInProgress = 0;
    
#ifdef TIME_DEFINED_TIMEOUT
    localSettings.timeout.tv_sec = 0;
    localSettings.timeout.tv_nsec = common_data.volume_timeout*1000000; // resolution ns -> ms
#else
    localSettings.timeout = common_data.volume_timeout*1000; // resolution μs -> ms
#endif
    
    /* Set signals to be blocked by threads */
    sigemptyset(&thread_sigmask);
    sigaddset(&thread_sigmask, SIGINT);
    sigaddset(&thread_sigmask, SIGTERM);
    
    int tempInt = -1;
    if((tempInt = pthread_attr_init(&thread_attr)) != 0) {
        syslog(LOG_ERR, "Failed to initialize thread attributes: %i", tempInt);
        return EXIT_FAILURE;
    }
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);

    setExternalVolumeThread = thisThread = pthread_self();
    
    if(common_data.discrete_volume) {
        localSettings.setDescreteVolume = setDescreteVolume;
        int precision = 0;
        int tempInt;
        validateConfigInt(&config, "volume.mutation", &tempInt, -1, 1, 1000, 100);
        if(strcmp(common_data.process->name, "ascii") == 0) {
            config_lookup_int(&config, "volume.precision", &precision);
        }
        
        localSettings.volumeMutation = (tempInt / 100);
        
        // log uses internal linear volume curve as reference and can't handle float
        if(precision == 0 || strcmp(common_data.volume->curve, "log") == 0) {
            localSettings.volumeMutation = (int)localSettings.volumeMutation;
            if(localSettings.volumeMutation == 0) localSettings.volumeMutation = 1;
        }
        else if(precision == 1) {
            localSettings.volumeMutation = (int)(10*localSettings.volumeMutation)/10;
            if(localSettings.volumeMutation == 0) localSettings.volumeMutation = 0.1;
        }
    }
    else {
        // replace the mixer reinit function with 'wrap' function
        common_data.reinitVolume = &reinit;
        localSettings.volMin = volMin;
        localSettings.volPlus = volPlus;
        localSettings.volMinLen = strlen(localSettings.volMin);
        localSettings.volPlusLen = strlen(localSettings.volPlus);
        
        validateConfigInt(&config, "volume.range", &localSettings.volumeMutationRange, -1, 1, 1000, -1);
        
        validateConfigInt(&config, "volume.double_zero_interval", &localSettings.resetInterval, -1, 0, 30, 2);
        resetVolume();
        
        if(validateConfigInt(&config, "volume.max", &common_data.volume_max, -1, 1, 
        localSettings.volumeMutationRange, localSettings.volumeMutationRange) == EXIT_FAILURE)
            return EXIT_FAILURE;
    }
    // if exit failure, do not user smoothvolume, otherwise do
    return EXIT_SUCCESS;
}

static int reinit(void) {
    if(resetInProgress)
        return EXIT_SUCCESS;
    
    reinitInProgress = resetInProgress = 1;
    pthread_cancel(setExternalVolumeThread);
    syslog(LOG_DEBUG, "Initiating volume reinitialisation");
    int status;
    
    if(common_data.defaultExternalVolume >= common_data.initial_volume_min) {
        defaultVolume = common_data.defaultExternalVolume;
        common_data.volume->convertExternal2Mixer(&defaultVolume);
    }
    
    if(!pthread_equal(setExternalVolumeThread, thisThread) && 
    pthread_kill(setExternalVolumeThread, 0) == 0 && !resetInProgress) // will always fail -> resetInProgress is 1
        return EXIT_SUCCESS;
        
    if(!pthread_equal(setExternalVolumeThread, thisThread) && 
    (errno = pthread_join(setExternalVolumeThread, NULL)) != 0)
        syslog(LOG_WARNING, "Unable to join smoothVolume thread: %s\n", strerror(errno));
    
    if(status = pthread_create(&setExternalVolumeThread, &thread_attr, setExternalVolume, NULL)) {
        pthread_attr_destroy(&thread_attr);
        syslog(LOG_ERR, "Failed to open smoothVolume thread: %i", status);
        pthread_kill(mainThread, SIGTERM);
        pause();
    }
    
    return EXIT_SUCCESS;
}

static int resetVolume(void) {
    int count = 0;
    
    while(count <= localSettings.volumeMutationRange) {
        if(common_data.interface->send(localSettings.volMin, localSettings.volMinLen) == EXIT_FAILURE) {
            syslog(LOG_ERR, "Failed to send volume command");
            pthread_kill(mainThread, SIGTERM);
            pause();
        }
            
#ifdef TIME_DEFINED_TIMEOUT
        clock_nanosleep(CLOCK_MONOTONIC, 0, &localSettings.timeout, NULL);
#else
        nanosleep(localSettings.timeout);
#endif
        count++;
    }
    
    if(setMixer(common_data.alsa_volume_min) == EXIT_FAILURE)
        return EXIT_FAILURE;
    common_data.volume_level_status = actualVolume = 0;
    
    if(reinitInProgress && common_data.defaultExternalVolume >= common_data.initial_volume_min) {
        setMixer((int)defaultVolume);//? == EXIT_FAILURE;
    }
    
    reinitInProgress = resetInProgress = 0;
        
    return EXIT_SUCCESS;
}

static int process(long *volumeInternal) {
    int status;
    if(resetInProgress)
        return EXIT_SUCCESS;
    
#ifdef TIME_DEFINED_TIMEOUT
    if(!common_data.discrete_volume && *volumeInternal == 0) { // is this ok? used to be volumeExternal
        clock_gettime(CLOCK_MONOTONIC , &timestampCurrent);
        if(!localSettings.resetInterval || (timestampCurrent.tv_sec - timestampLastMute.tv_sec) <= localSettings.resetInterval) {
            syslog(LOG_DEBUG, "Initiating volume reset");
            resetInProgress = 1;
            
            pthread_cancel(setExternalVolumeThread);
        }
        else
            timestampLastMute = timestampCurrent;
    }
#endif
    if(strcmp(common_data.volume->curve, "log") == 0) {
        requestedVolume = *volumeInternal;
    }
    else {
        common_data.volume->convertInternal2External(volumeInternal, &requestedVolume);
    }
    
    if(common_data.discrete_volume == 0) {
        requestedVolume = (int)requestedVolume;
    }
        
    // after setting resetInProgress it can only be catched once
    if(!pthread_equal(setExternalVolumeThread, thisThread) && 
    pthread_kill(setExternalVolumeThread, 0) == 0 && !resetInProgress)
        return EXIT_SUCCESS;
    
    // sync local external volume status with global one.// 
    if(common_data.discrete_volume == 1) {
        if(strcmp(common_data.volume->curve, "log") == 0) {
            long testvar = (long)actualVolume;
            common_data.volume->convertExternal2Internal(&common_data.volume_level_status, &testvar);
            syslog(LOG_INFO, "Actual incoming log volume: %i (%.2f - %.2f)\n", testvar, actualVolume, common_data.volume_level_status);
            actualVolume = (double)testvar;
        }
        else
            actualVolume = common_data.volume_level_status;
    }
        
    if(!pthread_equal(setExternalVolumeThread, thisThread) && 
    (errno = pthread_join(setExternalVolumeThread, NULL)) != 0)
        syslog(LOG_WARNING, "Unable to join smoothVolume thread: %s\n", strerror(errno));
    
    if(status = pthread_create(&setExternalVolumeThread, &thread_attr, setExternalVolume, NULL)) {
        pthread_attr_destroy(&thread_attr);
        syslog(LOG_ERR, "Failed to open smoothVolume thread: %i", status);
        pthread_kill(mainThread, SIGTERM);
        pause();
    }
    return EXIT_SUCCESS;
}

static void *setExternalVolume(void *unused) {
    double actualExternalVolume;
    if(pthread_sigmask(SIG_BLOCK, &thread_sigmask, NULL) < 0) {
        syslog(LOG_ERR, "Failed to ignore signals in smoothVolume: %i", errno);
        pthread_kill(mainThread, SIGTERM);
        pause();
    }
    
    if(resetInProgress == 1)
        resetVolume();
    
    while(actualVolume != requestedVolume) {
        if(common_data.discrete_volume) {
            if(actualVolume > requestedVolume) {
                if((actualVolume - requestedVolume) > localSettings.volumeMutation) {
                    actualVolume -= localSettings.volumeMutation;
                }
                else {
                    actualVolume = requestedVolume;
                }
            }
            else {
                if((requestedVolume - actualVolume) > localSettings.volumeMutation) {
                    actualVolume += localSettings.volumeMutation;
                }
                else {
                    actualVolume = requestedVolume;
                }
            }
            if(strcmp(common_data.volume->curve, "log") == 0) {
                long x = (long)actualVolume;
                common_data.volume->convertInternal2External(&x, &actualExternalVolume);
            }
            else
                actualExternalVolume = actualVolume;
            
            localSettings.setDescreteVolume(&actualExternalVolume);
        }
        else {
            char *volCurrent;
            int volLenCurrent = 0;
            if(actualVolume > requestedVolume) {
                volCurrent = localSettings.volMin;
                volLenCurrent = localSettings.volMinLen;
                actualVolume--;
            }
            else {
                volCurrent = localSettings.volPlus;
                volLenCurrent = localSettings.volPlusLen;
                actualVolume++;
            }
            
            if(common_data.interface->send(volCurrent, volLenCurrent) == EXIT_FAILURE) {
                syslog(LOG_ERR, "Failed to send volume command");
                pthread_kill(mainThread, SIGTERM);
                pause();
            }
        }
        
        syslog(LOG_DEBUG, "Smooth volume level status: current (requested): %.2f (%.2f)", actualVolume, requestedVolume);
        
#ifdef TIME_DEFINED_TIMEOUT
        clock_nanosleep(CLOCK_MONOTONIC, 0, &localSettings.timeout, NULL);
#else
        nanosleep(localSettings.timeout);
#endif
    }
    pthread_exit(NULL);
}

static void deinit(void) {
    if(!pthread_equal(setExternalVolumeThread, thisThread)) {
        if(pthread_kill(setExternalVolumeThread, 0) == 0 && pthread_cancel(setExternalVolumeThread) != 0)
            syslog(LOG_WARNING, "Unable request cancelation for smoothVolume thread: %s", strerror(errno));
        if((errno = pthread_join(setExternalVolumeThread, NULL)) != 0)
            syslog(LOG_WARNING, "Unable to join smoothVolume thread: %s\n", strerror(errno));
        setExternalVolumeThread = thisThread;
    }
    pthread_attr_destroy(&thread_attr);
}