/*
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 *
 *  Took from: https://github.com/j1sk1ss/CordellOS.PETPRJ/blob/grub-based-bootloader/src/libs/include/hash.h
 */

#ifndef HASH_H_
#define HASH_H_

#include <string.h>
#include <stdint.h>


#define SALT    "CordellDBMS_SHA"
#define MAGIC   8


/*
Hash generator.

Params:
- str - input string for hash.

Return hash (uint32_t)
*/
uint32_t HASH_str2hash(const char* str);

#endif
