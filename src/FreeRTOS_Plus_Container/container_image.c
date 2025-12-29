/*
 * Container image management for FreeRTOS
 * Copyright (C) 2025
 */

#include "container.h"

#ifdef configUSE_FILESYSTEM
#include "file_system.h"
#include "lfs.h"

#ifdef MY_DEBUG
#include "xil_printf.h"
#endif

/* Helper function to convert uint32_t to string */
static void uint32_to_string(uint32_t value, char *buffer, size_t buffer_size)
{
    char temp[12]; /* Max uint32_t is 4294967295 (10 digits) + null terminator */
    int i = 0;
    int j;
    
    if (buffer == NULL || buffer_size == 0) {
        return;
    }
    
    /* Handle zero specially */
    if (value == 0) {
        if (buffer_size >= 2) {
            buffer[0] = '0';
            buffer[1] = '\0';
        }
        return;
    }
    
    /* Extract digits in reverse order */
    while (value > 0 && i < 11) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    /* Reverse the string into buffer */
    for (j = 0; j < i && j < (int)(buffer_size - 1); j++) {
        buffer[j] = temp[i - 1 - j];
    }
    buffer[j] = '\0';
}

/* Helper function to ensure directory exists */
static BaseType_t prvEnsureDirectoryExists(const char *pcPath, LittleFSOps_t *lfs_ops)
{
    struct lfs_info info;
    int result;

    /* Check if path exists */
    result = lfs_ops->stat(pcPath, &info);
    
    if (result == LFS_ERR_OK) {
        /* Path exists, check if it's a directory */
        if (info.type == LFS_TYPE_DIR) {
            return pdPASS;
        } else {
            /* Path exists but is not a directory */
            return pdFAIL;
        }
    } else if (result == LFS_ERR_NOENT) {
        /* Directory doesn't exist, create it */
        result = lfs_ops->mkdir(pcPath);
        if (result == LFS_ERR_OK) {
            return pdPASS;
        } else {
            return pdFAIL;
        }
    }
    
    /* Other error */
    return pdFAIL;
}

/* Helper function to check if directory exists */
static BaseType_t prvDirectoryExists(const char *pcPath, LittleFSOps_t *lfs_ops)
{
    struct lfs_info info;
    int result;

    result = lfs_ops->stat(pcPath, &info);
    
    if (result == LFS_ERR_OK && info.type == LFS_TYPE_DIR) {
        return pdTRUE;
    }
    
    return pdFALSE;
}

BaseType_t xContainerUnpackImage(const char *pcImagePath, uint32_t ulContainerID)
{
    FileSystem_t *pxFS;
    LittleFSOps_t *lfs_ops;
    lfs_file_t image_file;
    uint8_t ucNumFiles;
    uint8_t i;
    int result;
    char acContainerDir[64];
    char acFilePath[320]; /* 64 (dir) + 256 (filename) */
    uint8_t aucFilenameBuf[256];
    uint64_t ullFileSize;
    uint8_t *pucBuffer = NULL;
    lfs_file_t output_file;
    BaseType_t xReturn = pdFAIL;

    if (pcImagePath == NULL) {
        return pdFAIL;
    }

    /* Get filesystem instance */
    pxFS = pxGetFileSystem();
    if (pxFS == NULL || pxFS->fs_ops == NULL) {
#ifdef MY_DEBUG
        xil_printf("File system not initialized\r\n");
#endif
        return pdFAIL;
    }
    lfs_ops = (LittleFSOps_t *)pxFS->fs_ops;

    /* Format container directory path: /var/container/<id> */
    {
        char *p = acContainerDir;
        const char *prefix = "/var/container/";
        char id_str[12];
        
        /* Copy prefix */
        while (*prefix && (p - acContainerDir) < (int)sizeof(acContainerDir) - 1) {
            *p++ = *prefix++;
        }
        
        /* Convert ID to string and append */
        uint32_to_string(ulContainerID, id_str, sizeof(id_str));
        prefix = id_str;
        while (*prefix && (p - acContainerDir) < (int)sizeof(acContainerDir) - 1) {
            *p++ = *prefix++;
        }
        *p = '\0';
    }

    /* Check if /var/container exists, create if not */
    if (prvEnsureDirectoryExists("/var", lfs_ops) != pdPASS) {
#ifdef MY_DEBUG
        xil_printf("Failed to ensure /var exists\r\n");
#endif
        return pdFAIL;
    }
    
    if (prvEnsureDirectoryExists("/var/container", lfs_ops) != pdPASS) {
#ifdef MY_DEBUG
        xil_printf("Failed to ensure /var/container exists\r\n");
#endif
        return pdFAIL;
    }

    /* Check if container directory already exists */
    if (prvDirectoryExists(acContainerDir, lfs_ops) == pdTRUE) {
#ifdef MY_DEBUG
        xil_printf("Container directory already exists");
#endif
        /* Container directory already exists, return error */
        return pdFAIL;
    }

    /* Create container directory */
    result = lfs_ops->mkdir(acContainerDir);
    if (result != LFS_ERR_OK) {
#ifdef MY_DEBUG
        xil_printf("Failed to create container directory\r\n");
#endif
        return pdFAIL;
    }

    /* Open image file for reading */
    result = lfs_ops->file_open(&image_file, pcImagePath, LFS_O_RDONLY);
    if (result < 0) {
#ifdef MY_DEBUG
        xil_printf("Failed to open image file\r\n");
#endif
        goto cleanup_dir;
    }

    /* Read number of files (1 byte) */
    result = lfs_ops->file_read(&image_file, &ucNumFiles, 1);
    if (result != 1) {
#ifdef MY_DEBUG
        xil_printf("Failed to read number of files\r\n");
#endif
        goto cleanup_file;
    }

    if (ucNumFiles == 0) {
        /* No files to extract */
        lfs_ops->file_close(&image_file);
        return pdPASS;
    }

    /* Process each file in the image */
    for (i = 0; i < ucNumFiles; i++) {
        /* Read file size (8 bytes, little-endian) */
        result = lfs_ops->file_read(&image_file, &ullFileSize, 8);
        if (result != 8) {
#ifdef MY_DEBUG
            xil_printf("Failed to read file size\r\n");
#endif
            goto cleanup_file;
        }

        /* Read filename (256 bytes) */
        result = lfs_ops->file_read(&image_file, aucFilenameBuf, 256);
        if (result != 256) {
#ifdef MY_DEBUG
            xil_printf("Failed to read filename\r\n");
#endif
            goto cleanup_file;
        }

        /* Ensure filename is null-terminated */
        aucFilenameBuf[255] = '\0';

        /* Build full file path: <container_dir>/<filename> */
        {
            char *p = acFilePath;
            const char *src = acContainerDir;
            
            /* Copy container directory path */
            while (*src && (p - acFilePath) < (int)sizeof(acFilePath) - 1) {
                *p++ = *src++;
            }
            
            /* Add slash */
            if ((p - acFilePath) < (int)sizeof(acFilePath) - 1) {
                *p++ = '/';
            }
            
            /* Copy filename */
            src = (const char *)aucFilenameBuf;
            while (*src && (p - acFilePath) < (int)sizeof(acFilePath) - 1) {
                *p++ = *src++;
            }
            *p = '\0';
        }

        /* Allocate buffer for file content */
        pucBuffer = (uint8_t *)pvPortMalloc(ullFileSize);
        if (pucBuffer == NULL && ullFileSize > 0) {
            goto cleanup_file;
        }

        /* Read file content */
        if (ullFileSize > 0) {
            result = lfs_ops->file_read(&image_file, pucBuffer, ullFileSize);
            if (result != (int)ullFileSize) {
                vPortFree(pucBuffer);
                pucBuffer = NULL;
                goto cleanup_file;
            }
        }

        /* Create and write output file */
        result = lfs_ops->file_open(&output_file, acFilePath, 
                              LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
        if (result < 0) {
            vPortFree(pucBuffer);
            pucBuffer = NULL;
            goto cleanup_file;
        }

        if (ullFileSize > 0) {
            result = lfs_ops->file_write(&output_file, pucBuffer, ullFileSize);
            if (result != (int)ullFileSize) {
                lfs_ops->file_close(&output_file);
                vPortFree(pucBuffer);
                pucBuffer = NULL;
                goto cleanup_file;
            }
        }

        lfs_ops->file_close(&output_file);
        vPortFree(pucBuffer);
        pucBuffer = NULL;
    }

    /* Success */
    xReturn = pdPASS;

cleanup_file:
    lfs_ops->file_close(&image_file);
    
    if (xReturn != pdPASS) {
        /* Cleanup: remove container directory on failure */
        /* Note: lfs_remove requires directory to be empty, 
         * may need to iterate and remove files first */
    }
    
    return xReturn;

cleanup_dir:
    /* Remove the created directory on error */
    lfs_ops->remove(acContainerDir);
    return pdFAIL;
}

BaseType_t xContainerPackImage(uint32_t ulContainerID, const char *pcImagePath)
{
    FileSystem_t *pxFS;
    LittleFSOps_t *lfs_ops;
    lfs_dir_t dir;
    struct lfs_info info;
    lfs_file_t image_file;
    lfs_file_t input_file;
    uint8_t ucNumFiles = 0;
    uint8_t ucFileCount;
    int result;
    char acContainerDir[64];
    char acFilePath[512];
    uint8_t aucFilenameBuf[256];
    uint64_t ullFileSize;
    uint8_t *pucBuffer = NULL;
    BaseType_t xReturn = pdFAIL;

    if (pcImagePath == NULL) {
        return pdFAIL;
    }

    /* Get filesystem instance */
    pxFS = pxGetFileSystem();
    if (pxFS == NULL || pxFS->fs_ops == NULL) {
        return pdFAIL;
    }
    lfs_ops = (LittleFSOps_t *)pxFS->fs_ops;

    /* Build container directory path: /var/container/<id> */
    {
        char *p = acContainerDir;
        const char *prefix = "/var/container/";
        char id_str[12];
        
        /* Copy prefix */
        while (*prefix && (p - acContainerDir) < (int)sizeof(acContainerDir) - 1) {
            *p++ = *prefix++;
        }
        
        /* Convert ID to string and append */
        uint32_to_string(ulContainerID, id_str, sizeof(id_str));
        prefix = id_str;
        while (*prefix && (p - acContainerDir) < (int)sizeof(acContainerDir) - 1) {
            *p++ = *prefix++;
        }
        *p = '\0';
    }

    /* Check if container directory exists and is valid */
    if (prvDirectoryExists(acContainerDir, lfs_ops) != pdTRUE) {
        /* Container directory doesn't exist */
        return pdFAIL;
    }

    /* Open directory */
    result = lfs_ops->dir_open(&dir, acContainerDir);
    if (result < 0) {
        return pdFAIL;
    }

    /* First pass: count files (non-directories only) */
    while (1) {
        result = lfs_ops->dir_read(&dir, &info);
        if (result <= 0) {
            break;
        }

        /* Skip "." and ".." */
        if (strncmp(info.name, ".", 2) == 0 || strncmp(info.name, "..", 3) == 0) {
            continue;
        }

        /* Only count regular files */
        if (info.type == LFS_TYPE_REG) {
            ucNumFiles++;
            if (ucNumFiles == 0) {
                /* Overflow - more than 255 files */
                lfs_ops->dir_close(&dir);
                return pdFAIL;
            }
        }
    }

    if (ucNumFiles == 0) {
        /* No files to pack */
        lfs_ops->dir_close(&dir);
        return pdFAIL;
    }

    /* Rewind directory for second pass */
    lfs_ops->dir_rewind(&dir);

    /* Create image file */
    result = lfs_ops->file_open(&image_file, pcImagePath, 
                          LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (result < 0) {
        lfs_ops->dir_close(&dir);
        return pdFAIL;
    }

    /* Write number of files (1 byte) */
    result = lfs_ops->file_write(&image_file, &ucNumFiles, 1);
    if (result != 1) {
        goto cleanup;
    }

    /* Second pass: pack files */
    ucFileCount = 0;
    while (ucFileCount < ucNumFiles) {
        result = lfs_ops->dir_read(&dir, &info);
        if (result <= 0) {
            break;
        }

        /* Skip "." and ".." */
        if (strncmp(info.name, ".", 2) == 0 || strncmp(info.name, "..", 3) == 0) {
            continue;
        }

        /* Only process regular files */
        if (info.type != LFS_TYPE_REG) {
            continue;
        }

        /* Get file size */
        ullFileSize = (uint64_t)info.size;

        /* Prepare filename buffer (256 bytes, zero-padded) */
        memset(aucFilenameBuf, 0, 256);
        strncpy((char *)aucFilenameBuf, info.name, 255);

        /* Write file size (8 bytes, little-endian) */
        result = lfs_ops->file_write(&image_file, &ullFileSize, 8);
        if (result != 8) {
            goto cleanup;
        }

        /* Write filename (256 bytes) */
        result = lfs_ops->file_write(&image_file, aucFilenameBuf, 256);
        if (result != 256) {
            goto cleanup;
        }

        /* Build full file path: <container_dir>/<filename> */
        {
            char *p = acFilePath;
            const char *src = acContainerDir;
            
            /* Copy container directory path */
            while (*src && (p - acFilePath) < (int)sizeof(acFilePath) - 1) {
                *p++ = *src++;
            }
            
            /* Add slash */
            if ((p - acFilePath) < (int)sizeof(acFilePath) - 1) {
                *p++ = '/';
            }
            
            /* Copy filename from info.name */
            src = info.name;
            while (*src && (p - acFilePath) < (int)sizeof(acFilePath) - 1) {
                *p++ = *src++;
            }
            *p = '\0';
        }

        /* Open input file */
        result = lfs_ops->file_open(&input_file, acFilePath, LFS_O_RDONLY);
        if (result < 0) {
            goto cleanup;
        }

        /* Allocate buffer for file content */
        if (ullFileSize > 0) {
            pucBuffer = (uint8_t *)pvPortMalloc(ullFileSize);
            if (pucBuffer == NULL) {
                lfs_ops->file_close(&input_file);
                goto cleanup;
            }

            /* Read file content */
            result = lfs_ops->file_read(&input_file, pucBuffer, ullFileSize);
            if (result != (int)ullFileSize) {
                lfs_ops->file_close(&input_file);
                vPortFree(pucBuffer);
                pucBuffer = NULL;
                goto cleanup;
            }

            /* Write file content to image */
            result = lfs_ops->file_write(&image_file, pucBuffer, ullFileSize);
            if (result != (int)ullFileSize) {
                lfs_ops->file_close(&input_file);
                vPortFree(pucBuffer);
                pucBuffer = NULL;
                goto cleanup;
            }

            vPortFree(pucBuffer);
            pucBuffer = NULL;
        }

        lfs_ops->file_close(&input_file);
        ucFileCount++;
    }

    /* Verify we packed all files */
    if (ucFileCount != ucNumFiles) {
        goto cleanup;
    }

    /* Success */
    xReturn = pdPASS;

cleanup:
    lfs_ops->file_close(&image_file);
    lfs_ops->dir_close(&dir);
    
    if (xReturn != pdPASS) {
        /* Remove failed image file */
        lfs_ops->remove(pcImagePath);
    }
    
    return xReturn;
}

#endif