#ifndef MY_DEF_H
#define MY_DEF_H

#include <stddef.h>
#include <string.h>

// FreeRTOS memory management functions
extern void *pvPortMalloc(size_t xSize);
extern void  vPortFree(void *pv);

// Define littlefs memory allocation macros to use FreeRTOS functions
#define LFS_MALLOC(sz) pvPortMalloc(sz)
#define LFS_FREE(p) vPortFree(p)

// // Disable standard library malloc since we're using FreeRTOS
// #define LFS_NO_MALLOC

// For embedded systems, we might want to disable some features
// to reduce code size and memory usage
#define LFS_NO_DEBUG
#define LFS_NO_WARN
#define LFS_NO_ERROR
#define LFS_NO_ASSERT

// You can also define custom trace functions if needed
// #define LFS_TRACE(...)

#endif