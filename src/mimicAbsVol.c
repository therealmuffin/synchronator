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
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#ifndef TIME_DEFINED_TIMEOUT
    #include <unistd.h> // nanosleep only?
#endif
#include "synchronator.h"
#include "common.h"
#include "mimicAbsVol.h"

// create files volumeProcessLinear.c, volumeProcess
static int init(char *volMin, char *volPlus);
static int process(double volumeExternal);
static void deinit(void);
static void *setExternalVolume(void *unused);
static int resetVolume(void);

mimicAbsVol_t mimicAbsVol = {
    .name = "mimic",
    .init = &init,
    .deinit = &deinit,
    .process = &process
};

typedef struct {
    char *volMin;
    char *volPlus;
    int volMinLen;
    int volPlusLen;
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
static int actualExternalVolume;
static int requestedExternalVolume;

static int init(char *volMin, char *volPlus) { // timeout in milliseconds
    localSettings.volMin = volMin;
    localSettings.volPlus = volPlus;
    localSettings.volMinLen = strlen(localSettings.volMin);
    localSettings.volPlusLen = strlen(localSettings.volPlus);
    timestampLastMute = (struct timespec) { 0, 0 };
    
    resetInProgress = 0;
    int tempInt;
    
    validateConfigInt(&config, "volume.double_zero_interval", &localSettings.resetInterval, -1, 0, 30, 2);
    
    validateConfigInt(&config, "volume.timeout", &tempInt, -1, 0, 100, 0);
#ifdef TIME_DEFINED_TIMEOUT
    localSettings.timeout.tv_sec = 0;
    localSettings.timeout.tv_nsec = tempInt*1000000; // resolution ns -> ms
#else
    localSettings.timeout = tempInt*1000; // resolution μs -> ms
#endif
    
    /* Set signals to be blocked by threads */
    sigemptyset(&thread_sigmask);
    sigaddset(&thread_sigmask, SIGINT);
    sigaddset(&thread_sigmask, SIGTERM);
    
    if((tempInt = pthread_attr_init(&thread_attr)) != 0) {
        syslog(LOG_ERR, "Failed to initialize thread attributes: %i", tempInt);
        return EXIT_FAILURE;
    }
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);

    setExternalVolumeThread = thisThread = pthread_self();
    
    resetVolume();
    
    return EXIT_SUCCESS;
}

static int reinit(void) {
    resetVolume();
}

static int resetVolume(void) {
    int count = 0;
    
    while(count <= common_data.volumeMutationRange) {
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
    common_data.volume_level_status = actualExternalVolume = 0;
    resetInProgress = 0;
    
    return EXIT_SUCCESS;
}

static int process(double volumeExternal) {
    int status;
    if(resetInProgress)
        return EXIT_SUCCESS;
    
#ifdef TIME_DEFINED_TIMEOUT
    if(volumeExternal == 0) {
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

    requestedExternalVolume = volumeExternal;
    
    if(!pthread_equal(setExternalVolumeThread, thisThread) && 
    pthread_kill(setExternalVolumeThread, 0) == 0 && !resetInProgress)
        return EXIT_SUCCESS;
        
    if(!pthread_equal(setExternalVolumeThread, thisThread) && 
    (errno = pthread_join(setExternalVolumeThread, NULL)) != 0)
        syslog(LOG_WARNING, "Unable to join mimicAbsVol thread: %s\n", strerror(errno));
    
    if(status = pthread_create(&setExternalVolumeThread, &thread_attr, setExternalVolume, NULL)) {
        pthread_attr_destroy(&thread_attr);
        syslog(LOG_ERR, "Failed to open mimicAbsVol thread: %i", status);
        pthread_kill(mainThread, SIGTERM);
        pause();
    }
    return EXIT_SUCCESS;
}

static void *setExternalVolume(void *unused) {
    if(pthread_sigmask(SIG_BLOCK, &thread_sigmask, NULL) < 0) {
        syslog(LOG_ERR, "Failed to ignore signals in mimicAbsVol: %i", errno);
        pthread_kill(mainThread, SIGTERM);
        pause();
    }
    
    if(resetInProgress == 1)
        reinit();
    
    while(actualExternalVolume != requestedExternalVolume) {
        if(actualExternalVolume > requestedExternalVolume) {
            if(common_data.interface->send(localSettings.volMin, localSettings.volMinLen) == EXIT_FAILURE) {
                syslog(LOG_ERR, "Failed to send volume command");
                pthread_kill(mainThread, SIGTERM);
                pause();
            }
            actualExternalVolume--;
        }
        else {
            if(common_data.interface->send(localSettings.volPlus, localSettings.volPlusLen) == EXIT_FAILURE) {
                syslog(LOG_ERR, "Failed to send volume command");
                pthread_kill(mainThread, SIGTERM);
                pause();
            }
            actualExternalVolume++;
        }
        
        syslog(LOG_DEBUG, "Current volume mimicked (requested): %i (%i)", actualExternalVolume, requestedExternalVolume);
        
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
            syslog(LOG_WARNING, "Unable request cancelation for mimicAbsVol thread: %s", strerror(errno));
        if((errno = pthread_join(setExternalVolumeThread, NULL)) != 0)
            syslog(LOG_WARNING, "Unable to join mimicAbsVol thread: %s\n", strerror(errno));
        setExternalVolumeThread = thisThread;
    }
    pthread_attr_destroy(&thread_attr);
}