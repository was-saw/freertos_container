/*
 * FreeRTOS Kernel V10.0.1
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

#ifndef INC_IPC_NAMESPACE_H
#define INC_IPC_NAMESPACE_H

#ifndef INC_FREERTOS_H
    #error "include FreeRTOS.h must appear in source files before include ipc_namespace.h"
#endif

#include "list.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------
 * MACROS AND DEFINITIONS
 *----------------------------------------------------------*/

/* IPC Namespace configuration */
#ifndef configUSE_IPC_NAMESPACE
    #define configUSE_IPC_NAMESPACE 0
#endif

#ifndef configMAX_IPC_NAMESPACES
    #define configMAX_IPC_NAMESPACES 4
#endif

#ifndef configMAX_IPC_NAMESPACE_NAME_LEN
    #define configMAX_IPC_NAMESPACE_NAME_LEN 16
#endif

#ifndef configMAX_IPC_OBJECTS_PER_NAMESPACE
    #define configMAX_IPC_OBJECTS_PER_NAMESPACE 32
#endif

/* IPC Object Types */
typedef enum {
    IPC_TYPE_QUEUE = 0,
    IPC_TYPE_SEMAPHORE,
    IPC_TYPE_MUTEX,
    IPC_TYPE_EVENT_GROUP
} IpcObjectType_t;

/* IPC Namespace types */
typedef void *IpcNamespaceHandle_t;

/* IPC Object registry entry */
typedef struct xIPC_OBJECT_ENTRY {
    ListItem_t      xNamespaceListItem; /* List item for namespace membership */
    void           *pvIpcObject; /* Pointer to the actual IPC object (queue, semaphore, etc.) */
    IpcObjectType_t xObjectType; /* Type of IPC object */
    char            pcObjectName[configMAX_TASK_NAME_LEN]; /* Object name for debugging */
    UBaseType_t     ulObjectId;                            /* Unique ID within namespace */
    IpcNamespaceHandle_t xNamespace;                       /* Owning namespace */
} IpcObjectEntry_t;

/* IPC Namespace structure */
typedef struct xIPC_NAMESPACE {
    char        pcNamespaceName[configMAX_IPC_NAMESPACE_NAME_LEN]; /* Namespace name */
    UBaseType_t ulNamespaceId;                                     /* Unique namespace ID */
    UBaseType_t ulNextObjectId;                                    /* Next available object ID */
    List_t      xObjectList;   /* List of IPC objects in this namespace */
    UBaseType_t uxObjectCount; /* Number of objects in namespace */
    BaseType_t  xActive;       /* Namespace active flag */
} IpcNamespace_t;

/*-----------------------------------------------------------
 * IPC NAMESPACE API FUNCTIONS
 *----------------------------------------------------------*/

#if (configUSE_IPC_NAMESPACE == 1)

/*-----------------------------------------------------------
 * PRIVATE FUNCTION DECLARATIONS (FOR FreeRTOS KERNEL USE)
 *----------------------------------------------------------*/

/**
 * @brief Called when a task is deleted - internal FreeRTOS use only
 * @param xTask Handle to the task being deleted
 */
void prvIpcNamespaceTaskDelete(TaskHandle_t xTask);

/**
 * @brief Set IPC namespace for a task - internal use
 * @param xTask Task handle
 * @param xNamespace Namespace handle
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t prvIpcNamespaceSetTaskNamespace(TaskHandle_t xTask, IpcNamespaceHandle_t xNamespace);

/*-----------------------------------------------------------
 * PUBLIC API FUNCTIONS
 *----------------------------------------------------------*/

/**
 * ipc_namespace.h
 * @brief Initialize the IPC namespace system
 * Must be called before using any IPC namespace functions
 */
void vIpcNamespaceInit(void);

/**
 * ipc_namespace.h
 * @brief Create a new IPC namespace
 *
 * @param pcNamespaceName Name of the namespace
 * @return Namespace handle on success, NULL on failure
 */
IpcNamespaceHandle_t xIpcNamespaceCreate(const char *const pcNamespaceName);

/**
 * ipc_namespace.h
 * @brief Delete an IPC namespace (must be empty)
 *
 * @param xNamespace Handle to the namespace
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xIpcNamespaceDelete(IpcNamespaceHandle_t xNamespace);

/**
 * ipc_namespace.h
 * @brief Register an IPC object with a namespace
 *
 * @param xNamespace Handle to the namespace
 * @param pvIpcObject Pointer to the IPC object (queue, semaphore, etc.)
 * @param xObjectType Type of the IPC object
 * @param pcObjectName Name of the object (for debugging)
 * @return Object ID on success, 0 on failure
 */
UBaseType_t ulIpcNamespaceRegisterObject(IpcNamespaceHandle_t xNamespace,
                                         void                *pvIpcObject,
                                         IpcObjectType_t      xObjectType,
                                         const char *const    pcObjectName);

/**
 * ipc_namespace.h
 * @brief Unregister an IPC object from a namespace
 *
 * @param xNamespace Handle to the namespace
 * @param pvIpcObject Pointer to the IPC object
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xIpcNamespaceUnregisterObject(IpcNamespaceHandle_t xNamespace, void *pvIpcObject);

/**
 * ipc_namespace.h
 * @brief Find an IPC object by ID within a namespace
 *
 * @param xNamespace Handle to the namespace
 * @param ulObjectId Object ID to find
 * @param pxObjectType Pointer to store the object type (optional, can be NULL)
 * @return Pointer to the IPC object, or NULL if not found
 */
void *pvIpcNamespaceFindObject(IpcNamespaceHandle_t xNamespace,
                               UBaseType_t          ulObjectId,
                               IpcObjectType_t     *pxObjectType);

/**
 * ipc_namespace.h
 * @brief Check if a task has access to an IPC object
 * Only tasks in the same IPC namespace can access the object
 *
 * @param xTask Task handle (use NULL for current task)
 * @param pvIpcObject Pointer to the IPC object
 * @return pdTRUE if access is allowed, pdFALSE otherwise
 */
BaseType_t xIpcNamespaceCheckAccess(TaskHandle_t xTask, void *pvIpcObject);

/**
 * ipc_namespace.h
 * @brief Get the IPC namespace of a task
 *
 * @param xTask Task handle (use NULL for current task)
 * @return Namespace handle, or NULL if task is not in any IPC namespace
 */
IpcNamespaceHandle_t xIpcNamespaceGetTaskNamespace(TaskHandle_t xTask);

/**
 * ipc_namespace.h
 * @brief Set the IPC namespace for a task
 *
 * @param xTask Task handle
 * @param xNamespace Namespace handle
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xIpcNamespaceSetTaskNamespace(TaskHandle_t xTask, IpcNamespaceHandle_t xNamespace);

/**
 * ipc_namespace.h
 * @brief Get namespace information
 *
 * @param xNamespace Handle to the namespace
 * @param puxObjectCount Pointer to store object count (optional)
 * @param puxNextObjectId Pointer to store next object ID (optional)
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xIpcNamespaceGetInfo(IpcNamespaceHandle_t xNamespace,
                                UBaseType_t         *puxObjectCount,
                                UBaseType_t         *puxNextObjectId);

/**
 * ipc_namespace.h
 * @brief Get the root IPC namespace
 * The root namespace is the default namespace for tasks not assigned to any specific namespace
 *
 * @return Root namespace handle
 */
IpcNamespaceHandle_t xIpcNamespaceGetRoot(void);

/*-----------------------------------------------------------
 * ISOLATED IPC API WRAPPERS
 *----------------------------------------------------------*/

/**
 * @brief Create a queue in the current task's IPC namespace
 * Wrapper around xQueueCreate with namespace isolation
 */
QueueHandle_t xQueueCreateIsolated(UBaseType_t       uxQueueLength,
                                   UBaseType_t       uxItemSize,
                                   const char *const pcQueueName);

/**
 * @brief Create a binary semaphore in the current task's IPC namespace
 * Wrapper around xSemaphoreCreateBinary with namespace isolation
 */
SemaphoreHandle_t xSemaphoreCreateBinaryIsolated(const char *const pcSemaphoreName);

/**
 * @brief Create a mutex in the current task's IPC namespace
 * Wrapper around xSemaphoreCreateMutex with namespace isolation
 */
SemaphoreHandle_t xSemaphoreCreateMutexIsolated(const char *const pcMutexName);

#else /* configUSE_IPC_NAMESPACE == 0 */

    /* If IPC namespaces are disabled, provide pass-through macros */
    #define vIpcNamespaceInit()                                                                    \
        do {                                                                                       \
        } while (0)
    #define xIpcNamespaceCreate(name) NULL
    #define xIpcNamespaceDelete(ns) pdFAIL
    #define xIpcNamespaceCheckAccess(task, obj) pdTRUE
    #define xQueueCreateIsolated(length, size, name) xQueueCreate(length, size)
    #define xSemaphoreCreateBinaryIsolated(name) xSemaphoreCreateBinary()
    #define xSemaphoreCreateMutexIsolated(name) xSemaphoreCreateMutex()

#endif /* configUSE_IPC_NAMESPACE */

#ifdef __cplusplus
}
#endif

#endif /* INC_IPC_NAMESPACE_H */
