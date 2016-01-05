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
#include <libconfig.h>
#include "verifyConfig.h"
#include "synchronator.h"
#include "common.h"
#include "interfaces.h"

/* I2C interface */
#include <fcntl.h>
#include <linux/i2c-dev.h>


static void help(void);
static int init(void);
static int deinit(void);
static int send(const void *buf, size_t count);
static void *listen(void *arg);

interface_t interface_i2c = {
    .name = "i2c",
    .help = &help,
    .init = &init,
    .deinit = &deinit,
    .send = &send,
    .listen = &listen
};

static pthread_mutex_t interfaceLock;
static int interfaceFD = -1;
static const char *i2cDevice;
static int i2cAddress;

static void help(void) {
    printf("I2C\n"
          );
} /* end help */

static int init(void) {

    int status;

	if(common_data.sync_2way) {
		syslog(LOG_ERR, "Setting 'sync_2way' to FALSE, not implemented for I2C");
		common_data.sync_2way = 0;
	}

    if(validateConfigString(&config, "i2c_device", &i2cDevice, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
	interfaceFD = open(i2cDevice, O_RDWR);
	if (interfaceFD < 0) {
		syslog(LOG_ERR, "Error opening device %s: %s (%i)\n", i2cDevice, strerror(errno), errno);
		return EXIT_FAILURE;
	}
	
    if(validateConfigInt(&config, "i2c_address", &i2cAddress, -1, 0, 0, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
	if (ioctl(interfaceFD, I2C_SLAVE, i2cAddress) == -1) {
		syslog(LOG_ERR, "input/output error at address %x: %s (%i)\n", i2cAddress, strerror(errno), errno);
		return EXIT_FAILURE;
	}
	
    if((status = pthread_mutex_init(&interfaceLock, NULL)) != 0) {
        syslog(LOG_ERR, "Failed to create interface mutex: %i", status);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
} /* end init */

#ifdef ENABLE_INTERFACE_RECOVERY
    static int reconnectInterface(void) {
        unsigned int waitPeriod[3] = {5,5,5}; // seconds
        int count;
    
        close(interfaceFD);
        interfaceFD = -1;
    
        for(count = 0; count < 3; count += 1) {
            interfaceFD = open(i2cDevice, O_RDWR);
            if (interfaceFD < 0) {
                syslog(LOG_WARNING, "Failed to reopen device, initiating sleep [%i]: %s (%i)\n", 
                    waitPeriod[count], strerror(errno), errno);
                sleep(waitPeriod[count]);
                continue;
            }
            if (ioctl(interfaceFD, I2C_SLAVE, i2cAddress) == -1) {
                syslog(LOG_WARNING, "input/output error, initiating sleep [%i]: %s (%i)\n", 
                    waitPeriod[count], strerror(errno), errno);
                close(interfaceFD);
                interfaceFD = -1;
                sleep(waitPeriod[count]);
                continue;
            }
        
            return EXIT_SUCCESS;
        }
    
        return EXIT_FAILURE;
    }
#endif // #ifdef ENABLE_INTERFACE_RECOVERY

static int send(const void *message, size_t messageLength) {

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&interfaceLock);
    
    if(write(interfaceFD, message, messageLength) == -1) {
    
#ifndef ENABLE_INTERFACE_RECOVERY
        pthread_mutex_unlock(&interfaceLock);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        syslog(LOG_ERR, "Failed to write to i2c device: %s (%i)", strerror(errno), errno);
        return EXIT_FAILURE;
#else
        syslog(LOG_WARNING, "Failed to write to i2c device, reopen attempted: %s (%i)", 
            strerror(errno), errno);
        if(reconnectInterface() == EXIT_FAILURE) {            
            pthread_mutex_unlock(&interfaceLock);
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
            syslog(LOG_WARNING, "Reopen failed");
            return EXIT_FAILURE;
        }
#endif // #ifdef ENABLE_INTERFACE_RECOVERY
	
    }
    
    pthread_mutex_unlock(&interfaceLock);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    return EXIT_SUCCESS;
} /* end send */

static void *listen(void *arg) {
/* function not implemented, perhaps if any use for it is found... */
	syslog(LOG_WARNING, "Listening for changes in I2C is not implemented.");
	
    pthread_exit(EXIT_SUCCESS);   
} /* listen() */

static int deinit(void) {

    /* Closing port */
    if(interfaceFD > 0) {
        close(interfaceFD);
	}
    pthread_mutex_destroy(&interfaceLock);
    
    return EXIT_SUCCESS;
} /* end deinit */
