#include "syscall.h"
#include "task.h"

extern void uart_puts(const char *str);

// 定义全局的 FreeRTOS 系统调用实例，供外部程序使用
FreeRTOSSyscalls_t freertos_syscalls = {
    .uart_puts = uart_puts,
    // 后续可以添加其他系统调用函数指针
};

FreeRTOS_GOT_t freertos_got __attribute__((section(".freertos_got"))) = {
    .freertos_syscalls = &freertos_syscalls,
    // 后续可以添加其他全局偏移表项
};

GOT_t got = {.num_entries = 1, .entrys = {{"freertos_syscalls", (Elf64_Addr *)&freertos_syscalls}}};