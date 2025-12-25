#include "../FreeRTOS_Plus_ELF/syscall.h"

FreeRTOS_GOT_t *got = get_got();

void taskA(void *_pvParameters) {
    FreeRTOSSyscalls_t *freertos_syscalls_ptr = got->freertos_syscalls;
    freertos_syscalls_ptr->uart_puts("Hello from Task B\n");
}

int __attribute__((section(".text.main"))) main(void) {
    taskA(NULL);

    return 0;
}