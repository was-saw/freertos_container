#include "lfs.h"
#include "syscall.h"

FreeRTOS_GOT_t* got = get_got();

// Simple string to number conversion
static int string_to_int(const char* str) {
    int result = 0;
    int sign = 1;
    
    if (*str == '-') {
        sign = -1;
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return result * sign;
}

// Simple number to string conversion
static void int_to_string(int num, char* str) {
    char temp[12];
    int i = 0;
    int is_negative = 0;
    
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    while (num > 0) {
        temp[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    int pos = 0;
    if (is_negative) {
        str[pos++] = '-';
    }
    
    // Reverse
    while (i > 0) {
        str[pos++] = temp[--i];
    }
    str[pos] = '\0';
}

int main(char* path) {
    LittleFSOps_t* lfs_ops_ptr;
    lfs_file_t file;
    char pwd_dir[configMAX_PATH_LEN];
    char file_path[configMAX_PATH_LEN];
    char count_str[12];
    int count = 0;
    int ret;
    lfs_ssize_t bytes_read, bytes_written;
    
    (void)path; // Unused parameter
    
    // Get LittleFS operations
    lfs_ops_ptr = got->get_lfs_ops();
    if (lfs_ops_ptr == NULL) {
        return -1;
    }
    LittleFSOps_t lfs_ops = *lfs_ops_ptr;
    
    // Get current directory
    got->freertos_syscalls->pwd(pwd_dir);
    
    // Build full path to my_count file
    char* dst = file_path;
    const char* src = pwd_dir;
    while (*src) {
        *dst++ = *src++;
    }
    // Add / if needed
    if (*(dst - 1) != '/') {
        *dst++ = '/';
    }
    // Add filename
    const char* filename = "my_count";
    src = filename;
    while (*src) {
        *dst++ = *src++;
    }
    *dst = '\0';
    
    // Try to open existing file
    ret = lfs_ops.file_open(&file, file_path, LFS_O_RDONLY);
    if (ret >= 0) {
        // File exists, read current count
        bytes_read = lfs_ops.file_read(&file, count_str, sizeof(count_str) - 1);
        if (bytes_read > 0) {
            count_str[bytes_read] = '\0';
            count = string_to_int(count_str);
            // Output the old count
            got->freertos_syscalls->uart_puts(count_str);
            got->freertos_syscalls->uart_puts("\r\n");
        } else {
            // File is empty
            got->freertos_syscalls->uart_puts("0\r\n");
            count = 0;
        }
        
        lfs_ops.file_close(&file);
        
        // Increment count
        count++;
    } else {
        // File doesn't exist, output 0
        got->freertos_syscalls->uart_puts("0\r\n");
        count = 1;
    }
    
    // Convert new count to string
    int_to_string(count, count_str);
    
    // Write new count (create or overwrite)
    ret = lfs_ops.file_open(&file, file_path, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (ret < 0) {
        return -1;
    }
    
    // Calculate string length
    int str_len = 0;
    const char* p = count_str;
    while (*p++) str_len++;
    
    bytes_written = lfs_ops.file_write(&file, count_str, str_len);
    if (bytes_written != str_len) {
        lfs_ops.file_close(&file);
        return -1;
    }
    
    lfs_ops.file_close(&file);
    
    return 0;
}
