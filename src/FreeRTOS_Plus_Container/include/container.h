/*
 * Container management for FreeRTOS
 * Copyright (C) 2025
 */

#ifndef CONTAINER_H
#define CONTAINER_H

#include "FreeRTOS.h"
#include "cgroup.h"
#include "ipc_namespace.h"
#include "pid_namespace.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Container states */
typedef enum {
    CONTAINER_STATE_STOPPED,
    CONTAINER_STATE_RUNNING,
    CONTAINER_STATE_PAUSED,
    CONTAINER_STATE_ERROR
} ContainerState_t;

/* Container function type */
typedef void (*ContainerFunction_t)(void *pvParameters);

/* Container structure */
typedef struct Container {
    uint32_t            ulContainerID;
    char                pcContainerName[32];
    ContainerState_t    eState;
    TaskHandle_t        xTaskHandle;
    ContainerFunction_t pxFunction;
    void               *pvParameters;
    uint32_t            ulStackSize;
    UBaseType_t         uxPriority;
    char                pcRootPath[256];
    char                elfName[64];

    /* Resource isolation components */
    CGroupHandle_t       xCGroup;       /* CGroup for resource limits */
    PidNamespaceHandle_t xPidNamespace; /* PID namespace for process isolation */
    IpcNamespaceHandle_t xIpcNamespace; /* IPC namespace for communication isolation */

    /* Container configuration */
    uint32_t ulMemoryLimit; /* Memory limit in bytes (0 = no limit) */
    uint32_t ulCpuQuota;    /* CPU quota percentage * 100 (e.g., 5000 = 50%) */

    /* Synchronization for task startup */
    SemaphoreHandle_t xReadySemaphore; /* Semaphore to signal task can proceed after isolation setup */

    struct Container *pxNext;
} Container_t;

/* Container daemon task priority */
#define CONTAINER_DAEMON_PRIORITY (tskIDLE_PRIORITY + 2)
#define CONTAINER_DAEMON_STACK_SIZE (2048)

/* Container manager functions */
BaseType_t xContainerManagerInit(void);

/* Basic container functions */
BaseType_t xContainerCreate(const char *pcName,
                            const char *elfName,
                            void       *pvParameters,
                            uint32_t    ulStackSize,
                            UBaseType_t uxPriority);

/* Enhanced container creation with resource limits */
BaseType_t xContainerCreateWithLimits(const char *pcName,
                                      const char *elfName,
                                      uint32_t    ulStackSize,
                                      UBaseType_t uxPriority,
                                      uint32_t    ulMemoryLimit,
                                      uint32_t    ulCpuQuota);

BaseType_t   xContainerStart(uint32_t ulContainerID);
BaseType_t   xContainerStop(uint32_t ulContainerID);
BaseType_t   xContainerDelete(uint32_t ulContainerID);
Container_t *pxContainerGetByID(uint32_t ulContainerID);
Container_t *pxContainerGetByName(const char *pcName);
uint32_t     ulContainerGetCount(void);
Container_t *pxContainerGetList(void);

/* Container resource management */
BaseType_t xContainerSetMemoryLimit(uint32_t ulContainerID, uint32_t ulMemoryLimit);
BaseType_t xContainerSetCpuQuota(uint32_t ulContainerID, uint32_t ulCpuQuota);
BaseType_t
xContainerGetStats(uint32_t ulContainerID, uint32_t *pulMemoryUsed, uint32_t *pulCpuUsage);

/* Container daemon task */
void vContainerDaemonTask(void *pvParameters);

/* CLI command functions */
BaseType_t
xContainerCreateCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
BaseType_t
xContainerListCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
BaseType_t
xContainerStartCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
BaseType_t
xContainerStopCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
BaseType_t
xContainerRunCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
BaseType_t
xContainerDeleteCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
BaseType_t
xContainerLoadCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
BaseType_t
xContainerSaveCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
BaseType_t
xContainerImageCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
BaseType_t xRunCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);

/* Register container CLI commands */
void vRegisterContainerCLICommands(void);

/* Container image management functions */

/**
 * @brief Unpack a container image file to /var/container/<id>/ directory
 *
 * This function reads an image file from the specified path and extracts
 * all files to /var/container/<id>/ directory. The image format is:
 * - 1 byte: number of files
 * - For each file:
 *   - 8 bytes: file size (little-endian)
 *   - 256 bytes: filename (null-terminated, zero-padded)
 *   - N bytes: file content
 *
 * @param pcImagePath Path to the image file in LFS filesystem
 * @param ulContainerID Container ID (used for target directory name)
 * @return pdPASS on success, pdFAIL on failure
 *
 * @note Checks if /var/container/<id>/ already exists and returns error if it does
 * @note Creates /var/container/ directory if it doesn't exist
 */
BaseType_t xContainerUnpackImage(const char *pcImagePath, uint32_t ulContainerID);

/**
 * @brief Pack files from a directory into a container image
 *
 * This function packs all files (non-recursively) from the container directory
 * /var/container/<id>/ into a single image file. Only files in the immediate
 * directory are packed, subdirectories are ignored.
 *
 * The output image format is:
 * - 1 byte: number of files
 * - For each file:
 *   - 8 bytes: file size (little-endian)
 *   - 256 bytes: filename (null-terminated, zero-padded)
 *   - N bytes: file content
 *
 * @param ulContainerID Container ID (used for source directory name)
 * @param pcImagePath Path for the output image file
 * @return pdPASS on success, pdFAIL on failure
 *
 * @note Maximum 255 files can be packed
 * @note Only packs files, not subdirectories
 * @note Checks if /var/container/<id>/ exists and is valid
 */
BaseType_t xContainerPackImage(uint32_t ulContainerID, const char *pcImagePath);

#endif /* CONTAINER_H */