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
#include <syslog.h>
#include <math.h>

#include "common.h"
#include "processData.h"
#include "verifyConfig.h"
#include "mods.h"


static void help(void);
static int init(int x, ...);
static void produce(const void *message);
static int process(void *command, int command_length);
static void deinit(void);

modCommand_t modCommandDenon = {
    .name = "denon-avr",
    .help = &help,
    .init = &init,
    .produce = &produce,
    .process = &process,
    .deinit = &deinit
};

static int inputPos, inputLen, zone, defaultInput;

static void help(void) {
    printf("Denon Tail Mod\n"
          );
}

static int init(int x, ...) {
    if(strcmp(common_data.process->name, "ascii") != 0) {
        syslog(LOG_ERR, "[Error] Setting 'data_type' must be 'ascii' for this mod to function: denon");
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

static void produce(const void *message) {

}

static int process(void *command, int command_length) {
    if(command_length != 5)
        return EXIT_SUCCESS;

    char *message = (char *)command;
    if(message[0] == 'M' && message[1] == 'V' && (message[4] >= '0' && message[4] <= '9')) {
        message[4] = '\0';
    }
    
    return EXIT_SUCCESS;
}

static void deinit(void) {

}