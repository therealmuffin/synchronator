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


#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <math.h>
#include <stdint.h> // declares uint8_t a.o.

#include "common.h"
#include "processData.h"
#include "mods.h"


static void help(void);
static int init(int x, ...);
static void produce(const void *message);
static int process(void *command, int command_length);
static void deinit(void);

modCommand_t modCommandDynaudio = {
    .name = "dynaudio",
    .help = &help,
    .init = &init,
    .produce = &produce,
    .process = &process,
    .deinit = &deinit
};

static int inputPos, inputLen, zone, defaultInput;

static void help(void) {
    printf("Dynaudio Tail Mod\n"
          );
}

static int init(int x, ...) {
    if(strcmp(common_data.process->name, "numeric") != 0) {
        syslog(LOG_ERR, "[Error] Setting 'data_type' must be 'numeric' for this mod to function: dynaudio");
        return EXIT_FAILURE;
    }
    
    va_list arguments;
    va_start (arguments, x);
    
    int *alwaysMatchTail = va_arg ( arguments, int* );
    *alwaysMatchTail = 1;
    
    int *headerLength = va_arg ( arguments, int* );
    if(*headerLength != 9) {
        syslog(LOG_ERR, "[Error] RX commands are of the wrong size for this mod: %i", *headerLength);
        return EXIT_FAILURE;
    }
    if(*(headerLength+1) != 12) {
        syslog(LOG_ERR, "[Error] TX commands are of the wrong size for this mod: %i", *(headerLength+1));
        return EXIT_FAILURE;
    }
    
    va_end (arguments);

    if(validateConfigInt(&config, "command_mod.zone", &zone, -1, 1, 3, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    if(validateConfigInt(&config, "command_mod.default_input", &defaultInput, -1, 1, 7, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;
        
//   check for compatibility:min length tail, command; // can only check at runtime!!
    inputPos =3;
    inputLen = 5;
    
    return EXIT_SUCCESS;
}

static void produce(const void *message) {
   /***********************************
    * Generating status byte
    ***********************************/
    int statusByte;
#ifdef REDUNDANT
    const char *currentInput;
    char config_query[CONFIG_QUERY_SIZE];
    
    statusInfo.retrieve("input", &currentInput);
    if(currentInput) {
        snprintf(config_query, CONFIG_QUERY_SIZE, "input.%s.[0]", currentInput);
        if((config_lookup_int(&config, config_query, &statusByte)) != CONFIG_TRUE) {
            statusByte = defaultInput;
        }
    }
    else
        statusByte = defaultInput;
    statusByte = statusByte*0x10;
#else
    statusByte = defaultInput*0x10;
#endif
    statusByte += zone;
    
    uint8_t *command = (uint8_t *)message;
    command[7] = statusByte;
    
   /***********************************
    * Generating checksum byte
    ***********************************/
    int checksum;
    int sum = 0;
    int count;
    for(count=0; count < inputLen; count++) {
        sum += *(command+inputPos+count);
    }
    
    int q = ceil(sum/255.00);
    checksum = q*255-sum-(inputLen-q);
    
    if(checksum < 0)
        checksum = checksum & ((1 << 8) - 1);
    
    command[8] = checksum;
    //ROUNDUP(SUM(payload)/255)*255-SUM(payload)-([Payload N]-ROUNDUP(SUM(payload)/255))
}

static int process(void *command, int command_length) {
   /***********************************
    * Verifying status byte (only zone part)
    ***********************************/
    uint8_t *message = (uint8_t *)command;
    // Extract 2 LSB
    int zonenr = message[7] & ((1 << 2) - 1);

    if(zonenr != zone) {
        syslog(LOG_DEBUG, "Incoming command not valid, wrong zone: %i", zonenr);
        return EXIT_FAILURE;
    }
    
    
   /***********************************
    * Update input status if changed
    **********************************/
    static int inputnr_backup;
    int action = message[7] >> 4; // input #
    int category = 0x06; // input message
    if(inputnr_backup != action) {
        inputnr_backup = action;
        common_data.process->processCommand(&category, &action);
    }

   /***********************************
    * Correct volume header if necessary
    ***********************************/
    if(message[5] == 0x05)
        message[5] = 0x04;

    return EXIT_SUCCESS;
}

static void deinit(void) {

}