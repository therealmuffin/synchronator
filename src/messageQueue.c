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
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
/* Message queue */
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "synchronator.h"
#include "common.h"


/* Create the file required for the message queue with 722 permissions */
static FILE * msq_file;
static int msq_id = -1;

int initMsQ(void) {

    key_t msq_key;

    if((msq_file = fopen(FTOK_PATH,"w")) != NULL) {
        fclose(msq_file);
        if(-1 == chmod(FTOK_PATH, S_IWRITE | S_IREAD | S_IWGRP | S_IWOTH)) {
            syslog(LOG_ERR, "Failed to set permissions message queue location: %s", strerror(errno));
            return EXIT_FAILURE;
        }
    }
    else {
        syslog(LOG_ERR, "Failed to create message queue location: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    if((msq_key = ftok(FTOK_PATH, FTOK_ID)) == -1) {
        syslog(LOG_ERR, "Failed to generate token: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    if((msq_id = msgget(msq_key, 0666 | IPC_CREAT)) == -1) {
        syslog(LOG_ERR, "Failed to create message queue: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    
	return EXIT_SUCCESS;
}

int watchMsQ(void) {

    typedef struct {
        long mtype;
        char mtext[MSQ_SIZE];
    } msq_buffer_t;
    
    msq_buffer_t msq_buffer;
    
    char input_command[MSQ_SIZE];
    char *input_value = NULL;
    long input_volume;
    
    while(1) {
        /* This does not always need to be fatal. If message is e.g. too long it will also
        /* return '-1'. Let's see how it works out....
        /* 
        /* !!!! pthread cancellation point !!!! */
        if(msgrcv(msq_id, &msq_buffer, sizeof(msq_buffer.mtext), 0, 0) == -1) {
            syslog(LOG_ERR, "Failed to receive message: %s", strerror(errno));
            raise(SIGTERM);
        }
        if((input_value = strstr(msq_buffer.mtext, "=")) == NULL) {
            syslog(LOG_WARNING, "Message has wrong format: %s\n", msq_buffer.mtext);
            continue;
        }
        strncpy(input_command, msq_buffer.mtext, input_value - msq_buffer.mtext);
        input_command[input_value - msq_buffer.mtext] = '\0';
        input_value += 1;
            
        syslog(LOG_INFO, "Message received: \"%s\"", msq_buffer.mtext);
        
        if(strcmp(input_command, "volume") == 0) {
            errno = 0;
            input_volume = strtol(input_value, (char **) NULL, 10);
            if(0 > input_volume || input_volume > 100 || errno == ERANGE) {
                syslog(LOG_WARNING, "Volume level is out of range: %s [0-100]\n", input_value);
                continue;
            }
            
            common_data.volume->convertMixer2Internal(&input_volume);
            if(common_data.process->compileVolumeCommand(&input_volume) == EXIT_FAILURE)
                raise(SIGTERM);
        }
        else            
            if(common_data.process->compileDeviceCommand(input_command, input_value) == EXIT_FAILURE)
                raise(SIGTERM);
    } /* end infinite msq loop */
    
	return EXIT_FAILURE;
} /* end listenMsq */

void deinitMsQ(void) {

    /* Close message queue and remove its location */
    if(msq_id >= 0) {
        if(msgctl(msq_id, IPC_RMID, NULL) == -1)
            syslog(LOG_WARNING, "Unable to remove message queue: %s", strerror(errno));
        if(remove(FTOK_PATH) == -1)
            syslog(LOG_WARNING, "Unable to delete message queue location: %s", strerror(errno));
    }
}