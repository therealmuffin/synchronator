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
#include <pthread.h>
#include <libconfig.h>
/* Serial interface */
#include <termios.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <unistd.h>

#include "verifyConfig.h"
#include "synchronator.h"
#include "common.h"
#include "interfaces.h"

static void help(void);
static int init(void);
static int deinit(void);
static int send(const void *message, size_t messageLength);
static void *listen(void *arg);

interface_t interface_serial = {
    .name = "serial",
    .help = &help,
    .init = &init,
    .deinit = &deinit,
    .reply = &send,
    .send = &send,
    .listen = &listen
};

static pthread_mutex_t interfaceLock;
static int interfaceFD = -1;
static struct termios termiosOriginalAttr;

static void help(void) {
    printf("Serial\n"
          );
} /* end help */

static int init(void) {
    /* Serial device settings */
    struct termios termiosNewAttr;
    const char *serialDevice;
    speed_t serialSpeed;
    tcflag_t serialBits;
    int serialParity, serialEven, serial2Stop, intSetting, status;

    if(validateConfigString(&config, "serial_port", &serialDevice, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    interfaceFD = open(serialDevice, O_RDWR| O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if(interfaceFD < 0) {
        syslog(LOG_ERR, "Failed to open serial device %s: %s (%i)", serialDevice, strerror(errno), errno);
        return EXIT_FAILURE;
    }
    
    /* see for available options:
     * http://www.easysw.com/~mike/serial/serial.html */
    
    tcgetattr(interfaceFD, &termiosOriginalAttr);
    tcgetattr(interfaceFD, &termiosNewAttr);
    
    validateConfigBool(&config, "serial_parity", &serialParity, 0);
    validateConfigBool(&config, "serial_even", &serialEven, 0);
    validateConfigBool(&config, "serial_2stop", &serial2Stop, 0);
    if(validateConfigInt(&config, "serial_baud", &intSetting, -1, 0, 0, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;
    else
        switch(intSetting) {
            case 1200:
                serialSpeed = B1200;
                break;
            case 1800:
                serialSpeed = B1800;
                break;
            case 2400:
                serialSpeed = B2400;
                break;
            case 4800:
                serialSpeed = B4800;
                break;
            case 9600:
                serialSpeed = B9600;
                break;
            case 19200:
                serialSpeed = B19200;
                break;
            case 38400:
                serialSpeed = B38400;
                break;
            case 57600:
                serialSpeed = B57600;
                break;
            case 115200:
                serialSpeed = B115200;
                break;
            default:
                syslog(LOG_ERR, "[Error] Entry is invalid [1200|1800|2400|4800|9600|19200|38400|57600|115200]: %i", intSetting);
                return EXIT_FAILURE;
        }
    if(validateConfigInt(&config, "serial_bits", &intSetting, -1, 0, 0, 8) == EXIT_FAILURE)
        return EXIT_FAILURE;
    else
        switch(intSetting) {
            case 5:
                serialBits = CS5;
                break;
            case 6:
                serialBits = CS6;
                break;
            case 7:
                serialBits = CS7;
                break;
            case 8:
                serialBits = CS8;
                break;
            default:
                syslog(LOG_ERR, "[Error] Entry is invalid [5|6|7|8]: %i", intSetting);
                return EXIT_FAILURE;
        }
    
    /* c_cflag 
     * setting all the variables according to the configuration file
     * and some default settings */
    termiosNewAttr.c_cflag |= (CLOCAL | CREAD);
    if(serialParity == 0)
        termiosNewAttr.c_cflag &= ~PARENB;
    else {
        termiosNewAttr.c_cflag |= PARENB;
        /* enable parity check and strip it from output */
        termiosNewAttr.c_iflag |= (INPCK | ISTRIP);
        if(serialEven == 0)
            termiosNewAttr.c_cflag |= PARODD;
        else
            termiosNewAttr.c_cflag &= ~PARODD;
    }

    if(serial2Stop == 0)
        termiosNewAttr.c_cflag &= ~CSTOPB;
    else
        termiosNewAttr.c_cflag |= CSTOPB;
    termiosNewAttr.c_cflag &= ~CSIZE;
    termiosNewAttr.c_cflag |= serialBits;
    
    /* c_lflag 
     * enable non-canonical, raw input */
    termiosNewAttr.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    
    /* Disabling hardware flow control */
    termiosNewAttr.c_cflag &= ~CRTSCTS;
    /* Disabling software flow control */
    termiosNewAttr.c_iflag &= ~(IXON | IXOFF | IXANY);
    /* Disabling any input processing */
    termiosNewAttr.c_iflag &= ~(ICRNL | IGNCR | INLCR);
    
    /* Disabling any postprocessing */
    termiosNewAttr.c_oflag &= ~OPOST;
    
    /* Minimum characters to read (non-canonical) */
    termiosNewAttr.c_cc[VMIN] = 1;
    /* Wait indefinitely for input (non-canonical) */
    termiosNewAttr.c_cc[VTIME] = 0;
    
    if((cfsetospeed(&termiosNewAttr, serialSpeed)) < 0) {
        syslog(LOG_ERR, "Failed to set serial output baud rate");
        return EXIT_FAILURE;
    }
    if((cfsetispeed(&termiosNewAttr, serialSpeed)) < 0) {
        syslog(LOG_ERR, "Failed to set serial input baud rate");
        return EXIT_FAILURE;
    }
    /* set attributes 'now' */
    if(tcsetattr(interfaceFD, TCSANOW, &termiosNewAttr) == -1) {
        syslog(LOG_ERR, "Failed to set attributes for serial device %s: %s (%i)", serialDevice, strerror(errno), errno);
        return EXIT_FAILURE;
    }
    if((status = pthread_mutex_init(&interfaceLock, NULL)) != 0) {
        syslog(LOG_ERR, "Failed to create interface mutex: %i", status);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
} /* end init */

static int send(const void *message, size_t messageLength) {
    
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&interfaceLock);
    
    if(write(interfaceFD, message, messageLength) == -1) {
    
        pthread_mutex_unlock(&interfaceLock);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        syslog(LOG_ERR, "Failed to write to serial port: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    
    pthread_mutex_unlock(&interfaceLock);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    return EXIT_SUCCESS;
} /* end send */

static void *listen(void *arg) {
    sigset_t *thread_sigmask = (sigset_t *)arg;
    if(pthread_sigmask(SIG_BLOCK, thread_sigmask, NULL) < 0) {
        syslog(LOG_ERR, "Failed to ignore signals in listen: %i", errno);
        pthread_kill(mainThread, SIGTERM);
        pause();
    }
    
    struct pollfd pollFD[1];
    pollFD[0].fd = interfaceFD;
    pollFD[0].events = POLLIN;
    ssize_t bytesRead;
    unsigned char interfaceRxBuffer[SERIAL_READ_BUFFER];
    unsigned char *interfaceRxPtr;
    int leftovers = 0;
    int status = 0;

    while(1) {
        interfaceRxPtr = interfaceRxBuffer;
        if(leftovers >= (SERIAL_READ_BUFFER/2))
            leftovers = 0; // crossed threshold, bogus input? reset leftovers
        interfaceRxPtr += leftovers;
            
        /* !!!! pthread cancellation point !!!! */
        status = poll(pollFD, 1, -1);
        if(status < 0) {
            syslog(LOG_ERR, "Failed to poll serial interface: %s", strerror(errno));
            pthread_kill(mainThread, SIGTERM);
            pause();
        }
        else if(status == 0)
            continue;
        
        if(pollFD[0].revents & POLLIN) {
            bytesRead = read(interfaceFD, interfaceRxPtr, SERIAL_READ_BUFFER-1-leftovers);
            if(bytesRead <= 0)
                continue;

            interfaceRxBuffer[bytesRead+leftovers] = '\0';
            
            leftovers = common_data.process->strip_raw_input(interfaceRxBuffer, bytesRead+leftovers);
            syslog(LOG_DEBUG, "Leftovers in buffer (bytes read, #leftovers): %s (%i, %i)", interfaceRxBuffer, bytesRead, leftovers);
        }
    } /* end infinite while loop */
} /* listen() */

static int deinit(void) {

    /* Restoring serial settings and closing port */
    if(interfaceFD > 0) {
        tcsetattr(interfaceFD, TCSANOW, &termiosOriginalAttr);
        close(interfaceFD);
    }
    pthread_mutex_destroy(&interfaceLock);
    
    return EXIT_SUCCESS;
} /* end deinit */
