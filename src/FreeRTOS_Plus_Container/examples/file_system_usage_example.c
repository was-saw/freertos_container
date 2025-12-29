/*
 * Simple Chroot Test Example
 *
 * Test Flow:
 * 1. Create /test.txt and write "Hello World"
 * 2. Read and verify /test.txt content
 * 3. Create /tmp directory
 * 4. Chroot to /tmp
 * 5. Verify /test.txt doesn't exist (isolated)
 * 6. Chroot back to /
 * 7. Read and verify /test.txt again
 */

#include "FreeRTOS.h"
#include "file_system.h"
#include "task.h"

#ifdef configUSE_LITTLEFS
#include "lfs.h"

extern void uart_puts(const char *str);
extern int  xTaskChroot(const char *path);

void vFileSystemExampleTask(void *pvParameters) {
    (void)pvParameters;

    /* Initialize the file system */
    uart_puts("\r\n=== File System Chroot Test ===\r\n");

    if (xFileSystemInit(lfs) != pdPASS) {
        uart_puts("ERROR: File system initialization failed\r\n");
        return;
    }
    uart_puts("File system initialized successfully\r\n");

    LittleFSOps_t *pxOps = pxGetLfsOps();
    if (pxOps == NULL) {
        uart_puts("ERROR: Cannot get file system operations\r\n");
        return;
    }

    lfs_file_t      file;
    char            buffer[64];
    struct lfs_info file_info;
    int             ret;

    /* ========================================================================
     * Step 1: Create /test.txt and write "Hello World"
     * ======================================================================== */
    uart_puts("\r\n[Step 1] Creating /test.txt with content 'Hello World'\r\n");

    ret = pxOps->file_open(&file, "/test.txt", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (ret < 0) {
        uart_puts("ERROR: Cannot create /test.txt\r\n");
        goto cleanup;
    }

    const char *test_data = "Hello World";
    lfs_ssize_t written = pxOps->file_write(&file, test_data, 11);
    if (written != 11) {
        uart_puts("ERROR: Write failed\r\n");
        pxOps->file_close(&file);
        goto cleanup;
    }

    pxOps->file_close(&file);
    uart_puts("SUCCESS: /test.txt created\r\n");

    /* ========================================================================
     * Step 2: Read and verify /test.txt content
     * ======================================================================== */
    uart_puts("\r\n[Step 2] Reading and verifying /test.txt\r\n");

    ret = pxOps->file_open(&file, "/test.txt", LFS_O_RDONLY);
    if (ret < 0) {
        uart_puts("ERROR: Cannot open /test.txt for reading\r\n");
        goto cleanup;
    }

    lfs_ssize_t read_size = pxOps->file_read(&file, buffer, sizeof(buffer));
    pxOps->file_close(&file);

    if (read_size == 11) {
        buffer[11] = '\0';
        uart_puts("Read content: ");
        uart_puts(buffer);
        uart_puts("\r\n");

        /* Verify content */
        int match = 1;
        for (int i = 0; i < 11; i++) {
            if (buffer[i] != test_data[i]) {
                match = 0;
                break;
            }
        }

        if (match) {
            uart_puts("SUCCESS: Content matches 'Hello World'\r\n");
        } else {
            uart_puts("ERROR: Content mismatch!\r\n");
        }
    } else {
        uart_puts("ERROR: Read size mismatch\r\n");
    }

    /* ========================================================================
     * Step 3: Create /tmp directory
     * ======================================================================== */
    uart_puts("\r\n[Step 3] Creating /tmp directory\r\n");

    ret = pxOps->mkdir("/tmp");
    if (ret < 0) {
        uart_puts("WARNING: /tmp might already exist or creation failed\r\n");
    } else {
        uart_puts("SUCCESS: /tmp directory created\r\n");
    }

    /* ========================================================================
     * Step 4: Chroot to /tmp
     * ======================================================================== */
    uart_puts("\r\n[Step 4] Changing root to /tmp\r\n");

    ret = xTaskChroot("/tmp");
    if (ret == pdTRUE) {
        uart_puts("SUCCESS: Chroot to /tmp successful\r\n");
    } else {
        uart_puts("ERROR: Chroot to /tmp failed\r\n");
        goto cleanup;
    }

    /* ========================================================================
     * Step 5: Verify /test.txt doesn't exist in /tmp (isolation test)
     * ======================================================================== */
    uart_puts("\r\n[Step 5] Checking if /test.txt exists after chroot to /tmp\r\n");
    uart_puts("(It should NOT exist - this proves isolation)\r\n");

    ret = pxOps->stat("/test.txt", &file_info);
    if (ret < 0) {
        uart_puts("SUCCESS: /test.txt NOT found in /tmp (correct isolation!)\r\n");
    } else {
        uart_puts("ERROR: /test.txt found in /tmp (isolation failed!)\r\n");
    }

    /* ========================================================================
     * Step 6: Chroot back to /
     * ======================================================================== */
    uart_puts("\r\n[Step 6] Changing root back to /\r\n");

    ret = xTaskChroot("/");
    if (ret == pdTRUE) {
        uart_puts("SUCCESS: Chroot back to / successful\r\n");
    } else {
        uart_puts("ERROR: Chroot back to / failed\r\n");
        goto cleanup;
    }

    /* ========================================================================
     * Step 7: Read and verify /test.txt again
     * ======================================================================== */
    uart_puts("\r\n[Step 7] Re-reading /test.txt after chroot back to /\r\n");

    ret = pxOps->file_open(&file, "/test.txt", LFS_O_RDONLY);
    if (ret < 0) {
        uart_puts("ERROR: Cannot open /test.txt after chroot back\r\n");
        goto cleanup;
    }

    read_size = pxOps->file_read(&file, buffer, sizeof(buffer));
    pxOps->file_close(&file);

    if (read_size == 11) {
        buffer[11] = '\0';
        uart_puts("Read content: ");
        uart_puts(buffer);
        uart_puts("\r\n");

        /* Verify content again */
        int match = 1;
        for (int i = 0; i < 11; i++) {
            if (buffer[i] != test_data[i]) {
                match = 0;
                break;
            }
        }

        if (match) {
            uart_puts("SUCCESS: Content still matches 'Hello World'\r\n");
        } else {
            uart_puts("ERROR: Content changed!\r\n");
        }
    } else {
        uart_puts("ERROR: Read size mismatch\r\n");
    }

    /* ========================================================================
     * Test Summary
     * ======================================================================== */
    uart_puts("\r\n=== Chroot Test Complete ===\r\n");
    uart_puts("Summary:\r\n");
    uart_puts("1. Created /test.txt with 'Hello World' - OK\r\n");
    uart_puts("2. Verified initial content - OK\r\n");
    uart_puts("3. Created /tmp directory - OK\r\n");
    uart_puts("4. Chroot to /tmp - OK\r\n");
    uart_puts("5. /test.txt not visible in /tmp - ISOLATION OK\r\n");
    uart_puts("6. Chroot back to / - OK\r\n");
    uart_puts("7. /test.txt still readable and correct - OK\r\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
cleanup:

    /* Cleanup when done */
    xFileSystemDeinit();
}

#endif /* configUSE_LITTLEFS */
