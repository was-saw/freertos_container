#include "projdefs.h"
#include "syscall.h"

FreeRTOS_GOT_t *got = get_got();

int main() {
    char pwd_dir[configMAX_PATH_LEN];
    got->freertos_syscalls->pwd(pwd_dir);
    got->freertos_syscalls->uart_puts(pwd_dir);
    got->freertos_syscalls->uart_puts("\r\n");
    return 0;
}