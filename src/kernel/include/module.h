/*
 *  License:
 *  Copyright (C) 2024 Nikolaj Fot
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of 
 *  the GNU General Public License as published by the Free Software Foundation, version 3.
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *  See the GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License along with this program. 
 *  If not, see https://www.gnu.org/licenses/. 
 */

#ifndef MODULE_H_
#define MODULE_H_

#include <stdint.h>

#include "logging.h"
#include "common.h"


#define MODULE_NAME_SIZE    8
#define MODULE_EXTENSION    ENV_GET("MODULE_EXTENSION", "mdl")
#define MODULE_BASE_PATH    ENV_GET("MODULE_BASE_PATH", "")


/*
Launch module by provided name.
Note: Module should return 100 exit code. If it don't,
dbms will ignore result.

Params:
- module_name - Module name.
- args - Arguments for module.
- buffer - Place, where will stored module answer.
- buffer_size - Size of module answer.
                Note: If answer will larger, then buffer size, it will trunc and
                invoke warn message.

Return -2 if module has error.
Return -1 if module not found.
Return 1 if launch and answer was success.
*/
int MDL_launch_module(char* module_name, char* args, uint8_t* buffer, size_t buffer_size);

#endif
