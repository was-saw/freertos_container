#include "lfs.h"
#include "syscall.h"

FreeRTOS_GOT_t* got = get_got();

// Simple hex to string conversion for debugging
static void print_hex(unsigned long value) {
    char hex_str[20];
    char* ptr = hex_str;
    int i;
    
    *ptr++ = '0';
    *ptr++ = 'x';
    
    for (i = 15; i >= 0; i--) {
        unsigned char nibble = (value >> (i * 4)) & 0xF;
        if (nibble < 10)
            *ptr++ = '0' + nibble;
        else
            *ptr++ = 'A' + (nibble - 10);
    }
    *ptr = '\0';
    
    got->freertos_syscalls->uart_puts(hex_str);
}

int main(char* path) {
    (void) path;
    lfs_dir_t dir;
    char ls_dir[configMAX_PATH_LEN];
    struct lfs_info info;
    LittleFSOps_t* lfs_ops_ptr;
    
    /* Debug output */
    got->freertos_syscalls->uart_puts("=== LS Debug Start ===\r\n");
    got->freertos_syscalls->uart_puts("got address: ");
    print_hex((unsigned long)got);
    got->freertos_syscalls->uart_puts("\r\n");
    
    got->freertos_syscalls->uart_puts("got->freertos_syscalls: ");
    print_hex((unsigned long)got->freertos_syscalls);
    got->freertos_syscalls->uart_puts("\r\n");
    
    if (got->freertos_syscalls != NULL) {
        got->freertos_syscalls->uart_puts("syscalls->uart_puts: ");
        print_hex((unsigned long)got->freertos_syscalls->uart_puts);
        got->freertos_syscalls->uart_puts("\r\n");
        
        got->freertos_syscalls->uart_puts("syscalls->pwd: ");
        print_hex((unsigned long)got->freertos_syscalls->pwd);
        got->freertos_syscalls->uart_puts("\r\n");
        
        got->freertos_syscalls->uart_puts("syscalls->set_pwd: ");
        print_hex((unsigned long)got->freertos_syscalls->set_pwd);
        got->freertos_syscalls->uart_puts("\r\n");
    }
    
    got->freertos_syscalls->uart_puts("got->get_lfs_ops: ");
    print_hex((unsigned long)got->get_lfs_ops);
    got->freertos_syscalls->uart_puts("\r\n");
    
    got->freertos_syscalls->uart_puts("Calling get_lfs_ops()...\r\n");
    lfs_ops_ptr = got->get_lfs_ops();
    
    got->freertos_syscalls->uart_puts("get_lfs_ops() returned: ");
    print_hex((unsigned long)lfs_ops_ptr);
    got->freertos_syscalls->uart_puts("\r\n");
    
    if (lfs_ops_ptr == NULL) {
        got->freertos_syscalls->uart_puts("ERROR: lfs_ops_ptr is NULL!\r\n");
        return -1;
    }
    
    LittleFSOps_t lfs_ops = *lfs_ops_ptr;
    got->freertos_syscalls->uart_puts("lfs_ops copied successfully\r\n");
    
    got->freertos_syscalls->uart_puts("Getting current directory...\r\n");
    got->freertos_syscalls->uart_puts("Calling pwd()...\r\n");
    got->freertos_syscalls->pwd(ls_dir);
    got->freertos_syscalls->uart_puts("pwd() returned: ");
    got->freertos_syscalls->uart_puts(ls_dir);
    got->freertos_syscalls->uart_puts("\r\n");
    
    got->freertos_syscalls->uart_puts("Opening directory: ");
    got->freertos_syscalls->uart_puts(ls_dir);
    got->freertos_syscalls->uart_puts("\r\n");
    int ret = lfs_ops.dir_open(&dir, ls_dir);
    if (ret < 0) {
        got->freertos_syscalls->uart_puts("ERROR: Failed to open directory\r\n");
        return -1;
    }
    got->freertos_syscalls->uart_puts("Directory opened successfully\r\n");
    got->freertos_syscalls->uart_puts("=== Listing Contents ===\r\n");
    
    lfs_off_t dir_pos = dir.pos;
    int entry_count = 0;
    int read_ret;
    
    while (1) {
        read_ret = lfs_ops.dir_read(&dir, &info);
        
        /* Exit if no more entries */
        if (read_ret <= 0) {
            break;
        }
        
        /* Avoid infinite loop - check if position changed */
        if (dir.pos == dir_pos) {
            got->freertos_syscalls->uart_puts("(same position, breaking loop)\r\n");
            break;
        }
        dir_pos = dir.pos;
        
        /* Skip . and .. entries */
        if (info.name[0] == '.' && (info.name[1] == '\0' || 
            (info.name[1] == '.' && info.name[2] == '\0'))) {
            continue;
        }
        
        if (info.type == LFS_TYPE_REG) {
            got->freertos_syscalls->uart_puts("File: ");
            got->freertos_syscalls->uart_puts(info.name);
            got->freertos_syscalls->uart_puts("\r\n");
            entry_count++;
        } else if (info.type == LFS_TYPE_DIR) {
            got->freertos_syscalls->uart_puts("Dir:  ");
            got->freertos_syscalls->uart_puts(info.name);
            got->freertos_syscalls->uart_puts("\r\n");
            entry_count++;
        }
    }
    
    got->freertos_syscalls->uart_puts("=== LS Debug End ===\r\n");
    got->freertos_syscalls->uart_puts("Total entries: ");
    // Simple number to string
    if (entry_count == 0) {
        got->freertos_syscalls->uart_puts("0");
    } else {
        char num_str[12];
        int i = 0;
        int temp = entry_count;
        while (temp > 0) {
            num_str[i++] = '0' + (temp % 10);
            temp /= 10;
        }
        // Reverse
        for (int j = 0; j < i / 2; j++) {
            char t = num_str[j];
            num_str[j] = num_str[i - 1 - j];
            num_str[i - 1 - j] = t;
        }
        num_str[i] = '\0';
        got->freertos_syscalls->uart_puts(num_str);
    }
    got->freertos_syscalls->uart_puts("\r\n");
    
    lfs_ops.dir_close(&dir);
    return 0;
}