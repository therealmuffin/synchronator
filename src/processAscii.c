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
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <syslog.h>
#include <libconfig.h>

#include "synchronator.h"
#include "common.h"
#include "processData.h"
#include "verifyConfig.h"
#include "smoothVolume.h"


typedef struct {
    const char *command_header[2], *command_tail[2], *volume_header[2], *volume_tail[2], *event_delimiter[2];
    int volume_precision, volume_length, header_length[2], tail_length[2], event_delimiter_length[2];
    /* non-discrete volume control */
    char *volumeMutationNegative, *volumeMutationPositive;
    
    int allowRequests;
    const char *requestIndicator;
} ascii_data_t;

static ascii_data_t ascii_data;
#ifdef TIME_DEFINED_TIMEOUT
    static struct timespec timestampCurrent;
#endif // #ifdef TIME_DEFINED_TIMEOUT


static void help(void);
static int init(void);
static void deinit(void);
static int sendVolumeCommand(long *volumeInternal);
static int sendSmoothVolumeCommand(double *volumeExternal);
static int replyVolumeCommand(long *volumeInternal);
static void compileDescreteVolumeCommand(double *volumeExternal, char serial_command[200]);
static int setVolumeCommand(long *volumeInternal, char serial_command[200]);
static int sendDeviceCommand(char *category, char *action);
static int replyDeviceCommand(char *category, char *action);
static int compileDeviceCommand(char *header, char *action, char serial_command[200]);
static int processCommand(void *event_header, void *event);
static int strip_raw_input(unsigned char *device_status_message, ssize_t bytes_read);


process_method_t processAscii = {
    .name = "ascii",
    .help = &help,
    .init = &init,
    .deinit = &deinit,
    .compileVolumeCommand = &sendVolumeCommand,
    .compileDeviceCommand = &sendDeviceCommand,
    .processCommand = &processCommand,
    .strip_raw_input = &strip_raw_input
};

static void help(void) {
    printf("ascii\n"
          );
}

static int init(void) {
    int count;
    int main_count;
    int int_setting = -1;
    char serial_command[200];
    const char *char_setting = NULL;
    ascii_data.volumeMutationNegative = NULL;
    ascii_data.volumeMutationPositive = NULL;
    
    for(main_count = 0; main_count <= common_data.diff_commands; main_count++)
    {
        if(validateConfigString(&config, "header", &ascii_data.command_header[main_count], main_count) == EXIT_FAILURE)
            return EXIT_FAILURE;
        ascii_data.header_length[main_count] = strlen(ascii_data.command_header[main_count]);
        if(validateConfigString(&config, "tail", &ascii_data.command_tail[main_count], main_count) == EXIT_FAILURE)
            return EXIT_FAILURE;
            
        if(common_data.sync_2way && (ascii_data.tail_length[main_count] = strlen(ascii_data.command_tail[main_count])) < 1) {
            syslog(LOG_ERR, "[Error] Setting 'tail' can not be empty");
            return EXIT_FAILURE;
        }
        if(validateConfigString(&config, "event_delimiter", &ascii_data.event_delimiter[main_count], main_count) == EXIT_FAILURE)
            return EXIT_FAILURE;
        ascii_data.event_delimiter_length[main_count] = strlen(ascii_data.event_delimiter[main_count]);
        if(validateConfigString(&config, "volume.header", &ascii_data.volume_header[main_count], main_count) == EXIT_FAILURE)
            return EXIT_FAILURE;
        if(validateConfigString(&config, "volume.tail", &ascii_data.volume_tail[main_count], main_count) == EXIT_FAILURE)
            return EXIT_FAILURE;
    }
    if(common_data.discrete_volume) {
        if(validateConfigInt(&config, "volume.precision", &ascii_data.volume_precision, -1, 0, 10, 0) == EXIT_FAILURE)
            return EXIT_FAILURE;
        if(validateConfigInt(&config, "volume.length", &ascii_data.volume_length, -1, 0, 10, 0) == EXIT_FAILURE)
            return EXIT_FAILURE;
        if(ascii_data.volume_precision) ascii_data.volume_length += ascii_data.volume_precision+1;
    }
    if(!common_data.discrete_volume) {
        if(validateConfigString(&config, "volume.min", &char_setting, -1) == EXIT_FAILURE)
            return EXIT_FAILURE;
        snprintf(serial_command, 200, "%s%s%s%s%s%s", 
            ascii_data.command_header[0], ascii_data.volume_header[0], 
            ascii_data.event_delimiter[0], char_setting, 
            ascii_data.volume_tail[0], ascii_data.command_tail[0]);
        ascii_data.volumeMutationNegative = calloc(strlen(serial_command)+1, sizeof(char));
        strcpy(ascii_data.volumeMutationNegative, serial_command);
            
        if(validateConfigString(&config, "volume.plus", &char_setting, -1) == EXIT_FAILURE)
            return EXIT_FAILURE;
        snprintf(serial_command, 200, "%s%s%s%s%s%s", 
            ascii_data.command_header[0], ascii_data.volume_header[0], 
            ascii_data.event_delimiter[0], char_setting, 
            ascii_data.volume_tail[0], ascii_data.command_tail[0]);
        ascii_data.volumeMutationPositive = calloc(strlen(serial_command)+1, sizeof(char));
        strcpy(ascii_data.volumeMutationPositive, serial_command);
    }
    
    if(common_data.volume_timeout) {
        if(smoothVolume.init(&sendSmoothVolumeCommand, (char *)ascii_data.volumeMutationNegative, (char *)ascii_data.volumeMutationPositive) == EXIT_FAILURE)
            return EXIT_FAILURE;
    }
    
    if(common_data.send_query && validateConfigString(&config, "query.trigger.[0]", &common_data.statusQuery, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;
        if(common_data.send_query) common_data.statusQueryLength = strlen(common_data.statusQuery);

    if(config_lookup_string(&config, "response.indicator", &ascii_data.requestIndicator)) {
        ascii_data.allowRequests = 1;
        syslog(LOG_INFO, "[OK] response.indicator: %s", ascii_data.requestIndicator);
    } 
    else
        ascii_data.allowRequests = 0;
    
    if(common_data.mod->command) {
        if(common_data.mod->command->init(0) == EXIT_FAILURE)
            return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
} /* end init() */

static int sendVolumeCommand(long *volumeInternal) {
    char serial_command[200];
    
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&lockProcess);
    
#ifdef TIME_DEFINED_TIMEOUT
    int timeout;
    clock_gettime(CLOCK_MONOTONIC , &timestampCurrent); // somewhat inaccurate since beginning or end of second. Anyways, at least it's monotonic
    if((timeout = timestampCurrent.tv_sec - common_data.timestampLastRX.tv_sec) <= DEFAULT_PROCESS_TIMEOUT_OUT) {
        syslog(LOG_DEBUG, "Outgoing volume level processing timeout (completed): %i/%i", timeout, DEFAULT_PROCESS_TIMEOUT_OUT);
#else
    if(common_data.volume_out_timeout > 0) {
        common_data.volume_out_timeout--;
        syslog(LOG_DEBUG, "Outgoing volume level processing timeout: %i", common_data.volume_out_timeout);
#endif // #ifdef TIME_DEFINED_TIMEOUT
    
        pthread_mutex_unlock(&lockProcess);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        return EXIT_SUCCESS;
    }
    
    if(setVolumeCommand(volumeInternal, serial_command) == EXIT_FAILURE) {
        pthread_mutex_unlock(&lockProcess);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        return EXIT_SUCCESS;
    }

#ifdef TIME_DEFINED_TIMEOUT 
    clock_gettime(CLOCK_MONOTONIC , &common_data.timestampLastTX);
#else
    common_data.volume_in_timeout = DEFAULT_PROCESS_TIMEOUT_IN;
#endif // #ifdef TIME_DEFINED_TIMEOUT

    syslog(LOG_DEBUG, "Volume level mutation (int. initiated): ext. level: %.2f", 
        common_data.volume_level_status);
    
    pthread_mutex_unlock(&lockProcess);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    if(common_data.interface->send(serial_command, strlen(serial_command)) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    return EXIT_SUCCESS;
}

static int sendSmoothVolumeCommand(double *volumeExternal) {
    char serial_command[200];
    
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&lockProcess);
    
    compileDescreteVolumeCommand(volumeExternal, serial_command);

#ifdef TIME_DEFINED_TIMEOUT 
    clock_gettime(CLOCK_MONOTONIC , &common_data.timestampLastTX);
#else
    common_data.volume_in_timeout = DEFAULT_PROCESS_TIMEOUT_IN;
#endif // #ifdef TIME_DEFINED_TIMEOUT

    syslog(LOG_DEBUG, "Volume level mutation (int. initiated): ext. level: %.2f", 
        common_data.volume_level_status);
    
    pthread_mutex_unlock(&lockProcess);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    if(common_data.interface->send(serial_command, strlen(serial_command)) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    return EXIT_SUCCESS;
}

static int replyVolumeCommand(long *volumeInternal) {
    char serial_command[200];
    
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&lockProcess);
    
    if(setVolumeCommand(volumeInternal, serial_command) == EXIT_FAILURE) {
        pthread_mutex_unlock(&lockProcess);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        return EXIT_SUCCESS;
    }
    
    syslog(LOG_DEBUG, "Replied current external volume level: %.2f", common_data.volume_level_status);
    
    pthread_mutex_unlock(&lockProcess);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    if(common_data.interface->reply(serial_command, strlen(serial_command)) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    return EXIT_SUCCESS;
}

static void compileDescreteVolumeCommand(double *volumeExternal, char serial_command[200]) {
    snprintf(serial_command, 200, "%s%s%s%0*.*f%s%s", 
        ascii_data.command_header[0], ascii_data.volume_header[0], 
        ascii_data.event_delimiter[0], ascii_data.volume_length, 
        ascii_data.volume_precision, *volumeExternal, ascii_data.volume_tail[0], 
        ascii_data.command_tail[0]);
}

static int setVolumeCommand(long *volumeInternal, char serial_command[200]) {
    
    if(*volumeInternal < 0 || *volumeInternal > 100) {
        syslog(LOG_WARNING, "Value for command \"volume\" is not valid: %ld", *volumeInternal);
        
        return EXIT_FAILURE;
    }
    
    if(common_data.discrete_volume || common_data.volume_timeout) {
        double volumeExternal;
        common_data.volume->convertInternal2External(volumeInternal, &volumeExternal);
            
        common_data.volume_level_status = volumeExternal;
        
        if(common_data.volume_timeout) {
            smoothVolume.process(&volumeExternal);
            return EXIT_FAILURE;
        }
        else {
            compileDescreteVolumeCommand(&volumeExternal, serial_command);
        }
    }
    else {
        if(common_data.volume_level_status == *volumeInternal)
            return EXIT_FAILURE;
        
        if(*volumeInternal > common_data.volume_level_status)
            strcpy(serial_command, ascii_data.volumeMutationPositive);
        else
            strcpy(serial_command, ascii_data.volumeMutationNegative);
        
        if(*volumeInternal < 25 || *volumeInternal > 75) {
            int status = -1;
            if((setMixer((common_data.alsa_volume_range/2)+common_data.alsa_volume_min)) == EXIT_FAILURE)
                return EXIT_FAILURE;
            
            syslog(LOG_INFO, "Mixer volume level: %ld", *volumeInternal);
            *volumeInternal = 50;
            //common_data.volume_out_timeout = 1;
        }
        
        common_data.volume_level_status = *volumeInternal;
    }
    
    return EXIT_SUCCESS;
}

static int sendDeviceCommand(char *category, char *action) {
    char serial_command[200];
    
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&lockConfig);
    
    if(compileDeviceCommand(category, action, serial_command) == EXIT_FAILURE) {
        pthread_mutex_unlock(&lockConfig);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
        return EXIT_SUCCESS;
    }
    
    pthread_mutex_unlock(&lockConfig);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    if(common_data.interface->send(serial_command, strlen(serial_command)) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    return EXIT_SUCCESS;
}

static int replyDeviceCommand(char *category, char *action) {
    char serial_command[200];
    
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&lockConfig);
    
    if(compileDeviceCommand(category, action, serial_command) == EXIT_FAILURE) {
        pthread_mutex_unlock(&lockConfig);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
        return EXIT_SUCCESS;
    }
    
    pthread_mutex_unlock(&lockConfig);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    if(common_data.interface->reply(serial_command, strlen(serial_command)) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    return EXIT_SUCCESS;
}

static int compileDeviceCommand(char *category, char *action, char serial_command[200]) {
    char config_query[CONFIG_QUERY_SIZE];
    const char *command_category, *command_action;
        
    snprintf(config_query, CONFIG_QUERY_SIZE, "%s.%s.[0]", category, (char *)action);
    
    if(!config_lookup_string(&config, config_query, &command_action)) {
        syslog(LOG_WARNING, "Could not identify command: %s", (char *)action);
        return EXIT_FAILURE;
    }
    
    snprintf(config_query, CONFIG_QUERY_SIZE, "%s.header.[0]", category);
    if(!config_lookup_string(&config, config_query, &command_category)) {
        syslog(LOG_WARNING, "Could not find header for message: %s", category);
        return EXIT_FAILURE;
    }
    
    snprintf(serial_command, 200, "%s%s%s%s%s", 
        ascii_data.command_header[0], command_category, 
        ascii_data.event_delimiter[0], command_action, ascii_data.command_tail[0]);
    
    return EXIT_SUCCESS;
} /* end serial_send_ascii */

static int processCommand(void *event_header, void *event) {
    int count = 0;
    int entry_count = 0;
    int total_root_entries = 0;
    int total_child_entries = 0;
    
    config_setting_t *config_root = NULL;
    config_setting_t *config_child = NULL;
    config_setting_t *config_entry = NULL;
    const char *current_header = NULL;
    const char *current_event = NULL;
    int int_setting = 0;

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
        (current_header = config_setting_get_string_elem(config_entry, common_data.diff_commands)) == NULL || 
        strcmp(current_header, (char *)event_header) != 0)
            continue;
        
        current_header = config_setting_name(config_child);
        total_child_entries = config_setting_length(config_child);
        
        for(entry_count = 0; entry_count < total_child_entries; entry_count++) {
            config_entry = config_setting_get_elem(config_child, entry_count);
            if((current_event = config_setting_get_string_elem(config_entry, common_data.diff_commands)) == 0 || 
            strcmp(current_event, (char *)event) != 0)
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
            syslog(LOG_DEBUG, "Status update event (header): %s (%s)", current_event, current_header);
            
            pthread_mutex_unlock(&lockConfig);
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
            
            return EXIT_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&lockConfig);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    return EXIT_SUCCESS;
} /* serial_process_ascii */

static int processRequest(char *request) {
    int count = 0;
    int responseTotal = 0;
    
    config_setting_t *responseConfig = NULL;
    config_setting_t *responseCurrent = NULL;
    const char *responseValue = NULL;
    const char *requestName = NULL;
    const char *requestValue = NULL;
    long volume;
    
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&lockConfig);
    
    responseConfig = config_lookup(&config, "response");
    responseTotal = config_setting_length(responseConfig);
    
    for(count = 0; count < responseTotal; count++) {
        responseCurrent = config_setting_get_elem(responseConfig, count);
        if((responseValue = config_setting_get_string_elem(responseCurrent, 1)) != NULL &&
        strcmp(responseValue, request) == 0) {
            responseValue = config_setting_get_string_elem(responseCurrent, 2);
            if(config_setting_get_bool_elem(responseCurrent, 0) == 1) { // formulating default response
            
                pthread_mutex_unlock(&lockConfig);
                pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
                
                common_data.interface->reply(responseValue, strlen(responseValue));
            }
            else { // attempt to formulate custom response
                requestName = config_setting_name(responseCurrent);
                
                pthread_mutex_unlock(&lockConfig);
                pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
                
                if(strcmp(requestName, "volume") == 0) {
                    if(getMixer(&volume) == EXIT_FAILURE) {
                        return EXIT_FAILURE;
                    }
                    replyVolumeCommand(&volume);
                }
                else {
                    statusInfo.retrieve(requestName, &requestValue);
                    if(requestValue != NULL)
                        replyDeviceCommand((char *)requestName, (char *)requestValue);
                    else // custom response not possible, reverting to default value
                        replyDeviceCommand((char *)requestName, (char *)responseValue);
                }
            }
            syslog(LOG_DEBUG, "Successfully processed request: %s", request);
            return EXIT_SUCCESS; // command is matched, returning
        }
        else {
            pthread_mutex_unlock(&lockConfig);
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        }
    }
    syslog(LOG_DEBUG, "Could not identify request: %s", request);
    return EXIT_SUCCESS;
}

static int strip_raw_input(unsigned char *device_status_message, ssize_t bytes_read) {
    int status = 0;
    int count = 0;
    char serial_command[SERIAL_READ_BUFFER];
    char *message_cpy = (char *)device_status_message;
    char *header_ptr = (char *)device_status_message;
    char *event_header_ptr = NULL;
    char *event_delimiter_ptr = NULL;
    char *event_ptr = NULL;
    char *tail_ptr = NULL;
    double volume_level;
    
    while(1) {
        if(ascii_data.header_length[common_data.diff_commands] == 0) // header is not required
            header_ptr = message_cpy;
        else
            if((header_ptr = strstr(message_cpy, ascii_data.command_header[common_data.diff_commands])) == NULL)
                break;
        if((tail_ptr = strstr(header_ptr+1, ascii_data.command_tail[common_data.diff_commands])) == NULL) // tail must exist always, if tail = "" always return header_ptr+1
            break;
        
        message_cpy = tail_ptr+ascii_data.tail_length[common_data.diff_commands]; // either beginning new message or null character if end message
        *tail_ptr = '\0';
        
        strncpy(serial_command, header_ptr, tail_ptr+1-header_ptr);
        
        event_header_ptr = serial_command+ascii_data.header_length[common_data.diff_commands];
        if((event_delimiter_ptr = strstr(event_header_ptr, ascii_data.event_delimiter[common_data.diff_commands])) > event_header_ptr) {
            event_ptr = event_delimiter_ptr+ascii_data.event_delimiter_length[common_data.diff_commands];
            *event_delimiter_ptr = '\0';
        }
        else if(event_delimiter_ptr == event_header_ptr) { // if true assume needle is empty, assume no event header exists.. so volume only
            event_ptr = event_header_ptr;
            event_header_ptr = &serial_command[tail_ptr-header_ptr]; // is or will be NULL character
        }
        else if(event_delimiter_ptr == NULL) { // delimiter not matched, incoming command not valid
            if(ascii_data.allowRequests && strstr(event_header_ptr, ascii_data.requestIndicator) > event_header_ptr)
                processRequest(event_header_ptr);
                
            /* check if we're still in the buffer, otherwise break loop */
            if((long)message_cpy-((long)device_status_message+bytes_read) >= 0)
                break;
            continue;
        }
        else { // shouldn't have gotten here, skip to next message or break loop
            /* check if we're still in the buffer, otherwise break loop */
            if((long)message_cpy-((long)device_status_message+bytes_read) >= 0)
                break;
            
            continue;
        }
        
        syslog(LOG_DEBUG, "Detected incoming event (header): %s (%s)", event_ptr, event_header_ptr);
        
        if(common_data.mod->command &&
        common_data.mod->command->process(&serial_command, tail_ptr-header_ptr) == EXIT_FAILURE)
            continue;
            
        if(strcmp(event_header_ptr, ascii_data.volume_header[common_data.diff_commands]) == 0) { // if the delimiter is empty, this will always match
            errno = 0;
            volume_level = strtod(event_ptr, (char **)NULL); // segfault if not number? no, just with more than 1 byte send
            if(errno == 0)
                status = common_data.volume->process(&volume_level);
        }
        else
            status = processCommand(event_header_ptr, event_ptr);
        
        /* check if we're still in the buffer, otherwise break loop */
        if((long)message_cpy-((long)device_status_message+bytes_read) >= 0)
            break;
        
        if(status == EXIT_FAILURE) {
            pthread_kill(mainThread, SIGTERM);
            pause();
        }
    }
    // causes segfault: *ZPre2.conf "W 1 9 2" and then "S1", can't reproduce...? Now I can.... ssize can be negative!!!! ought to be fixed now...
    strncpy((char *)device_status_message, message_cpy, ((char *)device_status_message+bytes_read) - message_cpy+1);

    return(((char *)device_status_message+bytes_read) - message_cpy);
} /* strip_raw_input */

static void deinit(void) {
    if(common_data.mod->command != NULL)
        common_data.mod->command->deinit();
    if(common_data.volume_timeout)
        smoothVolume.deinit();
        
    if(ascii_data.volumeMutationNegative != NULL)
        free(ascii_data.volumeMutationNegative);
    if(ascii_data.volumeMutationPositive != NULL)
        free(ascii_data.volumeMutationPositive);
}