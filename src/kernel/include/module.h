#ifndef MODULE_H_
#define MODULE_H_

#include <stdint.h>
#include "logging.h"


#define MODULE_NAME_SIZE    8
#define MODULE_EXTENSION    getenv("MODULE_EXTENSION") == NULL ? "mdl" : getenv("MODULE_EXTENSION")
#define MODULE_BASE_PATH    getenv("MODULE_BASE_PATH") == NULL ? "" : getenv("MODULE_BASE_PATH")


/*
Launch module by provided name.

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