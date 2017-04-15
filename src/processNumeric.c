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
#include <fcntl.h> // writing status files
#include <unistd.h>
#include <sys/types.h> 
#include <stdint.h> // declares uint8_t a.o.
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <syslog.h>
#include <libconfig.h>

#include "synchronator.h"
#include "common.h"
#include "processData.h"
#include "verifyConfig.h"
#include "mimicAbsVol.h"


typedef struct {
    /* Points to buffer that is send through serial interface + reference to sections */
    uint8_t *serial_command[2], *event_header[2], *event[2], *command_tail[2];
// length of serial_command == command_tail - serial_command + 1?
    int header_length[2], volume_header[2], serial_command_length[2], tail_length[2];
    int alwaysMatchTail;
    
    uint8_t *statusQuery;
    
    /* non-discrete volume control */
    int volumeMutationNegative, volumeMutationPositive;
    uint8_t *volumeArrayNegative, *volumeArrayPositive;
} dechex_data_t;

static dechex_data_t dechex_data;
#ifdef TIME_DEFINED_TIMEOUT
    static struct timespec timestampCurrent;
#endif // #ifdef TIME_DEFINED_TIMEOUT

static void help(void);
static int init(void);
static void deinit(void);
static int sendVolumeCommand(long *volumeInternal);
static int replyVolumeCommand(long *volumeInternal);
static int compileVolumeCommand(long *volumeInternal);
static int sendDeviceCommand(char *category, char *action);
static int replyDeviceCommand(char *category, char *action);
static int compileDeviceCommand(char *header, char *action);
static int processCommand(void *category_lookup, void *action_lookup);
static int strip_raw_input(unsigned char *device_status_message, ssize_t bytes_read);

process_method_t processNumeric = {
    .name = "numeric",
    .help = &help,
    .init = &init,
    .deinit = &deinit,
    .compileVolumeCommand = &sendVolumeCommand,
    .compileDeviceCommand = &sendDeviceCommand,
    .processCommand = &processCommand,
    .strip_raw_input = &strip_raw_input
};

static void help(void) {
    printf("numerical\n"
          );
}

static int init(void) {
    dechex_data.alwaysMatchTail = 0;
    
    config_setting_t *conf_setting = NULL;
    int count;
    int main_count;
    int int_setting = -1;
    char config_query[CONFIG_QUERY_SIZE];

    dechex_data.serial_command[0] = NULL;
    dechex_data.serial_command[1] = NULL;
    dechex_data.volumeArrayNegative = NULL;
    dechex_data.volumeArrayPositive = NULL;
    dechex_data.statusQuery = NULL;
    
// common_data.volume_min >= common_data.volume_max ||  -> Can occur, see Cambridge Audio ->  && |min < max|
    if((common_data.volume_min < 0 || common_data.volume_min > 255) ||
        (common_data.volume_max < 0 || common_data.volume_max > 255)) {
        syslog(LOG_ERR, "[ERROR] Minimum and/or maximum volume level invalid: [0 >= level <= 255]");
        return EXIT_FAILURE;
    }
    
    for(main_count = 0; main_count <= 1; main_count++) {
        snprintf(config_query, CONFIG_QUERY_SIZE, "header.[%i]", main_count*common_data.diff_commands);
        if((conf_setting = config_lookup(&config, config_query)) == NULL || config_setting_is_array(conf_setting) == CONFIG_FALSE) {
            syslog(LOG_INFO, "[NOTICE] Setting not present or not formatted as array, ignoring: %s", config_query);
            dechex_data.header_length[main_count] = 0;
        }
        else
            dechex_data.header_length[main_count] = config_setting_length(conf_setting);
            
        snprintf(config_query, CONFIG_QUERY_SIZE, "tail.[%i]", main_count*common_data.diff_commands);
        if((conf_setting = config_lookup(&config, config_query)) == NULL || config_setting_is_array(conf_setting) == CONFIG_FALSE) {
            syslog(LOG_INFO, "[NOTICE] Setting not present or not formatted as array, ignoring: %s", config_query);
            dechex_data.tail_length[main_count] = 0;
        }
        else
            dechex_data.tail_length[main_count] = config_setting_length(conf_setting);
        
        dechex_data.serial_command_length[main_count] = dechex_data.header_length[main_count]+2+dechex_data.tail_length[main_count];
        dechex_data.serial_command[main_count] = calloc(dechex_data.serial_command_length[main_count], sizeof(int)); // uint8_t seems to cause memory to corrupt in 32bit, not 64bit
        dechex_data.event_header[main_count] = dechex_data.serial_command[main_count]+dechex_data.header_length[main_count];
        dechex_data.event[main_count] = dechex_data.event_header[main_count]+1;
        dechex_data.command_tail[main_count] = dechex_data.event[main_count]+1;
        
        snprintf(config_query, CONFIG_QUERY_SIZE, "header.[%i]", main_count*common_data.diff_commands);
        for(count = 0; count < dechex_data.header_length[main_count]; count++) {
            if(validateConfigInt(&config, config_query, (int *)&dechex_data.serial_command[main_count][count], 
            count, 0, 255, -1) == EXIT_FAILURE)
                return EXIT_FAILURE;
        }
        if(validateConfigInt(&config, "volume.header", &dechex_data.volume_header[main_count], 
        main_count*common_data.diff_commands, 0, 255, -2) == EXIT_FAILURE) {
            dechex_data.serial_command_length[main_count]--;
            dechex_data.event[main_count] = dechex_data.event_header[main_count];
            if(dechex_data.tail_length[main_count] != 0)
                dechex_data.command_tail[main_count]--; 
        }
        snprintf(config_query, CONFIG_QUERY_SIZE, "tail.[%i]", main_count*common_data.diff_commands);
        for(count = 0; count < dechex_data.tail_length[main_count]; count++) {
            if(validateConfigInt(&config, config_query, (int *)&dechex_data.command_tail[main_count][count], 
            count, 0, 255, -1) == EXIT_FAILURE)
                return EXIT_FAILURE;
        }
    }

    if(!common_data.discrete_volume) {    
        if(validateConfigInt(&config, "volume.min", &dechex_data.volumeMutationNegative, -1, 0, 255, -1) == EXIT_FAILURE)
            return EXIT_FAILURE;
        dechex_data.volumeArrayNegative = calloc(dechex_data.serial_command_length[0], sizeof(uint8_t));
        *dechex_data.event_header[0] = dechex_data.volume_header[0];
        *dechex_data.event[0] = dechex_data.volumeMutationNegative;
        memcpy(dechex_data.volumeArrayNegative, dechex_data.serial_command[0], dechex_data.serial_command_length[0] * sizeof(uint8_t));
        
        if(validateConfigInt(&config, "volume.plus", &dechex_data.volumeMutationPositive, -1, 0, 255, -1) == EXIT_FAILURE)
            return EXIT_FAILURE;
        dechex_data.volumeArrayPositive = calloc(dechex_data.serial_command_length[0], sizeof(uint8_t));
        *dechex_data.event[0] = dechex_data.volumeMutationPositive;
        memcpy(dechex_data.volumeArrayPositive, dechex_data.serial_command[0], dechex_data.serial_command_length[0] * sizeof(uint8_t));
        
        if(common_data.volumeMutationRange) {
            if(validateConfigInt(&config, "volume.max", &common_data.volume_max, -1, 1, 
            common_data.volumeMutationRange, common_data.volumeMutationRange) == EXIT_FAILURE)
                return EXIT_FAILURE;
        
            if(mimicAbsVol.init((char *)dechex_data.volumeArrayNegative, (char *)dechex_data.volumeArrayPositive) == EXIT_FAILURE)
                return EXIT_FAILURE;
        }
    }
    
    if(common_data.send_query) {
        dechex_data.statusQuery = calloc(common_data.statusQueryLength, sizeof(uint8_t*));
        for(count = 0; count < common_data.statusQueryLength; count++) {
            if(validateConfigInt(&config, "query.trigger", (int *)&dechex_data.statusQuery[count], 
            count, 0, 255, -1) == EXIT_FAILURE)
                return EXIT_FAILURE;
        }
        common_data.statusQuery = (const char *)dechex_data.statusQuery;
    }
    
    if(common_data.mod->command) {
        if(common_data.mod->command->init(2, &dechex_data.alwaysMatchTail, &dechex_data.serial_command_length) == EXIT_FAILURE)
            return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
} /* end init() */

static int sendVolumeCommand(long *volumeInternal) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&lockProcess);

#ifdef TIME_DEFINED_TIMEOUT
    int timeout;
    clock_gettime(CLOCK_MONOTONIC , &timestampCurrent);
    if((timeout = timestampCurrent.tv_sec - common_data.timestampLastRX.tv_sec) <= DEFAULT_PROCESS_TIMEOUT_OUT) {
        syslog(LOG_DEBUG, "Outgoing volume level processing timeout (completed): %i/%i", timeout, DEFAULT_PROCESS_TIMEOUT_OUT);
#else
    if(common_data.volume_out_timeout > 0) {
        common_data.volume_out_timeout--;
        syslog(LOG_DEBUG, "Outgoing volume level processing timeout: %i ", common_data.volume_out_timeout);
#endif // #ifdef TIME_DEFINED_TIMEOUT
        pthread_mutex_unlock(&lockProcess);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        return EXIT_SUCCESS;
    }
    
    if(compileVolumeCommand(volumeInternal) == EXIT_FAILURE) {
        pthread_mutex_unlock(&lockProcess);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        return EXIT_SUCCESS;
    }

#ifdef TIME_DEFINED_TIMEOUT 
    clock_gettime(CLOCK_MONOTONIC, &common_data.timestampLastTX);
#else
    common_data.volume_in_timeout = DEFAULT_PROCESS_TIMEOUT_IN;
#endif // #ifdef TIME_DEFINED_TIMEOUT
    syslog(LOG_DEBUG, "Volume level mutation (int. initiated): ext. level: %.2f", 
        common_data.volume_level_status);
    
    if(common_data.interface->send(dechex_data.serial_command[0], dechex_data.serial_command_length[0]) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    pthread_mutex_unlock(&lockProcess);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    return EXIT_SUCCESS;
}

static int replyVolumeCommand(long *volumeInternal) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&lockProcess);
    
    if(compileVolumeCommand(volumeInternal) == EXIT_FAILURE) {
        pthread_mutex_unlock(&lockProcess);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        return EXIT_SUCCESS;
    }
    
    syslog(LOG_DEBUG, "Replied current external volume level: %.2f", common_data.volume_level_status);
    
    if(common_data.interface->reply(dechex_data.serial_command[0], dechex_data.serial_command_length[0]) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    pthread_mutex_unlock(&lockProcess);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    return EXIT_SUCCESS;
}

static int compileVolumeCommand(long *volumeInternal) {
    
    if(*volumeInternal < 0 || *volumeInternal > 100) {
        syslog(LOG_WARNING, "Value for command \"volume\" is not valid: %ld", *volumeInternal);
        
        return EXIT_FAILURE;
    }
    
    if(common_data.discrete_volume) {
        double volumeExternal;
        common_data.volume->convertInternal2External(volumeInternal, &volumeExternal);
        *dechex_data.event_header[0] = dechex_data.volume_header[0];
        *dechex_data.event[0] = (int)volumeExternal;
        
        common_data.volume_level_status = volumeExternal;
    }
    else if(common_data.volumeMutationRange) {
        double volumeExternal;
        common_data.volume->convertInternal2External(volumeInternal, &volumeExternal);
        mimicAbsVol.process(volumeExternal);
        common_data.volume_level_status = volumeExternal;
        return EXIT_FAILURE; // to prevent an attempt to access serial_command in calling function
    }
    else {
        if(common_data.volume_level_status == *volumeInternal) // to catch a reset of the volume level
            return EXIT_FAILURE;
        
        *dechex_data.event_header[0] = dechex_data.volume_header[0];
        if(*volumeInternal > common_data.volume_level_status)
            *dechex_data.event[0] = dechex_data.volumeMutationPositive;
        else
            *dechex_data.event[0] = dechex_data.volumeMutationNegative;
        
        if(*volumeInternal < 25 || *volumeInternal > 75) {
            if((setMixer((common_data.alsa_volume_range/2)+common_data.alsa_volume_min)) == EXIT_FAILURE)
                return EXIT_FAILURE;
            
            syslog(LOG_DEBUG, "Mixer volume level: %ld", *volumeInternal);
            *volumeInternal = 50;
            //common_data.volume_out_timeout = 1; // really necessary? 
        }
        
        common_data.volume_level_status = *volumeInternal;
    }
    
    if(common_data.mod->command)
        common_data.mod->command->produce((void *)dechex_data.serial_command[0]);
    
    return EXIT_SUCCESS;
}

static int sendDeviceCommand(char *category, char *action) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&lockConfig);
    
    if(compileDeviceCommand(category, action) == EXIT_FAILURE) {
        pthread_mutex_unlock(&lockConfig);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        return EXIT_SUCCESS;
    }
    
    if(common_data.interface->send(dechex_data.serial_command[0], dechex_data.serial_command_length[0]) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    pthread_mutex_unlock(&lockConfig);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    return EXIT_SUCCESS;
}

static int replyDeviceCommand(char *category, char *action) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&lockConfig);
    
    if(compileDeviceCommand(category, action) == EXIT_FAILURE) {
        pthread_mutex_unlock(&lockConfig);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        return EXIT_SUCCESS;
    }
    
    if(common_data.interface->reply(dechex_data.serial_command[0], dechex_data.serial_command_length[0]) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    pthread_mutex_unlock(&lockConfig);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    return EXIT_SUCCESS;
}

static int compileDeviceCommand(char *category, char *action) {
    char config_query[CONFIG_QUERY_SIZE];
    int command;
    
    snprintf(config_query, CONFIG_QUERY_SIZE, "%s.%s.[0]", category, (char *)action);
    
    if(!config_lookup_int(&config, config_query, &command)) {
        
        syslog(LOG_WARNING, "Could not identify command: %s", (char *)action);
        return EXIT_FAILURE;
    }
    if(command < 0 || command > 255) {
        
        syslog(LOG_WARNING, "Value for command \"%s\" is not valid: %i", (char *)action, command);
        return EXIT_FAILURE;
    }
    *dechex_data.event[0] = command;
    
    snprintf(config_query, CONFIG_QUERY_SIZE, "%s.header.[0]", category);
    if(!config_lookup_int(&config, config_query, &command)) {
        
        syslog(LOG_WARNING, "Could not find header for message: %s", category);
        return EXIT_FAILURE;
    }
    
    if(command < 0 || command > 255) {
        syslog(LOG_WARNING, "Value for header \"%s\" is not valid: %i", category, command);
        return EXIT_FAILURE;
    }
    *dechex_data.event_header[0] = command;
    
    if(common_data.mod->command)
        common_data.mod->command->produce(dechex_data.serial_command[0]);
    
    return EXIT_SUCCESS;
} /* end serial_send_dechex */

static int processCommand(void *category_lookup, void *action_lookup) {
    int count = 0;
    int entry_count = 0;
    int total_root_entries = 0;
    int total_child_entries = 0;
    
    config_setting_t *config_root = NULL;
    config_setting_t *config_child = NULL;
    config_setting_t *config_entry = NULL;
    const char *current_header = NULL;
    const char *current_event = NULL;
    char *char_setting = NULL;
    int int_setting = -1;

    int status_file;
    char status_file_path[MSQ_SIZE];
    
    config_root = config_root_setting(&config);
    total_root_entries = config_setting_length(config_root);

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&lockConfig);
    
    for(count = 0; count < total_root_entries; count++) {
        config_child = config_setting_get_elem(config_root, count);
        if(!config_setting_is_group(config_child) ||
        !config_setting_lookup_bool(config_child, "register", &int_setting) || int_setting == 0 ||
        !(config_entry = config_setting_get_member(config_child, "header")) || 
        ((int_setting = config_setting_get_int_elem(config_entry, common_data.diff_commands)) == 0 && 
        *(uint8_t *)category_lookup != 0) || int_setting != *(uint8_t *)category_lookup)
            continue;
        
        current_header = config_setting_name(config_child);
        total_child_entries = config_setting_length(config_child);
        
        for(entry_count = 0; entry_count < total_child_entries; entry_count++) {
            config_entry = config_setting_get_elem(config_child, entry_count);
            if(((int_setting = config_setting_get_int_elem(config_entry, common_data.diff_commands)) == 0 && 
            *(uint8_t *)category_lookup != 0) || int_setting != *(uint8_t *)action_lookup)
                continue;
            
            snprintf(status_file_path, MAX_PATH_LENGTH, "%s/%s.%s", TEMPLOCATION, PROGRAM_NAME, current_header);
            if((status_file = open(status_file_path, O_RDWR|O_CREAT|O_CLOEXEC, LOCKMODE)) < 0) {
            
                pthread_mutex_unlock(&lockConfig);
                pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
                
                return EXIT_FAILURE;
            }
    
            current_event = config_setting_name(config_entry);
            ftruncate(status_file, 0);
            write(status_file, current_event, strlen(current_event));
            close(status_file);
            
            statusInfo.update(current_header, current_event);
            syslog(LOG_DEBUG, "Status updated event (header): %s (%s)", current_event, current_header);
            break;
        }
    }
    
    pthread_mutex_unlock(&lockConfig);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    return EXIT_SUCCESS;
} /* serial_process_dechex */

static int strip_raw_input(unsigned char *device_status_message, ssize_t bytes_read) {
    int status = 0;
    int count = 0;
    int completed = 0;
    int messageIndexDfl = dechex_data.event_header[1] == dechex_data.event[1] ? 
        !dechex_data.header_length[1]*2 : !dechex_data.header_length[1];
    int alwaysVolume = dechex_data.event_header[1] == dechex_data.event[1] ? 1 : 0;
    static int header_count = 0;
    static int tail_count = 0;
    static int message_index = 0;
    double volume_level = -1;
    
    if(!message_index)
        message_index = messageIndexDfl;
    
    for(count = 0; count < bytes_read; count++) {
        // if only volume, immediately send to volume process.
        
        if((int)device_status_message[count] == dechex_data.serial_command[1][header_count] && 
        dechex_data.header_length[1] != 0) {
            header_count++;
            if(header_count == dechex_data.header_length[1]) {
                header_count = 0;
                tail_count = 0; // reset tail
                message_index = alwaysVolume ? 2 : 1;
                continue;
            }
        }
        else {
            header_count = 0; // is set if header segment is not matched or if no header is set, e.g. if serial command length is set to 1
        }
        switch(message_index) {
            case 1:
                *dechex_data.event_header[1] = (uint8_t)device_status_message[count];
                message_index = 2;
                break;
            case 2:
                *dechex_data.event[1] = (uint8_t)device_status_message[count];
                if(dechex_data.tail_length[1] == 0)
                    completed = 1;
                message_index = 3;
                break;
            case 3:
                if((int)device_status_message[count] == dechex_data.command_tail[1][tail_count] || dechex_data.alwaysMatchTail) {
                    if(common_data.mod->command)
                        *(dechex_data.command_tail[1]+tail_count) = (uint8_t)device_status_message[count];
                    tail_count++;
                    if(tail_count == dechex_data.tail_length[1]) {
                        tail_count = 0;
                        completed = 1;
                    }
                }
                else {
                    message_index = messageIndexDfl;
                    tail_count = 0;
                    continue; // doesn't this invalidate the tail? FIXED! see -1
                }
                    
                break;
            default:
                continue;
        } /* end switch */
        
        if(completed) {
            message_index = messageIndexDfl;
            completed = 0;
            
            if(common_data.mod->command &&
            common_data.mod->command->process(dechex_data.serial_command[1], dechex_data.serial_command_length[1]) == EXIT_FAILURE)
                continue;
            
            if(*dechex_data.event_header[1] == dechex_data.volume_header[1] || alwaysVolume) {
                volume_level = (double)*dechex_data.event[1];
                status = common_data.volume->process(&volume_level);
            }
            else
                status = processCommand(dechex_data.event_header[1], dechex_data.event[1]);
            
            if(status == EXIT_FAILURE) {
                pthread_kill(mainThread, SIGTERM);
                pause();
            }
        }
    }
    return EXIT_SUCCESS; // due to the static vars it will remember data between runs...
} /* serial_strip_dechex */

static void deinit(void) {
    if(!common_data.discrete_volume && common_data.volumeMutationRange)
        mimicAbsVol.deinit();
    if(dechex_data.serial_command[0] != NULL)
        free(dechex_data.serial_command[0]);
    if(dechex_data.serial_command[1] != NULL)
        free(dechex_data.serial_command[1]);
    if(dechex_data.statusQuery != NULL)
        free(dechex_data.statusQuery);
    if(dechex_data.volumeArrayPositive != NULL)
        free(dechex_data.volumeArrayPositive);
    if(dechex_data.volumeArrayNegative != NULL)
        free(dechex_data.volumeArrayNegative);
    if(common_data.mod->command != NULL)
        common_data.mod->command->deinit();
}