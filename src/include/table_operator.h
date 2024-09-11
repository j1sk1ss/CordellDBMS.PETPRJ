/*
 *  Table operator is a simple functions to work with OS filesystem.
 *  Main idea that we have files with sizes near 1 KB for pages, dirs and tables.
 *  Table contains list of dirs (names of file). In same time, every directory contain list pf pages (names of file).
 * 
 *  CordellDBMS source code: https://github.com/j1sk1ss/CordellDBMS.EXMPL
 *  Credits: j1sk1ss
 */

#ifndef FILE_MANAGER_
#define FILE_MANAGER_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#endif