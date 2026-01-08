#include "lfs.h"
#include "projdefs.h"
#include "syscall.h"

FreeRTOS_GOT_t *got = get_got();

// Simple string copy implementation for standalone executable
static void string_copy(char *dest, const char *src) {
    while ((*dest++ = *src++) != '\0');
}

int main(char* path) {
    lfs_dir_t dir;
    char dest_dir[configMAX_PATH_LEN];
    LittleFSOps_t lfs_ops = *(got->get_lfs_ops());
    int ret = 0;
    if (path[0] == '\0') {
        return pdFAIL;
    } else {
        string_copy(dest_dir, path);
    }
    got->freertos_syscalls->pwd(dest_dir);
    ret = lfs_ops.dir_open(&dir, dest_dir);
    if (ret != 0) {
        got->freertos_syscalls->uart_puts("Failed to cd directory: ");
        got->freertos_syscalls->uart_puts(dest_dir);
        got->freertos_syscalls->uart_puts("\r\n");
        return ret;
    } else {
        lfs_ops.dir_close(&dir);
        got->freertos_syscalls->set_pwd(dest_dir);
        return pdPASS;
    }
}