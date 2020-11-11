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
#include <string.h>
#include <syslog.h>

#include "common.h"
#include "mods.h"


static void help(void);
static int init(void);
static void processUpper(double *volume_level);
static void processLower(double *volume_level);
static void deinit(void);

modVolumeResponse_t modVolumeResponseCondResize = {
    .name = "conditional_resize",
    .help = &help,
    .init = &init,
    .process = &processUpper,
    .deinit = &deinit
};


/* Amp response offsets and multiplier if applicable */
static int responseLimit;
static double responseDivisor;

static void help(void) {
    printf("Conditional Resize\n"
          );
}

static int init(void) {
const char *type;
    if(validateConfigString(&config, "volume.response.type", &type, -1) == EXIT_SUCCESS) {
        if(strcmp(type, "lower_limit"))
            modVolumeResponseCondResize.process = &processLower;
        else
        if(strcmp(type, "upper_limit"))
            modVolumeResponseCondResize.process = &processUpper;
        else
            return EXIT_FAILURE;
    }
    if(validateConfigInt(&config, "volume.response.limit", &responseLimit, -1, 0, 0, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;
    if(validateConfigDouble(&config, "volume.response.divisor", &responseDivisor, -1, 0, 0, -1) == EXIT_FAILURE)
        return EXIT_FAILURE;
#ifdef REDUNDANT // causes segfault, process not yet loaded
    if(strcasecmp(common_data.process->name, "numeric") && (int)responseDivisor != responseDivisor) {
        syslog(LOG_ERR, "[ERROR] Numeric does not 'float'! Setting 'volume.response.divisor'" 
                        " is not an integer: %.2f", responseDivisor);
        return EXIT_FAILURE;
    }
#endif
    if(responseDivisor == 1)
        common_data.mod->volumeResponse = NULL;

    return EXIT_SUCCESS;
}

static void processUpper(double *volume_level) {
    if(*volume_level > responseLimit)
        *volume_level = *volume_level / responseDivisor;
}

static void processLower(double *volume_level) {
    if(*volume_level < responseLimit)
        *volume_level = *volume_level / responseDivisor;
}

static void deinit(void) {

}