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
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <pthread.h>
#include <libconfig.h>
#include <lirc/lirc_client.h>

#include "verifyConfig.h"
#include "synchronator.h"
#include "common.h"
#include "interfaces.h"

static void help(void);
static int init(void);
static int deinit(void);
static int send(const void *message, size_t messageLength);
static void *listen(void *arg);

interface_t interface_lirc = {
    .name = "lirc",
    .help = &help,
    .init = &init,
    .deinit = &deinit,
    .reply = &send,
    .send = &send,
    .listen = &listen
};

static pthread_mutex_t interfaceLock;
static int interfaceFD = -1;
static const char *lircSocket = NULL;
static const char *remoteName = NULL;

static void help(void) {
    printf("LIRC\n"
          );
} /* end help */

static int init(void) {
    int status;
    
    if(common_data.sync_2way) {
        syslog(LOG_INFO, "Setting 'sync_2way' to FALSE, not implemented for LIRC");
        common_data.sync_2way = 0;
    }
    if(strcmp(common_data.dataType, "ascii") != 0) {
        syslog(LOG_ERR, "Setting 'data_type' must be set to ASCII for interface LIRC");
        return EXIT_FAILURE;
    }
    
    if(validateConfigString(&config, "lirc_socket", &lircSocket, -1) == EXIT_FAILURE) {
        syslog(LOG_DEBUG, "LIRC socket not set, using default socket");
    }
    if(validateConfigString(&config, "remote_name", &remoteName, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    interfaceFD = lirc_get_local_socket(NULL, 1);
    if (interfaceFD < 0) {
        lircSocket = lircSocket ? lircSocket : getenv("LIRC_SOCKET_PATH");
        lircSocket = lircSocket ? lircSocket : "(undefined)";
        syslog(LOG_ERR, "Error opening LIRC socket %s: %s (%i)\n", lircSocket, strerror(errno), errno);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
} /* end init */

static int send(const void *message, size_t messageLength) {
    
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&interfaceLock);
    
    if (lirc_send_one(interfaceFD, remoteName, (const char *) message) == -1) {
        pthread_mutex_unlock(&interfaceLock);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        syslog(LOG_ERR, "Failed to write to LIRC socket: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    
    pthread_mutex_unlock(&interfaceLock);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    return EXIT_SUCCESS;
} /* end send */

static void *listen(void *arg) {
/* function not implemented, seems there are better methods of doing this */
    syslog(LOG_WARNING, "Listening for changes in LIRC is not implemented.");
    
    pthread_exit(EXIT_SUCCESS);
} /* listen() */

static int deinit(void) {
    if(interfaceFD > 0)
        close(interfaceFD);
    
    pthread_mutex_destroy(&interfaceLock);
    
    return EXIT_SUCCESS;
} /* end deinit */
