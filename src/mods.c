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
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "common.h"
#include "verifyConfig.h"
#include "mods.h"


static int init(void);
static void deinit(void);

mods_t mod = {
    .init = &init,
    .volumeResponse = NULL,
    .command = NULL,
    .deinit = &deinit
};


/***********************************
 * Volume Response Mod 
 ***********************************/

extern modVolumeResponse_t modVolumeResponseRange;
extern modVolumeResponse_t modVolumeResponseCondResize;

static modVolumeResponse_t *modVolumeResponseList[] = {
    &modVolumeResponseRange,
    &modVolumeResponseCondResize,
    NULL
};

static modVolumeResponse_t *getModVolumeResponse(const char *name) {
    modVolumeResponse_t **modVolumeResponse;

    if (!name)
        return NULL;

    for (modVolumeResponse=modVolumeResponseList; *modVolumeResponse; modVolumeResponse++)
        if (!strcasecmp(name, (*modVolumeResponse)->name))
            return *modVolumeResponse;
    
    syslog(LOG_WARNING, "[NOTICE] Setting 'volume.response.mode' is not recognised, ignoring: %s", name);
    return NULL;
}


/***********************************
 * Command Mod 
 ***********************************/

extern modCommand_t modCommandDynaudio;
extern modCommand_t modCommandDenon;

static modCommand_t *modCommandList[] = {
    &modCommandDynaudio,
    &modCommandDenon,
    NULL
};

static modCommand_t *getmodCommand(const char *name) {
    modCommand_t **modCommand;

    if (!name)
        return NULL;

    for (modCommand=modCommandList; *modCommand; modCommand++)
        if (!strcasecmp(name, (*modCommand)->name))
            return *modCommand;
    
    syslog(LOG_WARNING, "[NOTICE] Setting 'command_mod.type' is not recognised, ignoring: %s", name);
    return NULL;
}


/***********************************
 * Mods init/deinit
 ***********************************/

static int init(void) {
    const char* conf_value = NULL;
    if(config_lookup_string(&config, "volume.response.mode", &conf_value) != CONFIG_FALSE &&
    (common_data.mod->volumeResponse = getModVolumeResponse(conf_value))) {
        syslog(LOG_INFO, "[OK] volume.response.mode: %s", conf_value);
    }
    
    if(config_lookup_string(&config, "command_mod.type", &conf_value) != CONFIG_FALSE &&
    (common_data.mod->command = getmodCommand(conf_value))) {
        syslog(LOG_INFO, "[OK] command_mod.type: %s", conf_value);
    }
    
    return EXIT_SUCCESS;
}

static void deinit(void) {

}