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
#include "synchronator.h"
#include "interfaces.h"

#ifndef DISABLE_SERIAL
extern interface_t interface_serial;
#endif
#ifdef ENABLE_I2C
extern interface_t interface_i2c;
#endif
#ifdef ENABLE_TCP
extern interface_t interface_tcp;
#endif


static interface_t *list_interfaces[] = {
#ifndef DISABLE_SERIAL
	&interface_serial,
#endif
#ifdef ENABLE_I2C
	&interface_i2c,
#endif
#ifdef ENABLE_TCP
	&interface_tcp,
#endif
    NULL
};


interface_t *getInterface(const char *name) {
    interface_t **interface;

    // default to first interface
    if (!name)
        return list_interfaces[0];

    for (interface=list_interfaces; *interface; interface++)
        if (!strcasecmp(name, (*interface)->name))
            return *interface;

    return NULL;
}