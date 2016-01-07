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

//#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <syslog.h>
#include <pthread.h>
#include <signal.h>
#include <libconfig.h>
#include "verifyConfig.h"
#include "synchronator.h"
#include "common.h"
#include "processData.h"


typedef struct {
    const char *command_header[2], *command_tail[2], *volume_header[2], *event_delimiter[2];
    int volume_precision, volume_length, header_length[2], tail_length[2], event_delimiter_length[2];
    /* non-discrete volume control */
    const char *volumeMutationNegative, *volumeMutationPositive;
    
} ascii_data_t;

static ascii_data_t ascii_data;

static void help(void);
static int init(void);
static int deinit(void);
static int compileCommand(char *header, void *command);
static int processCommand(char *event_header, char *event);
static int strip_raw_input(unsigned char *device_status_message, ssize_t bytes_read);


process_method_t processAscii = {
    .name = "ascii",
    .help = &help,
    .init = &init,
    .deinit = &deinit,
    .compileCommand = &compileCommand,
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
    ascii_data.volume_precision = 0;
    
	for(main_count = 0; main_count <= common_data.diff_commands; main_count++)
	{
		if(validateConfigString(&config, "header", &ascii_data.command_header[main_count], main_count) == EXIT_FAILURE)
			return EXIT_FAILURE;
		ascii_data.header_length[main_count] = strlen(ascii_data.command_header[main_count]);
		if(validateConfigString(&config, "tail", &ascii_data.command_tail[main_count], main_count) == EXIT_FAILURE)
			return EXIT_FAILURE;
			
		if((ascii_data.tail_length[main_count] = strlen(ascii_data.command_tail[main_count])) < 1) {
			syslog(LOG_ERR, "[Error] Setting 'tail' can not be empty");
			return EXIT_FAILURE;
		}
		if(validateConfigString(&config, "event_delimiter", &ascii_data.event_delimiter[main_count], main_count) == EXIT_FAILURE)
			return EXIT_FAILURE;
		ascii_data.event_delimiter_length[main_count] = strlen(ascii_data.event_delimiter[main_count]);
		if(validateConfigString(&config, "volume.header", &ascii_data.volume_header[main_count], main_count) == EXIT_FAILURE)
			return EXIT_FAILURE;
	}
	if(common_data.discrete_volume && validateConfigInt(&config, "volume.precision", &ascii_data.volume_precision, -1, 0, 10, 0) == EXIT_FAILURE)
		return EXIT_FAILURE;
	if(common_data.discrete_volume && validateConfigInt(&config, "volume.length", &ascii_data.volume_length, -1, 0, 10, 0) == EXIT_FAILURE)
		return EXIT_FAILURE;
	if(common_data.discrete_volume && ascii_data.volume_precision) ascii_data.volume_length += ascii_data.volume_precision+1;
	if(!common_data.discrete_volume && validateConfigString(&config, "volume.min", &ascii_data.volumeMutationNegative, -1) == EXIT_FAILURE)
		return EXIT_FAILURE;
	if(!common_data.discrete_volume && validateConfigString(&config, "volume.plus", &ascii_data.volumeMutationPositive, -1) == EXIT_FAILURE)
		return EXIT_FAILURE;
	if(common_data.send_query && validateConfigString(&config, "query.trigger.[0]", &common_data.statusQuery, -1) == EXIT_FAILURE)
		return EXIT_FAILURE;
        if(common_data.send_query) common_data.statusQueryLength = strlen(common_data.statusQuery);
        
	
    return EXIT_SUCCESS;
} /* end init() */

static int compileCommand(char *category, void *action) {
    char serial_command[200];
    /* Volume commands come in at a much higher speed, therefore, it takes a few
     * shortcuts rather than having libconfig do the work each time */
    if(strcmp(category, "volume") == 0) {
    
    	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_mutex_lock(&lockProcess);
    
        if(common_data.volume_out_timeout > 0) {
            common_data.volume_out_timeout--;
            
            pthread_mutex_unlock(&lockProcess);
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
            
            syslog(LOG_DEBUG, "Outgoing volume level processing timeout: %i", common_data.volume_out_timeout);
            return EXIT_SUCCESS;
        }
     //   int* temp_int = (int*) action;
        double volume_level = *(int*) action;
        if(volume_level < 0 || volume_level > 100) {
            syslog(LOG_WARNING, "Value for command \"volume\" is not valid: %i", volume_level);
            
            pthread_mutex_unlock(&lockProcess);
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
            
            return EXIT_SUCCESS;
        }
        
        if(common_data.discrete_volume) {
//            float volume_multiplier = ((float)common_data.volume_max - (float)common_data.volume_min) / (100-0);
            volume_level = (volume_level * common_data.multiplierIntToDevice) + common_data.volume_min;
            snprintf(serial_command, 200, "%s%s%s%0*.*f%s", ascii_data.command_header[0], ascii_data.volume_header[0], ascii_data.event_delimiter[0], ascii_data.volume_length, ascii_data.volume_precision, volume_level, ascii_data.command_tail[0]);
        }
        else {
            if(common_data.volume_level_status == volume_level) {
            
                pthread_mutex_unlock(&lockProcess);
                pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
                
                return EXIT_SUCCESS;
            }
            const char* volumeMutation;
            if(volume_level > common_data.volume_level_status)
                volumeMutation = ascii_data.volumeMutationPositive;
            else
                volumeMutation = ascii_data.volumeMutationNegative;
            snprintf(serial_command, 200, "%s%s%s%s%s", ascii_data.command_header[0], ascii_data.volume_header[0], ascii_data.event_delimiter[0], volumeMutation, ascii_data.command_tail[0]);
            if(volume_level < 25 || volume_level > 75) {
                int status = -1;
                if((setMixer((common_data.alsa_volume_range/2)+common_data.alsa_volume_min)) != EXIT_SUCCESS)
                    syslog(LOG_WARNING, "Setting mixer failed");
                syslog(LOG_INFO, "Mixer volume level: %i", volume_level);
                volume_level = 50;
                common_data.volume_out_timeout = 1;
            }
        }
        
        common_data.volume_level_status = volume_level;
        common_data.volume_in_timeout = DEFAULT_PROCESS_TIMEOUT_IN;
        
		syslog(LOG_DEBUG, "Volume level mutation (int. initiated): ext. level: %.2f", common_data.volume_level_status);
        
        pthread_mutex_unlock(&lockProcess);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }
    else {
        char config_query[CONFIG_QUERY_SIZE];
        const char *command_category, *command_action;
        
        snprintf(config_query, CONFIG_QUERY_SIZE, "%s.%s.[0]", category, (char *)action);
        
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_mutex_lock(&lockConfig);
        
        if(!config_lookup_string(&config, config_query, &command_action)) {
        
            pthread_mutex_unlock(&lockConfig);
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
            
            syslog(LOG_WARNING, "Could not identify command: %s", (char *)action);
            return EXIT_SUCCESS;
        }
        
        snprintf(config_query, CONFIG_QUERY_SIZE, "%s.header.[0]", category);
        if(!config_lookup_string(&config, config_query, &command_category)) {
        
            pthread_mutex_unlock(&lockConfig);
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
            
            syslog(LOG_WARNING, "Could not find header for message: %s", category);
            return EXIT_SUCCESS;
        }
        
        pthread_mutex_unlock(&lockConfig);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        snprintf(serial_command, 200, "%s%s%s%s%s", ascii_data.command_header[0], command_category, ascii_data.event_delimiter[0], command_action, ascii_data.command_tail[0]);
    }
        
    if(common_data.interface->send(serial_command, strlen(serial_command)) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    return EXIT_SUCCESS;
} /* end serial_send_ascii */

static int processCommand(char *event_header, char *event) {   
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
        strcmp(current_header, event_header) != 0)
            continue;
        
        current_header = config_setting_name(config_child);
        total_child_entries = config_setting_length(config_child);
        
        for(entry_count = 0; entry_count < total_child_entries; entry_count++) {
            config_entry = config_setting_get_elem(config_child, entry_count);
            if((current_event = config_setting_get_string_elem(config_entry, common_data.diff_commands)) == 0 || 
            strcmp(current_event, event) != 0)
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
            
            syslog(LOG_DEBUG, "Written '%s' to file '%s'", current_event, status_file_path);
            
            pthread_mutex_unlock(&lockConfig);
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
            
            return EXIT_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&lockConfig);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    
    return EXIT_SUCCESS;
} /* serial_process_ascii */

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
        if((event_delimiter_ptr = strstr(event_header_ptr, ascii_data.event_delimiter[common_data.diff_commands])) == NULL ||
        event_delimiter_ptr == event_header_ptr) { // if true assume needle is empty, assume no event header exists.. so volume only
            event_ptr = event_header_ptr;
            event_header_ptr = &serial_command[tail_ptr-header_ptr]; // is or will be NULL character
        }
        else {
            event_ptr = event_delimiter_ptr+ascii_data.event_delimiter_length[common_data.diff_commands];
            *event_delimiter_ptr = '\0';
        }
        
        syslog(LOG_DEBUG, "Detected incoming event (header): %s (%s)", event_ptr, event_header_ptr);
                    
        if(strcmp(event_header_ptr, ascii_data.volume_header[common_data.diff_commands]) == 0) { // if the delimiter is empty, this will always match
        	errno = 0;
            volume_level = strtod(event_ptr, (char **)NULL); // segfault if not number? no, just with more than 1 byte send
            if(errno == 0)
                status = processVolume(&volume_level);
        }
        else
            status = processCommand(event_header_ptr, event_ptr);
        
        /* check if we're still in the buffer, otherwise break loop */
        if((int)message_cpy-((int)device_status_message+bytes_read) >= 0)
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

static int deinit(void) {
	return EXIT_SUCCESS;
}