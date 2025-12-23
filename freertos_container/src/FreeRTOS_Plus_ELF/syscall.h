#ifndef FREERTOS_PLUS_ELF_SYSCALL_H
#define FREERTOS_PLUS_ELF_SYSCALL_H

#include "FreeRTOS.h"
#include "elf_loader.h"
#include "portmacro.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#include <stddef.h>

typedef struct FreeRTOSSyscalls {
    void (*uart_puts)(const char *str);
    // System calls can use file system operations through LittleFSOps_t
} FreeRTOSSyscalls_t;

typedef struct FreeRTOS_GOT {
    FreeRTOSSyscalls_t *freertos_syscalls;
    // 可继续添加其他全局偏移表项
} FreeRTOS_GOT_t;

typedef struct GOT_Entry {
    char       *name;    // 符号名称
    Elf64_Addr *address; // 符号地址
} GOT_Entry_t;

typedef struct GOT {
    size_t      num_entries;
    GOT_Entry_t entrys[]; // GOT 表项的值
} GOT_t;

// 内核实现文件中（如某个.c文件）应有如下定义和初始化：
// FreeRTOSSyscalls_t freertos_syscalls = {
//     .xTaskCreate = xTaskCreate,
//     // 其他系统调用初始化
// };
// GOT_t got = {
//     .freertos_syscalls = (Elf64_Addr)&freertos_syscalls
// };

// 使用非重定位方案时可以通过此函数获取系统调用结构体指针
#define get_got() ((FreeRTOS_GOT_t *)0x80000)

#endif /* FREERTOS_PLUS_ELF_SYSCALL_H */