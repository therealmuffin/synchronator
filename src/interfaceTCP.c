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
#include <unistd.h> 
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <string.h>
#include <libconfig.h>
#include "verifyConfig.h"
#include "synchronator.h"
#include "common.h"
#include "interfaces.h"

/* TCP/IP interface */
#include <sys/socket.h>
#include <netdb.h>
//#include <arpa/inet.h>
//#include <netinet/in.h>

static void help(void);
static int init(void);
static int deinit(void);
static int sendTCP(const void *buf, size_t count);
static void *listenTCP(void *arg);

interface_t interface_tcp = {
    .name = "tcp",
    .help = &help,
    .init = &init,
    .deinit = &deinit,
    .send = &sendTCP,
    .listen = &listenTCP
};

static pthread_mutex_t interfaceLock;
struct addrinfo *addrinfoResults, *addresInfo;
static int interfaceFD = -1;

static void help(void) {
    printf("TCP (beta)\n"
          );
} /* end help */

static int init(void) {

    const char *tcpIP;
    char tcpPortChar[5];
    int status, tcpPortInt;

    if(validateConfigString(&config, "tcp_address", &tcpIP, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;

    if(validateConfigInt(&config, "tcp_port", &tcpPortInt, -1, 1, 99999, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;
    snprintf (tcpPortChar, sizeof(tcpPortChar), "%i",tcpPortInt);

    struct addrinfo addrinfoHints;
    
    memset(&addrinfoHints, 0, sizeof addrinfoHints);
    addrinfoHints.ai_family = AF_UNSPEC;
    addrinfoHints.ai_socktype = SOCK_STREAM;

    if((status = getaddrinfo(tcpIP, tcpPortChar, &addrinfoHints, &addrinfoResults)) != 0) {
        syslog(LOG_ERR, "Failed to get IP info: %s", gai_strerror(status));
        return EXIT_FAILURE;
    }
    
    // loop through all the results and connect to the first we can
    for(addresInfo = addrinfoResults; addresInfo != NULL; addresInfo = addresInfo->ai_next) {
        if ((interfaceFD = socket(addresInfo->ai_family, addresInfo->ai_socktype, 
        addresInfo->ai_protocol)) == -1) {
            continue;
        }

        if (connect(interfaceFD, addresInfo->ai_addr, addresInfo->ai_addrlen) == -1) {
            close(interfaceFD);
            continue;
        }

        break;
    }

    if (addresInfo == NULL) {
        syslog(LOG_ERR, "Failed to connect socket");
        return EXIT_FAILURE;
    }
    
    if((status = pthread_mutex_init(&interfaceLock, NULL)) != 0) {
        syslog(LOG_ERR, "Failed to create interface mutex: %i", status);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

static int reconnectSocket(void) {
    
    unsigned int waitPeriod[3] = {5,15,60}; // seconds
    int count;
    
    close(interfaceFD);
    interfaceFD = -1;
    
    if ((interfaceFD = socket(addresInfo->ai_family, addresInfo->ai_socktype, 
        addresInfo->ai_protocol)) == -1) {
        syslog(LOG_ERR, "Failed to create new socket descriptor: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    
    for(count = 0; count < 3; count += 1) {
        if (connect(interfaceFD, addresInfo->ai_addr, addresInfo->ai_addrlen) == -1) {
            syslog(LOG_WARNING, "Failed to reconnect, initiating sleep [%i]: %s", waitPeriod[count], strerror(errno));
            sleep(waitPeriod[count]);
        }
        else {
            syslog(LOG_WARNING, "Reconnected: %s", strerror(errno));
            return EXIT_SUCCESS;
        }
    }
    
    return EXIT_FAILURE;
}

static int sendTCP(const void *message, size_t messageLength) {
    int bytesSend = 0;
    
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&interfaceLock);
    
    while(0 < messageLength) {
        if((bytesSend = send(interfaceFD, message, messageLength, MSG_NOSIGNAL)) == -1) {
            syslog(LOG_WARNING, "Send failed, reconnect attempted: %s", strerror(errno));
            if(reconnectSocket() == EXIT_FAILURE) {
            
                pthread_mutex_unlock(&interfaceLock);
                pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
                
                syslog(LOG_WARNING, "Reconnect failed");
                return EXIT_FAILURE;
            }
            continue;
        }
        message = message+bytesSend;
        messageLength = messageLength-bytesSend;
    }
    
    pthread_mutex_unlock(&interfaceLock);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	
    return EXIT_SUCCESS;
}

static void *listenTCP(void *arg) {
    sigset_t *thread_sigmask = (sigset_t *)arg;
    if(pthread_sigmask(SIG_BLOCK, thread_sigmask, NULL) < 0) {
        syslog(LOG_WARNING, "Failed to ignore signals in listen: %i", errno);
        pthread_kill(mainThread, SIGTERM);
        pause();
    }
    
    int bytesReceived;
    unsigned char interfaceRxBuffer[SERIAL_READ_BUFFER];
    unsigned char *interfaceRxPtr;
    int leftovers = 0;

    while(1) {
        interfaceRxPtr = interfaceRxBuffer;
        if(leftovers >= (SERIAL_READ_BUFFER/2))
			leftovers = 0; // crossed threshold, bogus input? reset leftovers
        interfaceRxPtr += leftovers;
			
        /* !!!! pthread cancellation point !!!! */
        if ((bytesReceived = recv(interfaceFD, interfaceRxPtr, SERIAL_READ_BUFFER-1-leftovers, 0)) <= 0) {
            syslog(LOG_ERR, "Failed to receive message over tcp/ip: %s", strerror(errno));
            
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
            if(pthread_mutex_trylock(&interfaceLock) == 0) {
                if(reconnectSocket() == EXIT_FAILURE) {
                    syslog(LOG_WARNING, "Reconnect failed");
                    pthread_kill(mainThread, SIGTERM);
                    
                    pthread_mutex_unlock(&interfaceLock);
                    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
                    
                    pause();
                }
                pthread_mutex_unlock(&interfaceLock);
                pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
            }
            else {
                pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
                sleep(15); // allow for other process to reestablish connection
                continue;
            }
        }
        interfaceRxBuffer[bytesReceived+leftovers] = '\0';
        
        leftovers = common_data.process->strip_raw_input(interfaceRxBuffer, bytesReceived+leftovers);
        syslog(LOG_DEBUG, "Leftovers in buffer (bytes read, #leftovers): %s (%i, %i)", interfaceRxBuffer, bytesReceived, leftovers);
        
    } /* end infinite while loop */
    
	// create option for allowing unsolicited incoming commands? (HEOS)
	// what if more than one simultaneous connection?
	
} /* listen() */

static int deinit(void) {
	freeaddrinfo(addrinfoResults); // free the linked-list
	
    /* Closing port */
    if(interfaceFD > 0) {
        close(interfaceFD);
	}
	
    pthread_mutex_destroy(&interfaceLock);
    
    return EXIT_SUCCESS;
} /* end deinit */
