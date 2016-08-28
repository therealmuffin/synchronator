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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>

#include "processData.h"
#include "common.h"


/***********************************
 * Process method selection
 ***********************************/

extern process_method_t processAscii;
extern process_method_t processNumeric;

static process_method_t *processMethods[] = {
    &processNumeric,
    &processAscii,
    NULL
};

process_method_t *getProcessMethod(const char **name) {
    process_method_t **process_type;

    // default to first interface
    if (!*name)
        *name = processMethods[0]->name;

    for (process_type=processMethods; *process_type; process_type++)
        if (!strcasecmp(*name, (*process_type)->name))
            return *process_type;

    return NULL;
}

/***********************************
* Device status functions + data
**********************************/

static int updateStatus(const char *name, const char *value);
static int processStatus(const char *name, const char *value);
static void retrieveStatus(const char *name, const char **value);
static void deinitStatus(void);

struct status_t {
    const char *name;
    const char *value;
    struct status_t *nextStatus;
};
typedef struct status_t status_t;
 // statusManager
statusInfo_t statusInfo = {
    .update = &processStatus,
    .retrieve = &retrieveStatus,
    .deinit = &deinitStatus
};

static status_t *statusRoot = NULL;

static int processStatus(const char *name, const char *value) {
    const char *storedValue;
    if(strcmp(name, "power") == 0 && strcmp(value, "deviceon") == 0) {
        retrieveStatus(name, &storedValue);
        if(storedValue != value) {
            sleep(1); // to allow the amp to wake up 
            common_data.reinitVolume();
        }
    }
    
    updateStatus(name, value);

    return EXIT_SUCCESS;
}

static int updateStatus(const char *name, const char *value) {
    status_t *statusCurrent = statusRoot;
    
    if(statusRoot != NULL) {
        while(1) {
            if (strcmp(name, statusCurrent->name) == 0) {
                statusCurrent->value = value;
                return EXIT_SUCCESS;
            }
            if(statusCurrent->nextStatus == NULL)
                break;
            statusCurrent = statusCurrent->nextStatus;
        }
        statusCurrent->nextStatus = (status_t *)malloc(sizeof(status_t));
        statusCurrent = statusCurrent->nextStatus;
    }
    else {
        statusRoot = (status_t *)malloc(sizeof(status_t));
        statusCurrent = statusRoot;
    }
    
    if (statusCurrent == NULL) {
        syslog(LOG_ERR, "Could not save status: out of memory?");
        return EXIT_FAILURE;
    }

    statusCurrent->name = name;
    statusCurrent->value = value;
    statusCurrent->nextStatus = NULL;

    return EXIT_SUCCESS;
}

static void retrieveStatus(const char *name, const char **value) {
    status_t *statusCurrent = statusRoot;
    
    while(statusCurrent != NULL) {
        if (strcmp(name, statusCurrent->name) == 0) {
            *value = statusCurrent->value;
            return;
        }
        statusCurrent = statusCurrent->nextStatus;
    }
    
    *value = NULL;
}

static void deinitStatus(void) {
    status_t *statusCurrent = statusRoot;
    status_t *statusBackup;
    while(statusCurrent != NULL) {
        statusBackup = statusCurrent->nextStatus;
        syslog(LOG_DEBUG, "Deleted status: %s", statusCurrent->name);
        free(statusCurrent);
        statusCurrent = statusBackup;
    }
}
