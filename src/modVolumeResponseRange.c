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
#include <syslog.h>

#include "common.h"
#include "mods.h"


static void help(void);
static int init(void);
static void process(double *volume_level);
static void deinit(void);

modVolumeResponse_t modVolumeResponseRange = {
    .name = "range",
    .help = &help,
    .init = &init,
    .process = &process,
    .deinit = &deinit
};


/* Amp response offsets and multiplier if applicable */
static int responsePreOffset;
static int responsePostOffset;
static double responseMultiplier;

static void help(void) {
    printf("Range\n"
          );
}

static int init(void) {
    int int_setting;
    
    validateConfigInt(&config, "volume.response.pre_offset", &responsePreOffset, -1, 0, 0, 0);
    validateConfigInt(&config, "volume.response.post_offset", &responsePostOffset, -1, 0, 0, 0);
    validateConfigDouble(&config, "volume.response.multiplier", &responseMultiplier, -1, 0, 0, 1);
    validateConfigBool(&config, "volume.response.invert_multiplier", &int_setting, 0);
#ifdef REDUNDANT // causes segfault, process not yet loaded
    if(strcasecmp(common_data.process->name, "numeric") && (int)responseMultiplier != responseMultiplier) {
        syslog(LOG_ERR, "[ERROR] Numeric does not 'float'! Setting 'volume.response.multiplier'" 
                        " is not an integer: %.2f", responseMultiplier);
        return EXIT_FAILURE;
    }
#endif
    if(int_setting)
        responseMultiplier = 1/responseMultiplier;
    if(!responsePreOffset && !responsePostOffset && responseMultiplier == 1)
        common_data.mod->volumeResponse = NULL;
    
    return EXIT_SUCCESS;
}

static void process(double *volume_level) {
    *volume_level = (*volume_level + responsePreOffset) * responseMultiplier + responsePostOffset;
}

static void deinit(void) {

}