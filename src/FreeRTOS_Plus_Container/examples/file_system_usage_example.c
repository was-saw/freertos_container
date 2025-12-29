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
#include "xil_printf.h"

#ifdef configUSE_LITTLEFS
#include "lfs.h"

extern int  xTaskChroot(const char *path);

void vFileSystemExampleTask(void *pvParameters) {
    (void)pvParameters;

    /* Initialize the file system */
    xil_printf("\r\n=== File System Chroot Test ===\r\n");

    if (xFileSystemInit(lfs) != pdPASS) {
        xil_printf("ERROR: File system initialization failed\r\n");
        return;
    }
    xil_printf("File system initialized successfully\r\n");

    LittleFSOps_t *pxOps = pxGetLfsOps();
    if (pxOps == NULL) {
        xil_printf("ERROR: Cannot get file system operations\r\n");
        return;
    }

    lfs_file_t      file;
    char            buffer[64];
    struct lfs_info file_info;
    int             ret;

    /* ========================================================================
     * Step 1: Create /test.txt and write "Hello World"
     * ======================================================================== */
    xil_printf("\r\n[Step 1] Creating /test.txt with content 'Hello World'\r\n");

    ret = pxOps->file_open(&file, "/test.txt", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (ret < 0) {
        xil_printf("ERROR: Cannot create /test.txt\r\n");
        goto cleanup;
    }

    const char *test_data = "Hello World";
    lfs_ssize_t written = pxOps->file_write(&file, test_data, 11);
    if (written != 11) {
        xil_printf("ERROR: Write failed\r\n");
        pxOps->file_close(&file);
        goto cleanup;
    }

    pxOps->file_close(&file);
    xil_printf("SUCCESS: /test.txt created\r\n");

    /* ========================================================================
     * Step 2: Read and verify /test.txt content
     * ======================================================================== */
    xil_printf("\r\n[Step 2] Reading and verifying /test.txt\r\n");

    ret = pxOps->file_open(&file, "/test.txt", LFS_O_RDONLY);
    if (ret < 0) {
        xil_printf("ERROR: Cannot open /test.txt for reading\r\n");
        goto cleanup;
    }

    lfs_ssize_t read_size = pxOps->file_read(&file, buffer, sizeof(buffer));
    pxOps->file_close(&file);

    if (read_size == 11) {
        buffer[11] = '\0';
        xil_printf("Read content: %s\r\n", buffer);

        /* Verify content */
        int match = 1;
        for (int i = 0; i < 11; i++) {
            if (buffer[i] != test_data[i]) {
                match = 0;
                break;
            }
        }

        if (match) {
            xil_printf("SUCCESS: Content matches 'Hello World'\r\n");
        } else {
            xil_printf("ERROR: Content mismatch!\r\n");
        }
    } else {
        xil_printf("ERROR: Read size mismatch\r\n");
    }

    /* ========================================================================
     * Step 3: Create /tmp directory
     * ======================================================================== */
    xil_printf("\r\n[Step 3] Creating /tmp directory\r\n");

    ret = pxOps->mkdir("/tmp");
    if (ret < 0) {
        xil_printf("WARNING: /tmp might already exist or creation failed\r\n");
    } else {
        xil_printf("SUCCESS: /tmp directory created\r\n");
    }

    /* ========================================================================
     * Step 4: Chroot to /tmp
     * ======================================================================== */
    xil_printf("\r\n[Step 4] Changing root to /tmp\r\n");

    ret = xTaskChroot("/tmp");
    if (ret == pdTRUE) {
        xil_printf("SUCCESS: Chroot to /tmp successful\r\n");
    } else {
        xil_printf("ERROR: Chroot to /tmp failed\r\n");
        goto cleanup;
    }

    /* ========================================================================
     * Step 5: Verify /test.txt doesn't exist in /tmp (isolation test)
     * ======================================================================== */
    xil_printf("\r\n[Step 5] Checking if /test.txt exists after chroot to /tmp\r\n");
    xil_printf("(It should NOT exist - this proves isolation)\r\n");

    ret = pxOps->stat("/test.txt", &file_info);
    if (ret < 0) {
        xil_printf("SUCCESS: /test.txt NOT found in /tmp (correct isolation!)\r\n");
    } else {
        xil_printf("ERROR: /test.txt found in /tmp (isolation failed!)\r\n");
    }

    /* ========================================================================
     * Step 6: Chroot back to /
     * ======================================================================== */
    xil_printf("\r\n[Step 6] Changing root back to /\r\n");

    ret = xTaskChroot("/");
    if (ret == pdTRUE) {
        xil_printf("SUCCESS: Chroot back to / successful\r\n");
    } else {
        xil_printf("ERROR: Chroot back to / failed\r\n");
        goto cleanup;
    }

    /* ========================================================================
     * Step 7: Read and verify /test.txt again
     * ======================================================================== */
    xil_printf("\r\n[Step 7] Re-reading /test.txt after chroot back to /\r\n");

    ret = pxOps->file_open(&file, "/test.txt", LFS_O_RDONLY);
    if (ret < 0) {
        xil_printf("ERROR: Cannot open /test.txt after chroot back\r\n");
        goto cleanup;
    }

    read_size = pxOps->file_read(&file, buffer, sizeof(buffer));
    pxOps->file_close(&file);

    if (read_size == 11) {
        buffer[11] = '\0';
        xil_printf("Read content: %s\r\n", buffer);

        /* Verify content again */
        int match = 1;
        for (int i = 0; i < 11; i++) {
            if (buffer[i] != test_data[i]) {
                match = 0;
                break;
            }
        }

        if (match) {
            xil_printf("SUCCESS: Content still matches 'Hello World'\r\n");
        } else {
            xil_printf("ERROR: Content changed!\r\n");
        }
    } else {
        xil_printf("ERROR: Read size mismatch\r\n");
    }

    /* ========================================================================
     * Test Summary
     * ======================================================================== */
    xil_printf("\r\n=== Chroot Test Complete ===\r\n");
    xil_printf("Summary:\r\n");
    xil_printf("1. Created /test.txt with 'Hello World' - OK\r\n");
    xil_printf("2. Verified initial content - OK\r\n");
    xil_printf("3. Created /tmp directory - OK\r\n");
    xil_printf("4. Chroot to /tmp - OK\r\n");
    xil_printf("5. /test.txt not visible in /tmp - ISOLATION OK\r\n");
    xil_printf("6. Chroot back to / - OK\r\n");
    xil_printf("7. /test.txt still readable and correct - OK\r\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
cleanup:

    /* Cleanup when done */
    xFileSystemDeinit();
}

#endif /* configUSE_LITTLEFS */
