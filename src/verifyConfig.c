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
#include <syslog.h>

#include "synchronator.h"
#include "verifyConfig.h"

int validateConfigString(config_t *config, char *configSettingSource, 
const char **configSettingDestination, int position) {
    if(position >= 0) {
        char configQuery[CONFIG_QUERY_SIZE];
        snprintf(configQuery, CONFIG_QUERY_SIZE, "%s.[%i]", configSettingSource, position);
        configSettingSource = &configQuery[0];
    }
    
    if(!config_lookup_string(config, configSettingSource, configSettingDestination)) {
        syslog(LOG_ERR, "[ERROR] Setting not present or formatted as string: %s", configSettingSource);
        return EXIT_FAILURE;
    }
    else {
        syslog(LOG_INFO, "[OK] %s: %s", configSettingSource, *configSettingDestination);
        return EXIT_SUCCESS;
    }
}

int validateConfigInt(config_t *config, char *configSettingSource, int *configSettingDestination, 
int position, int valueMin, int valueMax, int valueDefault) {
    int configReturnedInt = -1;
    if(position >= 0) {
        char configQuery[CONFIG_QUERY_SIZE];
        snprintf(configQuery, CONFIG_QUERY_SIZE, "%s.[%i]", configSettingSource, position);
        configSettingSource = &configQuery[0];
    }
    
    if(!config_lookup_int(config, configSettingSource, &configReturnedInt)) {
        if(valueDefault == -1) {
            syslog(LOG_ERR, "[ERROR] Setting not present or not an integer: %s", configSettingSource);
            return EXIT_FAILURE;
        } 
        else if (valueDefault == -2) {
            syslog(LOG_INFO, "[NOTICE] Setting not found, ignoring %s", configSettingSource);
            return EXIT_FAILURE;
        } 
        else
            configReturnedInt = valueDefault;
    }
    if(!(valueMin == 0 && valueMax == 0) && (configReturnedInt < valueMin || configReturnedInt > valueMax)) {
        syslog(LOG_ERR, "[ERROR] Setting '%s' is out of range: %i [%i-%i]", configSettingSource, 
                configReturnedInt, valueMin, valueMax);
        return EXIT_FAILURE;
    }
    *configSettingDestination = configReturnedInt;
    syslog(LOG_INFO, "[OK] %s: %i", configSettingSource, *configSettingDestination);
    return EXIT_SUCCESS;
}

int validateConfigDouble(config_t *config, char *configSettingSource, double *configSettingDestination, 
int position, int valueMin, int valueMax, double valueDefault) {
    double configReturnedDouble = -1;
    if(position >= 0) {
        char configQuery[CONFIG_QUERY_SIZE];
        snprintf(configQuery, CONFIG_QUERY_SIZE, "%s.[%i]", configSettingSource, position);
        configSettingSource = &configQuery[0];
    }
    
    if(!config_lookup_float(config, configSettingSource, &configReturnedDouble)) {
        if(valueDefault == -1) {
            syslog(LOG_ERR, "[ERROR] Setting not present or not an integer/float: %s", configSettingSource);
            return EXIT_FAILURE;
        } 
        else if (valueDefault == -2) {
            syslog(LOG_INFO, "[NOTICE] Setting not found, ignoring %s", configSettingSource);
            return EXIT_FAILURE;
        } 
        else
            configReturnedDouble = valueDefault;
    }
    if(!(valueMin == 0 && valueMax == 0) && (configReturnedDouble < valueMin || configReturnedDouble > valueMax)) {
        syslog(LOG_ERR, "[ERROR] Setting '%s' is out of range: %.2f [%i-%i]", configSettingSource, 
                configReturnedDouble, valueMin, valueMax);
        return EXIT_FAILURE;
    }
    *configSettingDestination = configReturnedDouble;
    syslog(LOG_INFO, "[OK] %s: %.2f", configSettingSource, *configSettingDestination);
    return EXIT_SUCCESS;
}

int validateConfigBool(config_t *config, char *configSettingSource, int *configSettingDestination, 
int valueDefault) {
    const char* boolNaming[2];
    boolNaming[0] = "FALSE";
    boolNaming[1] = "TRUE";
    
    if(!config_lookup_bool(config, configSettingSource, configSettingDestination))
        *configSettingDestination = valueDefault;
    else {
        syslog(LOG_INFO, "[OK] %s: %s", configSettingSource, boolNaming[*configSettingDestination]);
        return EXIT_SUCCESS;
    }    
    return EXIT_SUCCESS;
}