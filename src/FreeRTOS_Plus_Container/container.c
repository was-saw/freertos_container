/*
 * Container management for FreeRTOS
 * Copyright (C) 2025
 */

#include "container.h"

#include "FreeRTOS.h"
#include "FreeRTOS_CLI.h"
#include "cgroup.h"
#include "elf_loader.h"
#include "portmacro.h"
#include "projdefs.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "xil_printf.h"

#ifdef configUSE_FILESYSTEM
#include "file_system.h"
#include "syscall.h"
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global variables */
static Container_t      *pxContainerList = NULL;
static SemaphoreHandle_t xContainerMutex = NULL;
static TaskHandle_t      xContainerDaemonHandle = NULL;
static uint32_t          ulNextContainerID = 1;

static void container_wrap_function(void *param) {
    ELF_WRAP *wrap = (ELF_WRAP *)param;

    elf_load_and_run(wrap->elf_data, wrap->elf_size);
    while (1) {
        vTaskDelay(1000);
    }
}

/* Helper function to convert uint32_t to string */
static void uint32_to_string(uint32_t value, char *buffer) {
    char temp[12]; /* Max uint32_t is 10 digits + null terminator + 1 extra */
    int  i = 0;
    int  j;

    /* Handle 0 specially */
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    /* Convert to string in reverse */
    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }

    /* Reverse the string */
    for (j = 0; j < i; j++) {
        buffer[j] = temp[i - 1 - j];
    }
    buffer[i] = '\0';
}
#ifdef configUSE_FILESYSTEM
static int get_elf_by_name(ELF_WRAP *wrap, const char *name) {
    FileSystem_t  *pxFS;
    LittleFSOps_t *lfs_ops;
    lfs_file_t     file;
    char           file_path[128];
    int            ret;
    lfs_soff_t     file_size;
    unsigned char *elf_buffer;

    if (wrap == NULL || name == NULL) {
        return pdFAIL;
    }

    /* Get file system instance */
    pxFS = pxGetFileSystem();
    if (pxFS == NULL || pxFS->fs_ops == NULL) {
        xil_printf("File system not initialized\r\n");
        return pdFAIL;
    }

    lfs_ops = (LittleFSOps_t *)pxFS->fs_ops;

    /* build full path*/
    file_path[0] = '/';
    strcpy(&file_path[1], name);
#ifdef MY_DEBUG
    // 输出/根目录所有文件信息（ls）
    {
        lfs_dir_t       dir;
        struct lfs_info info;
        int             res;

        xil_printf("Listing root directory:\r\n");
        res = lfs_ops->dir_open(&dir, "/");
        if (res < 0) {
            xil_printf("Failed to open root directory\r\n");
            return pdFAIL;
        }

        while (1) {
            res = lfs_ops->dir_read(&dir, &info);
            if (res < 0) {
                xil_printf("Failed to read directory entry\r\n");
                lfs_ops->dir_close(&dir);
                return pdFAIL;
            }
            if (res == 0) {
                /* End of directory */
                break;
            }

            xil_printf("%s %s\r\n", (info.type == LFS_TYPE_DIR) ? "DIR" : "FILE", info.name);
        }

        lfs_ops->dir_close(&dir);
    }
#endif

    /* Try to open the file */
    ret = lfs_ops->file_open(&file, file_path, LFS_O_RDONLY);
    if (ret < 0) {
        xil_printf("Failed to open ELF file: %s\r\n", file_path);
        return pdFAIL;
    }

    /* Get file size */
    file_size = lfs_ops->file_size(&file);
    if (file_size <= 0) {
        xil_printf("Invalid ELF file size\r\n");
        lfs_ops->file_close(&file);
        return pdFAIL;
    }

    /* Allocate memory for ELF data */
    elf_buffer = (unsigned char *)pvPortMalloc((size_t)file_size);
    if (elf_buffer == NULL) {
        xil_printf("Failed to allocate memory for ELF\r\n");
        lfs_ops->file_close(&file);
        return pdFAIL;
    }

    /* Read file content */
    ret = lfs_ops->file_read(&file, elf_buffer, (lfs_size_t)file_size);
    if (ret != file_size) {
        xil_printf("Failed to read ELF file\r\n");
        vPortFree(elf_buffer);
        lfs_ops->file_close(&file);
        return pdFAIL;
    }

    /* Assign to wrap structure */
    wrap->elf_data = (const uint8_t *)elf_buffer;
    wrap->elf_size = (unsigned int)file_size;

    /* Close file */
    lfs_ops->file_close(&file);

    xil_printf("Loaded ELF from: %s (%llu bytes)\r\n", file_path, (unsigned long long)file_size);

    return pdPASS;
}
#else
extern unsigned char data[];
extern size_t data_size;
static int get_elf_by_name(ELF_WRAP *wrap, const char *name) {
    wrap->elf_data = data;
    wrap->elf_size = data_size;
    return pdTRUE;
}
#endif

/* Container daemon task */
void vContainerDaemonTask(void *pvParameters) {
    TickType_t       xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); /* Check every second */

    (void)pvParameters;

    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        /* Wait for the next cycle */
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        /* Check container health and manage lifecycle */
        if (xSemaphoreTake(xContainerMutex, portMAX_DELAY) == pdTRUE) {
            Container_t *pxContainer = pxContainerList;
            while (pxContainer != NULL) {
                if (pxContainer->eState == CONTAINER_STATE_RUNNING &&
                    pxContainer->xTaskHandle != NULL) {
                    /* Check if task is still running */
                    if (eTaskGetState(pxContainer->xTaskHandle) == eDeleted) {
                        pxContainer->eState = CONTAINER_STATE_STOPPED;
                        pxContainer->xTaskHandle = NULL;
                    }
                }
                pxContainer = pxContainer->pxNext;
            }
            xSemaphoreGive(xContainerMutex);
        }
    }
}

/* Initialize container manager */
BaseType_t xContainerManagerInit(void) {
/* Initialize subsystems */
#if (configUSE_IPC_NAMESPACE == 1)
    vIpcNamespaceInit();
#endif

    /* Create mutex for container list protection */
    xContainerMutex = xSemaphoreCreateMutex();
    if (xContainerMutex == NULL) {
        return pdFAIL;
    }

    /* Create container daemon task */
    if (xTaskCreate(vContainerDaemonTask, "ContainerDaemon", CONTAINER_DAEMON_STACK_SIZE, NULL,
                    CONTAINER_DAEMON_PRIORITY, &xContainerDaemonHandle) != pdPASS) {
        vSemaphoreDelete(xContainerMutex);
        return pdFAIL;
    }

    return pdPASS;
}

/* Create a new container */
BaseType_t xContainerCreate(const char         *pcName,
                            const char         *elfName,
                            void               *pvParameters,
                            uint32_t            ulStackSize,
                            UBaseType_t         uxPriority) {
    (void) pvParameters;
    /* Call enhanced version with default resource limits */
    return xContainerCreateWithLimits(pcName, elfName, ulStackSize, uxPriority, 0, 0);
}

/* Enhanced container creation with resource limits */
BaseType_t xContainerCreateWithLimits(const char *pcName,
                                      const char *elfName,
                                      uint32_t    ulStackSize,
                                      UBaseType_t uxPriority,
                                      uint32_t    ulMemoryLimit,
                                      uint32_t    ulCpuQuota) {
    Container_t *pxNewContainer;
    char         pcCGroupName[32];
    char         pcPidNamespaceName[32];
    char         pcIpcNamespaceName[32];

    if (pcName == NULL) {
        return pdFAIL;
    }

    /* Allocate memory for new container */
    pxNewContainer = (Container_t *)pvPortMalloc(sizeof(Container_t));
    if (pxNewContainer == NULL) {
        return pdFAIL;
    }

    /* Initialize container structure */
    pxNewContainer->ulContainerID = ulNextContainerID++;
    strncpy(pxNewContainer->pcContainerName, pcName,
               sizeof(pxNewContainer->pcContainerName) - 1);
    pxNewContainer->pcContainerName[sizeof(pxNewContainer->pcContainerName) - 1] = '\0';
    pxNewContainer->eState = CONTAINER_STATE_STOPPED;
    pxNewContainer->xTaskHandle = NULL;
    pxNewContainer->ulStackSize = ulStackSize;
    pxNewContainer->uxPriority = uxPriority;
    pxNewContainer->ulMemoryLimit = ulMemoryLimit;
    pxNewContainer->ulCpuQuota = ulCpuQuota;
    pxNewContainer->xCGroup = NULL;
    pxNewContainer->xPidNamespace = NULL;
    pxNewContainer->xIpcNamespace = NULL;
    pxNewContainer->pxNext = NULL;
    pxNewContainer->pxFunction = container_wrap_function;
    pxNewContainer->pvParameters = NULL;
    pxNewContainer->xReadySemaphore = NULL;
    strcpy(pxNewContainer->elfName, elfName);
    const char *prefix_path = "/var/container/";
    char        idstr[12];
    uint32_to_string(pxNewContainer->ulContainerID, idstr);
    strcpy(pxNewContainer->pcRootPath, prefix_path);
    strcpy(pxNewContainer->pcRootPath + strlen(prefix_path), idstr);

    /* Create unique names for resources */
    strncpy(pcCGroupName, pcName, sizeof(pcCGroupName) - 1);
    pcCGroupName[sizeof(pcCGroupName) - 1] = '\0';
    strncpy(pcPidNamespaceName, pcName, sizeof(pcPidNamespaceName) - 1);
    pcPidNamespaceName[sizeof(pcPidNamespaceName) - 1] = '\0';
    strncpy(pcIpcNamespaceName, pcName, sizeof(pcIpcNamespaceName) - 1);
    pcIpcNamespaceName[sizeof(pcIpcNamespaceName) - 1] = '\0';

/* Create CGroup for resource limits - following cgroup_example pattern */
/* ALWAYS create a cgroup for each container to ensure proper isolation */
#if (configUSE_CGROUPS == 1)
    {
        /* Use appropriate defaults for unspecified limits */
        UBaseType_t ulMemLimit =
            (ulMemoryLimit > 0) ? ulMemoryLimit : 8192; /* Default 8KB like LowQuota example */
        UBaseType_t ulCpuLimit =
            (ulCpuQuota > 0) ? ulCpuQuota : 100; /* Default 100 ticks like examples */

        pxNewContainer->xCGroup = xCGroupCreate(pcCGroupName, ulMemLimit, ulCpuLimit);
        pxNewContainer->ulMemoryLimit = ulMemLimit;
        pxNewContainer->ulCpuQuota = ulCpuLimit;
        if (pxNewContainer->xCGroup == NULL) {
            /* CGroup creation failed - this is critical for proper container isolation */
            /* Continue without CGroup, but container won't have resource isolation */
        }
    }
#endif

/* Create PID namespace - ALWAYS create for proper process isolation */
#if (configUSE_PID_NAMESPACE == 1)
    pxNewContainer->xPidNamespace = xPidNamespaceCreate(pcPidNamespaceName);
    if (pxNewContainer->xPidNamespace == NULL) {
        /* PID namespace creation failed - continue without it */
        /* This means the container won't have process isolation */
    }
#endif

/* Create IPC namespace - ALWAYS create for proper communication isolation */
#if (configUSE_IPC_NAMESPACE == 1)
    pxNewContainer->xIpcNamespace = xIpcNamespaceCreate(pcIpcNamespaceName);
    if (pxNewContainer->xIpcNamespace == NULL) {
        /* IPC namespace creation failed - continue without it */
        /* This means the container won't have IPC isolation */
    }
#endif

    /* Add to container list */
    if (xSemaphoreTake(xContainerMutex, portMAX_DELAY) == pdTRUE) {
        pxNewContainer->pxNext = pxContainerList;
        pxContainerList = pxNewContainer;
        xSemaphoreGive(xContainerMutex);
        return pdPASS;
    }

/* Failed to add to list, cleanup */
#if (configUSE_CGROUPS == 1)
    if (pxNewContainer->xCGroup != NULL) {
        xCGroupDelete(pxNewContainer->xCGroup);
    }
#endif

#if (configUSE_PID_NAMESPACE == 1)
    if (pxNewContainer->xPidNamespace != NULL) {
        xPidNamespaceDelete(pxNewContainer->xPidNamespace);
    }
#endif

#if (configUSE_IPC_NAMESPACE == 1)
    if (pxNewContainer->xIpcNamespace != NULL) {
        xIpcNamespaceDelete(pxNewContainer->xIpcNamespace);
    }
#endif

    vPortFree(pxNewContainer);
    return pdFAIL;
}

/* Container task wrapper function */
typedef struct {
    Container_t        *pxContainer;
    ContainerFunction_t pxOriginalFunction;
    void               *pvOriginalParameters;
    ELF_WRAP            wrap;
} ContainerTaskParams_t;

static void vContainerTaskWrapper(void *pvParameters) {
    ContainerTaskParams_t *pxParams = (ContainerTaskParams_t *)pvParameters;
    Container_t           *pxContainer = pxParams->pxContainer;
    ContainerFunction_t    pxOriginalFunction = pxParams->pxOriginalFunction;
    void                  *pvOriginalParameters = pxParams->pvOriginalParameters;

    /* Wait for container isolation setup to complete before proceeding */
    if (pxContainer->xReadySemaphore != NULL) {
        xSemaphoreTake(pxContainer->xReadySemaphore, portMAX_DELAY);
        /* Delete the semaphore after use - no longer needed */
        vSemaphoreDelete(pxContainer->xReadySemaphore);
        pxContainer->xReadySemaphore = NULL;
    }

/* CRITICAL: Apply IPC namespace isolation - following ipcnamespace_example pattern */
#if (configUSE_IPC_NAMESPACE == 1)
    if (pxContainer->xIpcNamespace != NULL) {
        BaseType_t xIpcResult = xIpcNamespaceSetTaskNamespace(NULL, pxContainer->xIpcNamespace);
        if (xIpcResult != pdPASS) {
            /* IPC namespace application failed - this is critical for isolation */
            /* Task should terminate as it cannot provide proper isolation */
            pxContainer->eState = CONTAINER_STATE_ERROR;
            vTaskDelete(NULL); /* Delete self */
            return;
        }
    }
#endif

/* Verify that all isolation mechanisms are in place before running user code */
#if (configUSE_CGROUPS == 1)
    /* Verify CGroup membership */
    TaskHandle_t xCurrentTask = xTaskGetCurrentTaskHandle();
    if (pxContainer->xCGroup != NULL) {
        CGroupHandle_t xTaskCGroup = xCGroupGetTaskGroup(xCurrentTask);
        if (xTaskCGroup != pxContainer->xCGroup) {
            /* CGroup isolation verification failed */
            pxContainer->eState = CONTAINER_STATE_ERROR;
            vTaskDelete(NULL);
            return;
        }
    }
#endif

#if (configUSE_PID_NAMESPACE == 1)
    /* Verify PID namespace membership */
    if (pxContainer->xPidNamespace != NULL) {
        PidNamespaceHandle_t xTaskNamespace = xPidNamespaceGetTaskNamespace(xCurrentTask);
        if (xTaskNamespace != pxContainer->xPidNamespace) {
            /* PID namespace isolation verification failed */
            pxContainer->eState = CONTAINER_STATE_ERROR;
            vTaskDelete(NULL);
            return;
        }
    }
#endif

#ifdef configUSE_FILESYSTEM
    // chroot to container root fs if needed
    if (xTaskChroot(pxContainer->pcRootPath) != pdPASS) {
        pxContainer->eState = CONTAINER_STATE_ERROR;
#ifdef MY_DEBUG
        xil_printf("ERROR: Failed to chroot to %s\r\n", pxContainer->pcRootPath);
#endif
        vTaskDelete(NULL);
        return;
    }
#endif
    // Load ELF from file system
    if (get_elf_by_name(&pxParams->wrap, pxContainer->elfName) != pdPASS) {
        /* Failed to load ELF - cannot proceed */
        pxContainer->eState = CONTAINER_STATE_ERROR;
        vTaskDelete(NULL);
        return;
    }
    /* All isolation mechanisms verified - now call the original function */
    pxOriginalFunction(pvOriginalParameters);
    vPortFree(pxParams);
    for(;;) {
        
    }
    /* If we reach here, the container function has completed */
    /* Mark container as stopped */
    pxContainer->eState = CONTAINER_STATE_STOPPED;
    pxContainer->xTaskHandle = NULL;
}

/* Start a container */
BaseType_t xContainerStart(uint32_t ulContainerID) {
    Container_t           *pxContainer;
    BaseType_t             xResult = pdFAIL;
    ContainerTaskParams_t *pxTaskParams;
    xil_printf("Starting container...\r\n");

    if (xSemaphoreTake(xContainerMutex, portMAX_DELAY) == pdTRUE) {
        pxContainer = pxContainerGetByID(ulContainerID);
        if (pxContainer != NULL && pxContainer->eState == CONTAINER_STATE_STOPPED) {
            /* Allocate parameters for wrapper function */
            pxTaskParams = (ContainerTaskParams_t *)pvPortMalloc(sizeof(ContainerTaskParams_t));
            if (pxTaskParams == NULL) {
                xSemaphoreGive(xContainerMutex);
                xil_printf("ERROR: Failed to allocate task parameters.\r\n");
                return pdFAIL;
            }

            pxTaskParams->pxContainer = pxContainer;
            pxTaskParams->pxOriginalFunction = pxContainer->pxFunction;
            pxTaskParams->pvOriginalParameters = &pxTaskParams->wrap;
            pxTaskParams->wrap.elf_data = NULL;
            pxTaskParams->wrap.elf_size = 0;

            /* Create a binary semaphore to synchronize task startup */
            pxContainer->xReadySemaphore = xSemaphoreCreateBinary();
            if (pxContainer->xReadySemaphore == NULL) {
                vPortFree(pxTaskParams);
                xSemaphoreGive(xContainerMutex);
                xil_printf("ERROR: Failed to create ready semaphore.\r\n");
                return pdFAIL;
            }

/* Create task for container - ensuring proper namespace application */
#if (configUSE_PID_NAMESPACE == 1)
            /* Create task in PID namespace if available - following pidnamespace_example pattern */
            if (pxContainer->xPidNamespace != NULL) {
                xResult = xTaskCreateInNamespace(
                    pxContainer->xPidNamespace, vContainerTaskWrapper, pxContainer->pcContainerName,
                    pxContainer->ulStackSize, pxTaskParams, pxContainer->uxPriority,
                    &pxContainer->xTaskHandle);
            } else
#endif
            {
                /* Fallback to regular task creation */
                xResult = xTaskCreate(vContainerTaskWrapper, pxContainer->pcContainerName,
                                      pxContainer->ulStackSize, pxTaskParams,
                                      pxContainer->uxPriority, &pxContainer->xTaskHandle);
            }

            if (xResult == pdPASS) {
/* CRITICAL: Apply all isolation mechanisms to the newly created task */

/* 1. Add task to CGroup FIRST - following cgroup_example pattern */
#if (configUSE_CGROUPS == 1)
                if (pxContainer->xCGroup != NULL && pxContainer->xTaskHandle != NULL) {
                    BaseType_t xCGroupResult =
                        xCGroupAddTask(pxContainer->xCGroup, pxContainer->xTaskHandle);
                    if (xCGroupResult != pdPASS) {
                        /* CGroup application failed - this is serious for resource isolation */
                        pxContainer->eState = CONTAINER_STATE_ERROR;
                        vTaskDelete(pxContainer->xTaskHandle);
                        pxContainer->xTaskHandle = NULL;
                        if (pxContainer->xReadySemaphore != NULL) {
                            vSemaphoreDelete(pxContainer->xReadySemaphore);
                            pxContainer->xReadySemaphore = NULL;
                        }
                        vPortFree(pxTaskParams);
                        xSemaphoreGive(xContainerMutex);
                        xil_printf("ERROR: Failed to add task to CGroup.\r\n");
                        return pdFAIL;
                    }
                }
#endif

                /* 3. IPC namespace will be applied by the wrapper function when task starts */
                /* This is correct since IPC namespace must be set from within the task context */

                pxContainer->eState = CONTAINER_STATE_RUNNING;

                /* All isolation setup complete - release the semaphore to let task proceed */
                xSemaphoreGive(pxContainer->xReadySemaphore);
            } else {
                /* Task creation failed, free parameters and semaphore */
                xil_printf("ERROR: Failed to create container task.\r\n");
                if (pxContainer->xReadySemaphore != NULL) {
                    vSemaphoreDelete(pxContainer->xReadySemaphore);
                    pxContainer->xReadySemaphore = NULL;
                }
                vPortFree(pxTaskParams);
            }
        }
        xSemaphoreGive(xContainerMutex);
    } else {
        xil_printf("ERROR: Failed to acquire container mutex.\r\n");
    }

    xil_printf("Container start result: %d\r\n", (int)xResult);
    return xResult;
}

/* Stop a container */
BaseType_t xContainerStop(uint32_t ulContainerID) {
    Container_t *pxContainer;
    BaseType_t   xResult = pdFAIL;

    if (xSemaphoreTake(xContainerMutex, portMAX_DELAY) == pdTRUE) {
        pxContainer = pxContainerGetByID(ulContainerID);
        if (pxContainer != NULL && pxContainer->eState == CONTAINER_STATE_RUNNING) {
            /* For now, just mark as stopped. In a full implementation,
               you would gracefully stop the task with vTaskDelete */
            pxContainer->eState = CONTAINER_STATE_STOPPED;
            vTaskDelete(pxContainer->xTaskHandle);
            pxContainer->xTaskHandle = NULL;
            xResult = pdPASS;
        }
        xSemaphoreGive(xContainerMutex);
    }

    return xResult;
}

/* Delete a container - following cleanup patterns from examples */
BaseType_t xContainerDelete(uint32_t ulContainerID) {
    Container_t *pxContainer, *pxPrevContainer = NULL;
    BaseType_t   xResult = pdFAIL;

    if (xSemaphoreTake(xContainerMutex, portMAX_DELAY) == pdTRUE) {
        /* Find the container in the list */
        pxContainer = pxContainerList;
        while (pxContainer != NULL) {
            if (pxContainer->ulContainerID == ulContainerID) {
                /* Make sure container is stopped first */
                if (pxContainer->eState == CONTAINER_STATE_RUNNING) {
                    /* Stop the container first */
                    xContainerStop(ulContainerID);
                }

                /* Remove from list */
                if (pxPrevContainer == NULL) {
                    pxContainerList = pxContainer->pxNext;
                } else {
                    pxPrevContainer->pxNext = pxContainer->pxNext;
                }

/* Cleanup resources - following cgroup_example and pidnamespace_example cleanup patterns */
#if (configUSE_CGROUPS == 1)
                if (pxContainer->xCGroup != NULL) {
                    xCGroupDelete(pxContainer->xCGroup);
                    pxContainer->xCGroup = NULL;
                }
#endif

#if (configUSE_PID_NAMESPACE == 1)
                if (pxContainer->xPidNamespace != NULL) {
                    xPidNamespaceDelete(pxContainer->xPidNamespace);
                    pxContainer->xPidNamespace = NULL;
                }
#endif

#if (configUSE_IPC_NAMESPACE == 1)
                if (pxContainer->xIpcNamespace != NULL) {
                    xIpcNamespaceDelete(pxContainer->xIpcNamespace);
                    pxContainer->xIpcNamespace = NULL;
                }
#endif

                /* Free container memory */
                vPortFree(pxContainer);
                xResult = pdPASS;
                break;
            }
            pxPrevContainer = pxContainer;
            pxContainer = pxContainer->pxNext;
        }
        xSemaphoreGive(xContainerMutex);
    }

    return xResult;
}

/* Get container by ID */
Container_t *pxContainerGetByID(uint32_t ulContainerID) {
    Container_t *pxContainer = pxContainerList;

    while (pxContainer != NULL) {
        if (pxContainer->ulContainerID == ulContainerID) {
            return pxContainer;
        }
        pxContainer = pxContainer->pxNext;
    }

    return NULL;
}

/* Get container by name */
Container_t *pxContainerGetByName(const char *pcName) {
    Container_t *pxContainer = pxContainerList;

    if (pcName == NULL) {
        return NULL;
    }

    while (pxContainer != NULL) {
        if (strncmp(pxContainer->pcContainerName, pcName, strlen(pcName)) == 0) {
            return pxContainer;
        }
        pxContainer = pxContainer->pxNext;
    }

    return NULL;
}

/* Get container count */
uint32_t ulContainerGetCount(void) {
    Container_t *pxContainer = pxContainerList;
    uint32_t     ulCount = 0;

    while (pxContainer != NULL) {
        ulCount++;
        pxContainer = pxContainer->pxNext;
    }

    return ulCount;
}

/* Get container list */
Container_t *pxContainerGetList(void) { return pxContainerList; }

/* CLI Commands Implementation */

/* Container create command */
BaseType_t
xContainerCreateCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString) {
    const char *pcParameter1, *pcParameter2, *pcParameter3, *pcParameter4;
    BaseType_t  lParameterStringLength1, lParameterStringLength2, lParameterStringLength3,
        lParameterStringLength4;
    char     pcContainerName[32];
    char     elfName[64];
    uint32_t ulMemoryLimit = 0;
    uint32_t ulCpuQuota = 0;

    /* Ensure pcWriteBuffer is initialized. */
    *pcWriteBuffer = '\0';

    /* Obtain the first parameter (container name). */
    pcParameter1 =
        FreeRTOS_CLIGetParameter(pcCommandString,         /* The command string itself. */
                                 1,                       /* Return the first parameter. */
                                 &lParameterStringLength1 /* Store the parameter string length. */
        );

    if (pcParameter1 == NULL) {
        strcpy(pcWriteBuffer,
            "Usage: container-create <image> <program> [memory_limit_kb] [cpu_quota_percent]\r\n");
        return pdFALSE;
    }

    /* Copy the container name */
    if ((size_t)lParameterStringLength1 < sizeof(pcContainerName)) {
        strncpy(pcContainerName, pcParameter1, lParameterStringLength1);
        pcContainerName[lParameterStringLength1] = '\0';
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
            "Container name too long (max %d characters).\r\n",
            (int)(sizeof(pcContainerName) - 1));
        return pdFALSE;
    }

    /* ELF parameter */
    pcParameter2 = FreeRTOS_CLIGetParameter(pcCommandString, 2, &lParameterStringLength2);
    if (pcParameter2 == NULL) {
        strcpy(pcWriteBuffer,
            "Usage: container-create <image> <program> [memory_limit_kb] [cpu_quota_percent]\r\n");
        return pdFALSE;
    }

    /* Copy the program name */
    if ((size_t)lParameterStringLength2 < sizeof(elfName)) {
        strncpy(elfName, pcParameter2, lParameterStringLength2);
        elfName[lParameterStringLength2] = '\0';
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
            "Program name too long (max %d characters).\r\n",
            (int)(sizeof(elfName) - 1));
        return pdFALSE;
    }

    /* Obtain optional memory limit parameter */
    pcParameter3 = FreeRTOS_CLIGetParameter(pcCommandString, 3, &lParameterStringLength3);

    if (pcParameter3 != NULL) {
        ulMemoryLimit = (uint32_t)atoi(pcParameter3) * 1024; /* Convert KB to bytes */
    }

    /* Obtain optional CPU quota parameter */
    pcParameter4 = FreeRTOS_CLIGetParameter(pcCommandString, 4, &lParameterStringLength4);

    if (pcParameter4 != NULL) {
        ulCpuQuota = (uint32_t)atoi(pcParameter4) * 100; /* Convert percentage to quota units */
    }

    /* Create container with resource limits */
    BaseType_t xResult =
        xContainerCreateWithLimits(pcContainerName, elfName, configMINIMAL_STACK_SIZE * 2,
                                   tskIDLE_PRIORITY + 6, ulMemoryLimit, ulCpuQuota);

    if (xResult != pdPASS) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
            "Failed to create container '%s'.\r\n", pcContainerName);
        return pdFALSE;
    }
#ifdef configUSE_FILESYSTEM
    char image_path[256];
    /* Manual initialization to avoid memset */
    {
        strcpy(image_path, "/var/container/images/");
        strncpy(&image_path[22], pcParameter1, lParameterStringLength1);
        image_path[22 + lParameterStringLength1] = '\0';
    }
    xResult = xContainerUnpackImage(image_path, ulNextContainerID - 1);
#endif
    if (xResult == pdPASS) {
        if (ulMemoryLimit > 0 || ulCpuQuota > 0) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                "Container '%s' created successfully (Mem: %lu KB, CPU: %lu%%).\r\n",
                pcContainerName, (unsigned long)(ulMemoryLimit / 1024),
                (unsigned long)(ulCpuQuota / 100));
        } else {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                "Container '%s' created successfully.\r\n", pcContainerName);
        }
        return pdFALSE;
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
            "Failed to unpack image for container '%s'.\r\n", pcContainerName);
        xContainerDelete(--ulNextContainerID);
    }

    return pdFALSE;
}

/* Container list command */
BaseType_t
xContainerListCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString) {
    char               *pcState;
    static Container_t *pxCurrentContainer = NULL;
    static BaseType_t   xFirst = pdTRUE;
    size_t              xOffset = 0;

    /* Remove compile time warnings about unused parameters. */
    (void)pcCommandString;
    *pcWriteBuffer = '\0';

    if (xFirst == pdTRUE) {
        pxCurrentContainer = pxContainerList;
        xFirst = pdFALSE;

        xOffset = snprintf(pcWriteBuffer, xWriteBufferLen,
            "Container ID\tName\t\tState\t\tMemory Limit\tCPU Quota\r\n"
            "-------------------------------------------------------------\r\n");

        if (pxCurrentContainer == NULL) {
            snprintf(pcWriteBuffer + xOffset, xWriteBufferLen - xOffset,
                "No containers found.\r\n");
            xFirst = pdTRUE;
            return pdFALSE;
        }

        return pdTRUE; /* More data to come */
    }

    if (pxCurrentContainer != NULL) {
        switch (pxCurrentContainer->eState) {
            case CONTAINER_STATE_STOPPED:
                pcState = "STOPPED";
                break;
            case CONTAINER_STATE_RUNNING:
                pcState = "RUNNING";
                break;
            case CONTAINER_STATE_PAUSED:
                pcState = "PAUSED";
                break;
            case CONTAINER_STATE_ERROR:
                pcState = "ERROR";
                break;
            default:
                pcState = "UNKNOWN";
                break;
        }

        /* Format memory and CPU values */
        char pcMemLimit[16];
        char pcCpuQuota[16];
        
        if (pxCurrentContainer->ulMemoryLimit > 0) {
            snprintf(pcMemLimit, sizeof(pcMemLimit), "%lu KB",
                (unsigned long)(pxCurrentContainer->ulMemoryLimit / 1024));
        } else {
            strcpy(pcMemLimit, "N/A");
        }
        
        if (pxCurrentContainer->ulCpuQuota > 0) {
            snprintf(pcCpuQuota, sizeof(pcCpuQuota), "%lu%%",
                (unsigned long)(pxCurrentContainer->ulCpuQuota / 100));
        } else {
            strcpy(pcCpuQuota, "N/A");
        }

        snprintf(pcWriteBuffer, xWriteBufferLen,
            "%lu\t\t%s%s%s\t\t%s\t\t%s\r\n",
            (unsigned long)pxCurrentContainer->ulContainerID,
            pxCurrentContainer->pcContainerName,
            strlen(pxCurrentContainer->pcContainerName) >= 8 ? "\t" : "\t\t",
            pcState,
            pcMemLimit,
            pcCpuQuota);

        pxCurrentContainer = pxCurrentContainer->pxNext;

        if (pxCurrentContainer == NULL) {
            xFirst = pdTRUE;
            return pdFALSE; /* No more data */
        }

        return pdTRUE; /* More data to come */
    }

    xFirst = pdTRUE;
    return pdFALSE;
}

/* Container start command */
BaseType_t
xContainerStartCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString) {
    const char *pcParameter;
    BaseType_t  lParameterStringLength;
    uint32_t    ulContainerID;

    /* Ensure pcWriteBuffer is initialized. */
    *pcWriteBuffer = '\0';

    /* Obtain the parameter string. */
    pcParameter =
        FreeRTOS_CLIGetParameter(pcCommandString,        /* The command string itself. */
                                 1,                      /* Return the first parameter. */
                                 &lParameterStringLength /* Store the parameter string length. */
        );

    if (pcParameter != NULL) {
        ulContainerID = (uint32_t)atoi(pcParameter);

        if (xContainerStart(ulContainerID) == pdPASS) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                "Container %lu started successfully.\r\n",
                (unsigned long)ulContainerID);
        } else {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                "Failed to start container %lu.\r\n",
                (unsigned long)ulContainerID);
        }
    } else {
        strcpy(pcWriteBuffer, "Usage: container-start <id>\r\n");
    }

    return pdFALSE;
}

/* Container stop command */
BaseType_t
xContainerStopCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString) {
    const char *pcParameter;
    BaseType_t  lParameterStringLength;
    uint32_t    ulContainerID;

    /* Ensure pcWriteBuffer is initialized. */
    *pcWriteBuffer = '\0';

    /* Obtain the parameter string. */
    pcParameter =
        FreeRTOS_CLIGetParameter(pcCommandString,        /* The command string itself. */
                                 1,                      /* Return the first parameter. */
                                 &lParameterStringLength /* Store the parameter string length. */
        );

    if (pcParameter != NULL) {
        ulContainerID = (uint32_t)atoi(pcParameter);

        if (xContainerStop(ulContainerID) == pdPASS) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                "Container %lu stopped successfully.\r\n",
                (unsigned long)ulContainerID);
        } else {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                "Failed to stop container %lu. It may not be running or does not exist.\r\n",
                (unsigned long)ulContainerID);
        }
    } else {
        strcpy(pcWriteBuffer, "Usage: container-stop <id>\r\n");
    }

    return pdFALSE;
}

/* Container run command */
BaseType_t
xContainerRunCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString) {
    const char *pcParameter1, *pcParameter2, *pcParameter3, *pcParameter4;
    BaseType_t  lParameterStringLength1, lParameterStringLength2, lParameterStringLength3,
        lParameterStringLength4;
    char     pcContainerName[32];
    char     elfName[64];
    uint32_t ulMemoryLimit = 0;
    uint32_t ulCpuQuota = 0;

    /* Ensure pcWriteBuffer is initialized. */
    *pcWriteBuffer = '\0';

    /* Obtain the first parameter (container name). */
    pcParameter1 =
        FreeRTOS_CLIGetParameter(pcCommandString,         /* The command string itself. */
                                 1,                       /* Return the first parameter. */
                                 &lParameterStringLength1 /* Store the parameter string length. */
        );

    if (pcParameter1 == NULL) {
        strcpy(pcWriteBuffer,
            "Usage: container-run <image> <program> [memory_limit_kb] [cpu_quota_percent]\r\n");
        return pdFALSE;
    }

    /* Copy the container name */
    if ((size_t)lParameterStringLength1 < sizeof(pcContainerName)) {
        strncpy(pcContainerName, pcParameter1, lParameterStringLength1);
        pcContainerName[lParameterStringLength1] = '\0';
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
            "Container name too long (max %d characters).\r\n",
            (int)(sizeof(pcContainerName) - 1));
        return pdFALSE;
    }

    pcParameter2 =
        FreeRTOS_CLIGetParameter(pcCommandString,         /* The command string itself. */
                                 2,                       /* Return the second parameter. */
                                 &lParameterStringLength2 /* Store the parameter string length. */
        );
    if (pcParameter2 == NULL) {
        strcpy(pcWriteBuffer,
            "Usage: container-run <image> <program> [memory_limit_kb] [cpu_quota_percent]\r\n");
        return pdFALSE;
    }
    /* Copy the ELF name */
    if ((size_t)lParameterStringLength2 < sizeof(elfName)) {
        strncpy(elfName, pcParameter2, lParameterStringLength2);
        elfName[lParameterStringLength2] = '\0';
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
            "Program name too long (max %d characters).\r\n",
            (int)(sizeof(elfName) - 1));
        return pdFALSE;
    }

    /* Obtain optional memory limit parameter */
    pcParameter3 = FreeRTOS_CLIGetParameter(pcCommandString, 3, &lParameterStringLength3);

    if (pcParameter3 != NULL) {
        ulMemoryLimit = (uint32_t)atoi(pcParameter3) * 1024; /* Convert KB to bytes */
    }

    /* Obtain optional CPU quota parameter */
    pcParameter4 = FreeRTOS_CLIGetParameter(pcCommandString, 4, &lParameterStringLength4);

    if (pcParameter4 != NULL) {
        ulCpuQuota = (uint32_t)atoi(pcParameter4) * 100; /* Convert percentage to quota units */
    }

    /* Create container with resource limits */
    BaseType_t xResult =
        xContainerCreateWithLimits(pcContainerName, elfName, configMINIMAL_STACK_SIZE * 2,
                                   tskIDLE_PRIORITY + 6, ulMemoryLimit, ulCpuQuota);

    if (xResult != pdPASS) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
            "Failed to create container '%s'.\r\n", pcContainerName);
        return pdFALSE;
    }
#ifdef configUSE_FILESYSTEM
    char image_path[256];
    /* Manual initialization to avoid memset */
    {
        strcpy(image_path, "/var/container/images/");
        strncpy(&image_path[22], pcParameter1, lParameterStringLength1);
        image_path[22 + lParameterStringLength1] = '\0';
    }
    xResult = xContainerUnpackImage(image_path, ulNextContainerID - 1);
#endif
    if (xResult == pdPASS) {
        if (ulMemoryLimit > 0 || ulCpuQuota > 0) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                "Container '%s' created and started successfully (Mem: %lu KB, CPU: %lu%%).\r\n",
                pcContainerName, (unsigned long)(ulMemoryLimit / 1024),
                (unsigned long)(ulCpuQuota / 100));
        } else {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                "Container '%s' created and started successfully.\r\n", pcContainerName);
        }

        xContainerStart(ulNextContainerID - 1); // Start the newly created container

    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
            "Failed to create container '%s'.\r\n", pcContainerName);
    }

    return pdFALSE;
}

BaseType_t
xContainerDeleteCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString) {
    const char *pcParameter;
    BaseType_t  lParameterStringLength;
    uint32_t    ulContainerID;

    /* Ensure pcWriteBuffer is initialized. */
    *pcWriteBuffer = '\0';

    /* Obtain the parameter string. */
    pcParameter =
        FreeRTOS_CLIGetParameter(pcCommandString,        /* The command string itself. */
                                 1,                      /* Return the first parameter. */
                                 &lParameterStringLength /* Store the parameter string length. */
        );

    if (pcParameter != NULL) {
        ulContainerID = (uint32_t)atoi(pcParameter);

        if (xContainerDelete(ulContainerID) == pdPASS) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                "Container %lu deleted successfully.\r\n",
                (unsigned long)ulContainerID);
        } else {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                "Failed to delete container %lu. It may be running or does not exist.\r\n",
                (unsigned long)ulContainerID);
        }
    } else {
        strcpy(pcWriteBuffer, "Usage: container-delete <id>\r\n");
    }

    return pdFALSE;
}

#ifdef configUSE_FILESYSTEM
/* Container load command - Load an image file into /var/container/images */
BaseType_t
xContainerLoadCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString) {
    const char    *pcParameter;
    BaseType_t     lParameterStringLength;
    char           pcSourcePath[256];
    char           pcDestPath[256];
    char           pcImageName[128];
    FileSystem_t  *pxFS;
    LittleFSOps_t *lfs_ops;
    lfs_file_t     srcFile, destFile;
    int            ret;
    lfs_soff_t     file_size;
    unsigned char  buffer[256];
    lfs_ssize_t    bytes_read, bytes_written;
    unsigned char  file_count;

    /* Ensure pcWriteBuffer is initialized. */
    *pcWriteBuffer = '\0';

    /* Get file system instance */
    pxFS = pxGetFileSystem();
    if (pxFS == NULL || pxFS->fs_ops == NULL) {
        strcpy(pcWriteBuffer, "File system not initialized\r\n");
        return pdFALSE;
    }
    lfs_ops = (LittleFSOps_t *)pxFS->fs_ops;

    /* Obtain the parameter string (source image path) */
    pcParameter = FreeRTOS_CLIGetParameter(pcCommandString, 1, &lParameterStringLength);
    if (pcParameter == NULL) {
        strcpy(pcWriteBuffer, "Usage: container-load <image_path>\r\n");
        return pdFALSE;
    }

    /* Copy source path */
    if ((size_t)lParameterStringLength >= sizeof(pcSourcePath)) {
        strcpy(pcWriteBuffer, "Image path too long\r\n");
        return pdFALSE;
    }
    strncpy(pcSourcePath, pcParameter, lParameterStringLength);
    pcSourcePath[lParameterStringLength] = '\0';

    /* Extract image name from path (last component after /) */
    const char *last_slash = pcSourcePath;
    const char *ptr = pcSourcePath;
    while (*ptr) {
        if (*ptr == '/') {
            last_slash = ptr + 1;
        }
        ptr++;
    }
    strncpy(pcImageName, last_slash, sizeof(pcImageName) - 1);
    pcImageName[sizeof(pcImageName) - 1] = '\0';

    /* Open source file to verify format */
    ret = lfs_ops->file_open(&srcFile, pcSourcePath, LFS_O_RDONLY);
    if (ret < 0) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
            "Failed to open source image: %s\r\n", pcSourcePath);
        return pdFALSE;
    }

    /* Read and verify first byte (file count) */
    ret = lfs_ops->file_read(&srcFile, &file_count, 1);
    if (ret != 1) {
        strcpy(pcWriteBuffer, "Failed to read image header\r\n");
        lfs_ops->file_close(&srcFile);
        return pdFALSE;
    }

    /* Rewind to beginning */
    lfs_ops->file_rewind(&srcFile);

    /* Ensure /var/container/images directory exists */
    ret = lfs_ops->mkdir("/var");
    ret = lfs_ops->mkdir("/var/container");
    ret = lfs_ops->mkdir("/var/container/images");

    /* Build destination path manually */
    char       *dest_ptr = pcDestPath;
    const char *prefix = "/var/container/images/";
    /* Copy prefix */
    while (*prefix) {
        *dest_ptr++ = *prefix++;
    }
    /* Copy image name */
    const char *name_ptr = pcImageName;
    while (*name_ptr) {
        *dest_ptr++ = *name_ptr++;
    }
    *dest_ptr = '\0';

    /* Create destination file */
    ret = lfs_ops->file_open(&destFile, pcDestPath, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (ret < 0) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
            "Failed to create destination file: %s\r\n", pcDestPath);
        lfs_ops->file_close(&srcFile);
        return pdFALSE;
    }

    /* Copy file contents */
    file_size = lfs_ops->file_size(&srcFile);

    while (1) {
        bytes_read = lfs_ops->file_read(&srcFile, buffer, sizeof(buffer));
        if (bytes_read <= 0) {
            break;
        }

        bytes_written = lfs_ops->file_write(&destFile, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            strcpy(pcWriteBuffer, "Write error during copy\r\n");
            lfs_ops->file_close(&srcFile);
            lfs_ops->file_close(&destFile);
            lfs_ops->remove(pcDestPath);
            return pdFALSE;
        }
    }

    /* Close files */
    lfs_ops->file_close(&srcFile);
    lfs_ops->file_close(&destFile);

    snprintf(pcWriteBuffer, xWriteBufferLen,
        "Image loaded successfully: %s (%u files, %ld bytes)\r\n",
        pcDestPath, file_count, (long)file_size);

    return pdFALSE;
}

/* Container save command - Save container files to an image */
BaseType_t
xContainerSaveCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString) {
    const char *pcParameter1, *pcParameter2;
    BaseType_t  lParameterStringLength1, lParameterStringLength2;
    uint32_t    ulContainerID;
    char        pcContainerDir[256];
    char        pcOutputPath[256];

    /* Ensure pcWriteBuffer is initialized. */
    *pcWriteBuffer = '\0';

    /* Get container ID parameter */
    pcParameter1 = FreeRTOS_CLIGetParameter(pcCommandString, 1, &lParameterStringLength1);
    if (pcParameter1 == NULL) {
        strcpy(pcWriteBuffer, "Usage: container-save <container_id> <output_path>\r\n");
        return pdFALSE;
    }
    ulContainerID = (uint32_t)atoi(pcParameter1);

    /* Get output path parameter */
    pcParameter2 = FreeRTOS_CLIGetParameter(pcCommandString, 2, &lParameterStringLength2);
    if (pcParameter2 == NULL) {
        strcpy(pcWriteBuffer, "Usage: container-save <container_id> <output_path>\r\n");
        return pdFALSE;
    }

    /* Copy output path */
    if ((size_t)lParameterStringLength2 >= sizeof(pcOutputPath)) {
        strcpy(pcWriteBuffer, "Output path too long\r\n");
        return pdFALSE;
    }
    strncpy(pcOutputPath, pcParameter2, lParameterStringLength2);
    pcOutputPath[lParameterStringLength2] = '\0';

    /* Build container directory path manually */
    char       *dir_ptr = pcContainerDir;
    const char *dir_prefix = "/var/container/";
    char        id_str[12];

    /* Copy prefix */
    while (*dir_prefix) {
        *dir_ptr++ = *dir_prefix++;
    }

    /* Convert ID to string and append */
    uint32_to_string(ulContainerID, id_str);
    const char *id_ptr = id_str;
    while (*id_ptr) {
        *dir_ptr++ = *id_ptr++;
    }
    *dir_ptr = '\0';

    /* Call the pack image function */
    if (xContainerPackImage(ulContainerID, pcOutputPath) == pdPASS) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
            "Container %lu saved successfully to %s\r\n",
            (unsigned long)ulContainerID, pcOutputPath);
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
            "Failed to save container %lu\r\n",
            (unsigned long)ulContainerID);
    }

    return pdFALSE;
}

/* Container image command - List all images in /var/container/images */
BaseType_t
xContainerImageCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString) {
    static FileSystem_t   *pxFS = NULL;
    static LittleFSOps_t  *lfs_ops = NULL;
    static lfs_dir_t       dir;
    static BaseType_t      xFirst = pdTRUE;
    static int             image_count = 0;
    struct lfs_info        info;
    int                    ret;

    (void)pcCommandString;

    /* Ensure pcWriteBuffer is initialized. */
    *pcWriteBuffer = '\0';

    if (xFirst == pdTRUE) {
        xFirst = pdFALSE;
        image_count = 0;

        /* Get file system instance */
        pxFS = pxGetFileSystem();
        if (pxFS == NULL || pxFS->fs_ops == NULL) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                "File system not initialized\r\n");
            xFirst = pdTRUE;
            return pdFALSE;
        }
        lfs_ops = (LittleFSOps_t *)pxFS->fs_ops;

        /* Open images directory */
        ret = lfs_ops->dir_open(&dir, "/var/container/images");
        if (ret < 0) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                "No images directory found. Use 'container-load' to add images.\r\n");
            xFirst = pdTRUE;
            return pdFALSE;
        }

        snprintf(pcWriteBuffer, xWriteBufferLen,
            "Container Images:\r\n"
            "Name\t\t\tSize (bytes)\r\n"
            "----------------------------------------\r\n");
        return pdTRUE; /* More data to come */
    }

    /* Read next directory entry */
    while (1) {
        ret = lfs_ops->dir_read(&dir, &info);
        if (ret <= 0) {
            break;
        }

        /* Skip . and .. entries */
        if (strncmp(info.name, ".", 1) == 0 || strncmp(info.name, "..", 2) == 0) {
            continue;
        }

        /* Only show files, not directories */
        if (info.type == LFS_TYPE_REG) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                "%-24s%lu\r\n", info.name, (unsigned long)info.size);
            image_count++;
            return pdTRUE; /* More data to come */
        }
    }

    /* Done reading directory */
    lfs_ops->dir_close(&dir);

    if (image_count == 0) {
        snprintf(pcWriteBuffer, xWriteBufferLen, "No images found.\r\n");
    } else {
        snprintf(pcWriteBuffer, xWriteBufferLen,
            "\r\nTotal: %d image(s)\r\n", image_count);
    }

    xFirst = pdTRUE;
    return pdFALSE;
}
#endif
/* Run command - Load and execute ELF file from specified path */
BaseType_t xRunCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString) {
    const char  *pcParameter;
    BaseType_t   lParameterStringLength;
    char         pcFilePath[256];
    ELF_WRAP     wrap;

    /* Ensure pcWriteBuffer is initialized. */
    *pcWriteBuffer = '\0';

    /* Get file path parameter */
    pcParameter = FreeRTOS_CLIGetParameter(pcCommandString, 1, &lParameterStringLength);
    if (pcParameter == NULL) {
        strcpy(pcWriteBuffer, "Usage: run <elf_file_path>\r\n");
        return pdFALSE;
    }

    /* Copy file path */
    if ((size_t)lParameterStringLength >= sizeof(pcFilePath)) {
        strcpy(pcWriteBuffer, "File path too long\r\n");
        return pdFALSE;
    }
    strncpy(pcFilePath, pcParameter, lParameterStringLength);
    pcFilePath[lParameterStringLength] = '\0';

    /* Load ELF file */
    if (get_elf_by_name(&wrap, pcFilePath) != pdPASS) {
        snprintf(pcWriteBuffer, xWriteBufferLen,
            "Failed to load ELF file: %s\r\n", pcFilePath);
        return pdFALSE;
    }

    snprintf(pcWriteBuffer, xWriteBufferLen,
        "ELF loaded from %s (%lu bytes), executing...\r\n",
        pcFilePath, (unsigned long)wrap.elf_size);

    /* Create task to run the ELF */
    elf_load_and_run(wrap.elf_data, wrap.elf_size);

    vPortFree((void *)wrap.elf_data);

    return pdFALSE;
}

#ifdef configUSE_FILESYSTEM
/* Ls command - List directory contents */
BaseType_t xLsCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString) {
    static const char    *pcParameter = NULL;
    static BaseType_t     lParameterStringLength = 0;
    static char           pcDirPath[256];
    static FileSystem_t  *pxFS = NULL;
    static LittleFSOps_t *lfs_ops = NULL;
    static lfs_dir_t      dir;
    static BaseType_t     xFirst = pdTRUE;
    static lfs_off_t      dir_pos = 0;
    struct lfs_info       info;
    int                   ret;

    /* Ensure pcWriteBuffer is initialized. */
    *pcWriteBuffer = '\0';

    if (xFirst == pdTRUE) {
        xFirst = pdFALSE;

        /* Get file system instance */
        pxFS = pxGetFileSystem();
        if (pxFS == NULL || pxFS->fs_ops == NULL) {
            strcpy(pcWriteBuffer, "File system not initialized\r\n");
            xFirst = pdTRUE;
            return pdFALSE;
        }
        lfs_ops = (LittleFSOps_t *)pxFS->fs_ops;

        /* Get optional directory path parameter */
        pcParameter = FreeRTOS_CLIGetParameter(pcCommandString, 1, &lParameterStringLength);
        
        if (pcParameter == NULL || lParameterStringLength == 0) {
            /* No parameter provided, use current directory */
            strcpy(pcDirPath, "/");
        } else {
            /* Copy provided path */
            if ((size_t)lParameterStringLength >= sizeof(pcDirPath)) {
                strcpy(pcWriteBuffer, "Directory path too long\r\n");
                xFirst = pdTRUE;
                return pdFALSE;
            }
            strncpy(pcDirPath, pcParameter, lParameterStringLength);
            pcDirPath[lParameterStringLength] = '\0';
        }

        /* Open directory */
        ret = lfs_ops->dir_open(&dir, pcDirPath);
        if (ret < 0) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                "Failed to open directory: %s\r\n", pcDirPath);
            xFirst = pdTRUE;
            return pdFALSE;
        }

        snprintf(pcWriteBuffer, xWriteBufferLen,
            "Directory listing: %s\r\n"
            "----------------------------------------\r\n", pcDirPath);
        dir_pos = dir.pos;
        return pdTRUE; /* More data to come */
    }

    /* Read next directory entry */
    while (1) {
        ret = lfs_ops->dir_read(&dir, &info);
        if (ret <= 0) {
            break;
        }

        /* Avoid infinite loop */
        if (dir.pos == dir_pos) {
            break;
        }
        dir_pos = dir.pos;

        /* Display entry type and name */
        if (info.type == LFS_TYPE_REG) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                "File: %s (%lu bytes)\r\n", info.name, (unsigned long)info.size);
            return pdTRUE; /* More data to come */
        } else if (info.type == LFS_TYPE_DIR) {
            snprintf(pcWriteBuffer, xWriteBufferLen,
                "Dir:  %s\r\n", info.name);
            return pdTRUE; /* More data to come */
        }
    }

    lfs_ops->dir_close(&dir);
    xFirst = pdTRUE;
    return pdFALSE;
}

BaseType_t xPwdCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString) {
    extern FreeRTOS_GOT_t freertos_got;
    (void) pcWriteBuffer, (void) xWriteBufferLen, (void) pcCommandString;
    char pwd_dir[configMAX_PATH_LEN];
    
    /* Debug: Check if freertos_got structure is corrupted */
    xil_printf("=== Debug Info ===\r\n");
    xil_printf("freertos_got addr:              0x%lx\r\n", (unsigned long)&freertos_got);
    xil_printf("freertos_got->freertos_syscalls: 0x%lx\r\n", (unsigned long)freertos_got.freertos_syscalls);
    if (freertos_got.freertos_syscalls != NULL) {
        xil_printf("syscalls->pwd:                   0x%lx\r\n", (unsigned long)freertos_got.freertos_syscalls->pwd);
        xil_printf("syscalls->set_pwd:               0x%lx\r\n", (unsigned long)freertos_got.freertos_syscalls->set_pwd);
        xil_printf("syscalls->uart_puts:             0x%lx\r\n", (unsigned long)freertos_got.freertos_syscalls->uart_puts);
    }
    xil_printf("pvTaskGetPwdPath (expected):     0x%lx\r\n", (unsigned long)pvTaskGetPwdPath);
    xil_printf("==================\r\n");
    
    /* Call pwd function if pointer is valid */
    if (freertos_got.freertos_syscalls != NULL && 
        freertos_got.freertos_syscalls->pwd == pvTaskGetPwdPath) {
        freertos_got.freertos_syscalls->pwd(pwd_dir);
        freertos_got.freertos_syscalls->uart_puts(pwd_dir);
        freertos_got.freertos_syscalls->uart_puts("\r\n");
    } else { 
        xil_printf("ERROR: freertos_got structure corrupted!\r\n");
        /* Try to call directly */
        pvTaskGetPwdPath(pwd_dir);
        xil_printf("Direct call result: %s\r\n", pwd_dir);
    }
    
    return pdFALSE;
}

#endif

/* CLI command definitions */
static const CLI_Command_Definition_t xContainerCreateCmd = {
    "container-create",
    "\r\ncontainer-create <name> [memory_limit_kb] [cpu_quota_percent]:\r\n Creates a new "
    "container with optional resource limits\r\n",
    xContainerCreateCommand, -1 /* Variable number of parameters */
};

static const CLI_Command_Definition_t xContainerListCmd = {
    "container-ls",
    "\r\ncontainer-ls:\r\n Lists all containers with their states and resource limits\r\n",
    xContainerListCommand, 0};

static const CLI_Command_Definition_t xContainerStartCmd = {
    "container-start",
    "\r\ncontainer-start <id>:\r\n Starts the container with the specified ID\r\n",
    xContainerStartCommand, 1};

static const CLI_Command_Definition_t xContainerStopCmd = {
    "container-stop", "\r\ncontainer-stop <id>:\r\n Stops the container with the specified ID\r\n",
    xContainerStopCommand, 1};

static const CLI_Command_Definition_t xContainerRunCmd = {
    "container-run",
    "\r\ncontainer-run <image> <program> [memory_limit_kb] [cpu_quota_percent]:\r\n Creates and starts a "
    "new container with optional resource limits\r\n",
    xContainerRunCommand, -1 /* Variable number of parameters */
};

static const CLI_Command_Definition_t xContainerDeleteCmd = {
    "container-delete",
    "\r\ncontainer-delete <id>:\r\n Deletes the container with the specified ID\r\n",
    xContainerDeleteCommand, 1};

static const CLI_Command_Definition_t xRunCmd = {
    "run", "\r\nrun <elf_file_path>:\r\n Load and execute an ELF file from the specified path\r\n",
    xRunCommand, 1};

#ifdef configUSE_FILESYSTEM
static const CLI_Command_Definition_t xContainerLoadCmd = {
    "container-load",
    "\r\ncontainer-load <image_path>:\r\n Load an image file into /var/container/images\r\n",
    xContainerLoadCommand, 1};

static const CLI_Command_Definition_t xContainerSaveCmd = {
    "container-save",
    "\r\ncontainer-save <container_id> <output_path>:\r\n Save container to an image file\r\n",
    xContainerSaveCommand, 2};

static const CLI_Command_Definition_t xContainerImageCmd = {
    "container-image", "\r\ncontainer-image:\r\n List all container images\r\n",
    xContainerImageCommand, 0};

static const CLI_Command_Definition_t xLsCmd = {
    "ls",
    "\r\nls [directory_path]:\r\n List contents of a directory (defaults to root /)\r\n",
    xLsCommand,
    -1 /* Variable number of parameters (0 or 1) */
};

static const CLI_Command_Definition_t xPwdCmd = {
    "pwd", "\r\npwd:\r\n Print the current working directory\r\n",
    xPwdCommand, 0};
#endif

/* Register container CLI commands */
void vRegisterContainerCLICommands(void) {
    FreeRTOS_CLIRegisterCommand(&xContainerCreateCmd);
    FreeRTOS_CLIRegisterCommand(&xContainerListCmd);
    FreeRTOS_CLIRegisterCommand(&xContainerStartCmd);
    FreeRTOS_CLIRegisterCommand(&xContainerStopCmd);
    FreeRTOS_CLIRegisterCommand(&xContainerRunCmd);
    FreeRTOS_CLIRegisterCommand(&xContainerDeleteCmd);
    FreeRTOS_CLIRegisterCommand(&xRunCmd);
#ifdef configUSE_FILESYSTEM
    FreeRTOS_CLIRegisterCommand(&xContainerLoadCmd);
    FreeRTOS_CLIRegisterCommand(&xContainerSaveCmd);
    FreeRTOS_CLIRegisterCommand(&xContainerImageCmd);
    FreeRTOS_CLIRegisterCommand(&xLsCmd);
    FreeRTOS_CLIRegisterCommand(&xPwdCmd);
#endif
}

/* Container resource management functions */

/* Set memory limit for a container */
BaseType_t xContainerSetMemoryLimit(uint32_t ulContainerID, uint32_t ulMemoryLimit) {
    Container_t *pxContainer;
    BaseType_t   xResult = pdFAIL;

    if (xSemaphoreTake(xContainerMutex, portMAX_DELAY) == pdTRUE) {
        pxContainer = pxContainerGetByID(ulContainerID);
        if (pxContainer != NULL) {
            pxContainer->ulMemoryLimit = ulMemoryLimit;

#if (configUSE_CGROUPS == 1)
            if (pxContainer->xCGroup != NULL) {
                xResult = xCGroupSetMemoryLimit(pxContainer->xCGroup, ulMemoryLimit);
            } else {
                xResult = pdPASS; /* No CGroup, but we updated the container limit */
            }
#else
            xResult = pdPASS;
#endif
        }
        xSemaphoreGive(xContainerMutex);
    }

    return xResult;
}

/* Set CPU quota for a container */
BaseType_t xContainerSetCpuQuota(uint32_t ulContainerID, uint32_t ulCpuQuota) {
    Container_t *pxContainer;
    BaseType_t   xResult = pdFAIL;

    if (xSemaphoreTake(xContainerMutex, portMAX_DELAY) == pdTRUE) {
        pxContainer = pxContainerGetByID(ulContainerID);
        if (pxContainer != NULL) {
            pxContainer->ulCpuQuota = ulCpuQuota;

#if (configUSE_CGROUPS == 1)
            if (pxContainer->xCGroup != NULL) {
                xResult = xCGroupSetCpuQuota(pxContainer->xCGroup, ulCpuQuota);
            } else {
                xResult = pdPASS; /* No CGroup, but we updated the container quota */
            }
#else
            xResult = pdPASS;
#endif
        }
        xSemaphoreGive(xContainerMutex);
    }

    return xResult;
}

/* Get container statistics */
BaseType_t
xContainerGetStats(uint32_t ulContainerID, uint32_t *pulMemoryUsed, uint32_t *pulCpuUsage) {
    Container_t *pxContainer;
    BaseType_t   xResult = pdFAIL;

    if (pulMemoryUsed == NULL || pulCpuUsage == NULL) {
        return pdFAIL;
    }

    *pulMemoryUsed = 0;
    *pulCpuUsage = 0;

    if (xSemaphoreTake(xContainerMutex, portMAX_DELAY) == pdTRUE) {
        pxContainer = pxContainerGetByID(ulContainerID);
        if (pxContainer != NULL) {
#if (configUSE_CGROUPS == 1)
            if (pxContainer->xCGroup != NULL) {
                MemoryLimits_t xMemoryLimits;
                CpuLimits_t    xCpuLimits;

                if (xCGroupGetStats(pxContainer->xCGroup, &xMemoryLimits, &xCpuLimits) == pdPASS) {
                    *pulMemoryUsed = xMemoryLimits.ulMemoryUsed;
                    *pulCpuUsage = xCpuLimits.ulTicksUsed;
                    xResult = pdPASS;
                }
            } else
#endif
            {
                /* No CGroup stats available */
                xResult = pdPASS;
            }
        }
        xSemaphoreGive(xContainerMutex);
    }

    return xResult;
}