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
#include <string.h>
#include "processData.h"


extern process_method_t processAscii;
extern process_method_t processNumeric;

static process_method_t *processMethods[] = {
    &processNumeric,
    &processAscii,
    NULL
};


process_method_t *getProcessMethod(const char *name) {
    process_method_t **process_type;

    // default to first interface
    if (!name)
        return processMethods[0];

    for (process_type=processMethods; *process_type; process_type++)
        if (!strcasecmp(name, (*process_type)->name))
            return *process_type;

    return NULL;
}