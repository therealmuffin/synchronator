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
#include <sys/types.h>
#include <string.h>
#include <syslog.h>
#include <libconfig.h>
/* TCP/IP interface */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "synchronator.h"
#include "common.h"
#include "interfaces.h"

#define MAX_PORT_SIZE 6

static void help(void);
static int init(void);
static int initConnect(void);
static int initBind(void);
static int deinit(void);
static int sendDummy(const void *message, size_t messageLength);
static int sendTCP2InterfaceFD(const void *message, size_t messageLength);
static int replyTCP(const void *message, size_t messageLength);
static int sendAll(const void *message, size_t messageLength);
static int sendTCP(const void *message, size_t messageLength, int *address);
static void *listenTCP(void *arg);


interface_t interface_tcp = {
    .name = "tcp",
    .help = &help,
    .init = &init,
    .deinit = &deinit,
    .reply = replyTCP,
    .send = &sendTCP2InterfaceFD,
    .listen = &listenTCP
};

static pthread_mutex_t interfaceLock;
struct addrinfo *addrinfoResults, *addresInfo;
static int interfaceFD = -1, listenFD = -1, currentFD = -1;
static int persistent;
static int maxFD = 0;
static int maxConnections, listenTarget;
static int totalConnections = 0;
static int echoRx = 0;
static fd_set masterFDSet;
static int requestReconnect = 0;
static int sync_2way;
    
// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static void help(void) {
    printf("TCP (beta)\n"
          );
} /* end help */

static int init(void) {
    int temp, status;
    
    FD_ZERO(&masterFDSet);
    
    validateConfigBool(&config, "tcp_slave", &temp, 0); // set interface_tcp.send to dummy if true?
    if(!temp) {
        validateConfigBool(&config, "tcp_persistent", &persistent, 1);
        while((status = initConnect()) == 999 && persistent == 1) {
            sleep(5);
        }
        if(status != EXIT_SUCCESS)
            return EXIT_FAILURE;
    }
    else
        interface_tcp.send = &sendDummy;
    
    validateConfigBool(&config, "tcp_listen", &temp, 0);
    if(temp)
        if(initBind() == EXIT_FAILURE)
            return EXIT_FAILURE;
    
    if(listenFD == -1 && interfaceFD == -1) {
        syslog(LOG_ERR, "Can not be slave without listening: disable tcp_slave or enable tcp_listen");
        return EXIT_FAILURE;
    }
    
    validateConfigBool(&config, "tcp_echo", &echoRx, 0); // echo input
    validateConfigBool(&config, "tcp_send_all", &temp, 0);
    if(temp)
        interface_tcp.send = &sendAll;
    
    
    if(!common_data.sync_2way) { // to detect dropped connection safely in threads
        common_data.sync_2way = 1;
        sync_2way = 0;
    }
        
    
    if((status = pthread_mutex_init(&interfaceLock, NULL)) != 0) {
        syslog(LOG_ERR, "Failed to create interface mutex: %i", status);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

static int setKeepAlive(void) {
    int optval = 1; // BOOL TRUE
    socklen_t optlen = sizeof(optval);
    if(setsockopt(interfaceFD, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) == -1) {
        syslog(LOG_ERR, "Failed to set keepalive: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    optval = 120; // seconds idle before keepalive – https://linux.die.net/man/7/tcp
    if(setsockopt(interfaceFD, IPPROTO_TCP, TCP_KEEPIDLE, &optval, optlen) == -1) {
        syslog(LOG_ERR, "Failed to set keepidle: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    optval = 4; // retries before reconnect attempt
    if(setsockopt(interfaceFD, IPPROTO_TCP, TCP_KEEPCNT, &optval, optlen) == -1) {
        syslog(LOG_ERR, "Failed to set keepidle: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int initConnect(void) {
    const char *tcpIP;
    char tcpPortChar[MAX_PORT_SIZE];
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
        return 999;
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
        return 999;
    }
    
    if(setKeepAlive() == EXIT_FAILURE)
        return EXIT_FAILURE;

    FD_SET(interfaceFD, &masterFDSet);
    maxFD = interfaceFD;
    
    return EXIT_SUCCESS;
}

static int initBind(void) {
    struct addrinfo hints, *results, *cResult;
    char tcpPortChar[MAX_PORT_SIZE];
    int status, tcpPortInt;
    int yes=1; // need to obtain pointer to value

    validateConfigInt(&config, "tcp_max", &maxConnections, -1, 1, 100, 10);
        
//    validateConfigBool(&config, "tcp_dedicated", &listenTarget, 1)
    
    if(validateConfigInt(&config, "tcp_port", &tcpPortInt, -1, 1, 99999, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;
    snprintf (tcpPortChar, sizeof(tcpPortChar), "%i",tcpPortInt);
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((status = getaddrinfo(NULL, tcpPortChar, &hints, &results)) != 0) {
        syslog(LOG_ERR, "Failed to get IP info: %s", gai_strerror(status));
        return EXIT_FAILURE;
    }
    
    for(cResult = results; cResult != NULL; cResult = cResult->ai_next) {
        listenFD = socket(cResult->ai_family, cResult->ai_socktype, cResult->ai_protocol);
        if (listenFD < 0)
            continue;
        
        // prevent "address already in use" error message
        setsockopt(listenFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listenFD, cResult->ai_addr, cResult->ai_addrlen) < 0) {
            close(listenFD);
            continue;
        }
        break;
    }

    if (cResult == NULL) {
        syslog(LOG_ERR, "Failed to bind");
        return EXIT_FAILURE;
    }

    freeaddrinfo(results);

    if (listen(listenFD, 10) == -1) {
        syslog(LOG_ERR, "Failed to listen: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    
    FD_SET(listenFD, &masterFDSet);

    if (listenFD > maxFD)
        maxFD = listenFD;
    
    return EXIT_SUCCESS;
}

static int reconnectSocket(void) {
    int count = 0;
    
    close(interfaceFD);
    FD_CLR(interfaceFD, &masterFDSet);
    interfaceFD = -1;
    
    if ((interfaceFD = socket(addresInfo->ai_family, addresInfo->ai_socktype, 
        addresInfo->ai_protocol)) == -1) {
        syslog(LOG_ERR, "Failed to create new socket descriptor: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    
    while(count < 24 || persistent) {
        if (connect(interfaceFD, addresInfo->ai_addr, addresInfo->ai_addrlen) == -1) {
            syslog(LOG_WARNING, "Failed to reconnect, initiating sleep [5s]: %s", 
                strerror(errno));
            sleep(5);
        }
        else {
            syslog(LOG_WARNING, "Reconnected");
            FD_SET(interfaceFD, &masterFDSet); // replaced old fd for new
            if (interfaceFD > maxFD)
                maxFD = interfaceFD;
    
            if(setKeepAlive() == EXIT_FAILURE)
                return EXIT_FAILURE;
            
            return EXIT_SUCCESS;
        }
        count++;
    }
    return EXIT_FAILURE;
}

static int sendDummy(const void *message, size_t messageLength) {
    return EXIT_SUCCESS;
}

static int sendAll(const void *message, size_t messageLength) {
    int newFD, countFD;
    
    for(countFD = 0; countFD <= maxFD; countFD++) {
        if (FD_ISSET(countFD, &masterFDSet) && listenFD != countFD && 
        !(countFD == interfaceFD && requestReconnect))
            sendTCP(message, messageLength, &countFD);
    }

    return EXIT_SUCCESS;
}

static int sendTCP2InterfaceFD(const void *message, size_t messageLength) {
    if(requestReconnect == 1)
        return EXIT_SUCCESS;
    
    if(sendTCP(message, messageLength, &interfaceFD) == EXIT_FAILURE)
        return EXIT_FAILURE;
    else
        return EXIT_SUCCESS;
}

static int replyTCP(const void *message, size_t messageLength) {
    if(sendTCP(message, messageLength, &currentFD) == EXIT_FAILURE)
        return EXIT_FAILURE;
    else
        return EXIT_SUCCESS;
}

static int sendTCP(const void *message, size_t messageLength, int *address) {
    int bytesSend = 0;
    
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&interfaceLock);
    
    while(0 < messageLength) {
        if((bytesSend = send(*address, message, messageLength, MSG_NOSIGNAL)) == -1) {
            syslog(LOG_WARNING, "Send failed: %s", strerror(errno));
            if(*address == interfaceFD) {
                requestReconnect = 1;
            }
            break;
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
    
    int newFD, countFD;
    fd_set selectFDSet;
    FD_ZERO(&selectFDSet);
    char remoteIP[INET6_ADDRSTRLEN];
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;
    const char refuseMessage[] = "Too many connections, bye...\n";
    
    int bytesReceived;
    unsigned char interfaceRxBuffer[SERIAL_READ_BUFFER];
    unsigned char *interfaceRxPtr;
    int leftovers = 0;
    
    while(1) {
        selectFDSet = masterFDSet;
        if (select(maxFD+1, &selectFDSet, NULL, NULL, NULL) == -1) {
            syslog(LOG_ERR, "Failed to poll connections: %s", strerror(errno));
                    pthread_kill(mainThread, SIGTERM);
                    pause();
        }

        for(countFD = 0; countFD <= maxFD; countFD++) {
            if (FD_ISSET(countFD, &selectFDSet)) { // we got one!!
                if (countFD == listenFD) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newFD = accept(listenFD, (struct sockaddr *)&remoteaddr, &addrlen);
                    if(totalConnections == maxConnections) {
                        send(newFD, &refuseMessage, sizeof(refuseMessage), MSG_DONTWAIT);
                        close(newFD);
                        syslog(LOG_INFO, "Refused connection: one too many");
                        continue;
                    }
                    if (newFD == -1) {
                        syslog(LOG_WARNING, "Failed to accept connection");
                    } else {
                        FD_SET(newFD, &masterFDSet); // add to master set
                        totalConnections++;
                        if (newFD > maxFD) {    // keep track of the max
                            maxFD = newFD;
                        }
                        syslog(LOG_INFO, "Established connection from %s",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN));
                    }
                }
                else {
                    interfaceRxPtr = interfaceRxBuffer;
                    if(leftovers >= (SERIAL_READ_BUFFER/2))
                        leftovers = 0; // crossed threshold, bogus input? reset leftovers
                    interfaceRxPtr += leftovers;
                    // handle data from a client
                    if ((bytesReceived = recv(countFD, interfaceRxPtr, SERIAL_READ_BUFFER-1-leftovers, 0)) <= 0) {
                        if (countFD == interfaceFD) { // essential connection, attempt reconnect
                            requestReconnect = 1;
                            if(reconnectSocket() == EXIT_FAILURE) {
                                syslog(LOG_WARNING, "Reconnect failed");
                                pthread_kill(mainThread, SIGTERM);
                                pause();
                            }
                            requestReconnect = 0;
                        }
                        else { // obsolete connection
                            close(countFD);
                            FD_CLR(countFD, &masterFDSet);
                            totalConnections--;
                            syslog(LOG_INFO, "Closed connection from %s",
                                inet_ntop(remoteaddr.ss_family,
                                    get_in_addr((struct sockaddr*)&remoteaddr),
                                    remoteIP, INET6_ADDRSTRLEN));
                        }
                    }
                    else {
                        if(sync_2way)
                            continue;
                        if(echoRx)
                            sendTCP(interfaceRxPtr, bytesReceived, &countFD);
                        interfaceRxBuffer[bytesReceived+leftovers] = '\0';
                        
                        currentFD = countFD; // saving in case of reply needed
                        
                        leftovers = common_data.process->strip_raw_input(interfaceRxBuffer, bytesReceived+leftovers);
                        syslog(LOG_DEBUG, "Leftovers in buffer (bytes read, #leftovers): %s (%i, %i)", 
                            interfaceRxBuffer, bytesReceived, leftovers);
                    }
                } // end process data
            } // end process incoming connection
        } // end iterate through FDs
    } // end while loop
} /* end listen */

static int deinit(void) {
    int countFD;
    
    freeaddrinfo(addrinfoResults); // free the linked-list
    
    for(countFD = 0; countFD <= maxFD; countFD++) {
        if (FD_ISSET(countFD, &masterFDSet))
            close(countFD);
    }
    
    pthread_mutex_destroy(&interfaceLock);
    
    return EXIT_SUCCESS;
} /* end deinit */
