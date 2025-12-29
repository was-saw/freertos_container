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

#ifndef INC_PID_NAMESPACE_H
#define INC_PID_NAMESPACE_H

#include "FreeRTOS.h"
#ifndef INC_FREERTOS_H
    #error "include FreeRTOS.h must appear in source files before include pid_namespace.h"
#endif

#include "list.h"
#include "task.h"

/* Forward declarations */
#ifndef INC_TASK_H
struct tskTaskControlBlock;
typedef struct tskTaskControlBlock *TaskHandle_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------
 * MACROS AND DEFINITIONS
 *----------------------------------------------------------*/

/* PID Namespace configuration */
#ifndef configUSE_PID_NAMESPACE
    #define configUSE_PID_NAMESPACE 0
#endif

#ifndef configMAX_PID_NAMESPACES
    #define configMAX_PID_NAMESPACES 4
#endif

#ifndef configMAX_PID_NAMESPACE_NAME_LEN
    #define configMAX_PID_NAMESPACE_NAME_LEN 16
#endif

#ifndef configPID_NAMESPACE_MAX_PID
    #define configPID_NAMESPACE_MAX_PID 10
#endif

/* PID Namespace types */
typedef void *PidNamespaceHandle_t;

/* PID Namespace structure */
typedef struct xPID_NAMESPACE {
    char        pcNamespaceName[configMAX_PID_NAMESPACE_NAME_LEN]; /* Namespace name */
    UBaseType_t ulNamespaceId;                                     /* Unique namespace ID */
    UBaseType_t ulNextPid;                                         /* Next available virtual PID */
    UBaseType_t ulMaxPid;                                          /* Maximum PID value */
    xTaskHandle xTasks[configPID_NAMESPACE_MAX_PID]; /* List of tasks in this namespace */
    UBaseType_t uxTaskCount;                         /* Number of tasks in namespace */
    BaseType_t  xActive;                             /* Namespace active flag */
} PidNamespace_t;

/*-----------------------------------------------------------
 * PID NAMESPACE API FUNCTIONS
 *----------------------------------------------------------*/

#if (configUSE_PID_NAMESPACE == 1)

/**
 * pid_namespace.h
 * @brief Create a new PID namespace
 *
 * @param pcNamespaceName Name of the namespace
 * @return Namespace handle on success, NULL on failure
 */
PidNamespaceHandle_t xPidNamespaceCreate(const char *const pcNamespaceName);

/**
 * pid_namespace.h
 * @brief Delete a PID namespace (must be empty)
 *
 * @param xNamespace Handle to the namespace
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xPidNamespaceDelete(PidNamespaceHandle_t xNamespace);

/**
 * pid_namespace.h
 * @brief Add a task to a PID namespace
 *
 * @param xNamespace Handle to the namespace
 * @param xTask Handle to the task
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xPidNamespaceAddTask(PidNamespaceHandle_t xNamespace, TaskHandle_t xTask);

/**
 * pid_namespace.h
 * @brief Remove a task from a PID namespace
 *
 * @param xNamespace Handle to the namespace
 * @param xTask Handle to the task
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xPidNamespaceRemoveTask(PidNamespaceHandle_t xNamespace, TaskHandle_t xTask);

/**
 * pid_namespace.h
 * @brief Get virtual PID of a task in its namespace
 *
 * @param xTask Handle to the task
 * @return Virtual PID, or 0 if task is not in any namespace
 */
UBaseType_t xPidNamespaceGetTaskVirtualPid(TaskHandle_t xTask);

/**
 * pid_namespace.h
 * @brief Find task by virtual PID within a namespace
 *
 * @param xNamespace Handle to the namespace
 * @param ulVirtualPid Virtual PID to search for
 * @return Task handle if found, NULL otherwise
 */
TaskHandle_t xPidNamespaceFindTaskByVirtualPid(PidNamespaceHandle_t xNamespace,
                                               UBaseType_t          ulVirtualPid);

/**
 * pid_namespace.h
 * @brief Get the namespace of a task
 *
 * @param xTask Handle to the task
 * @return Namespace handle, or NULL if task is not in any namespace
 */
PidNamespaceHandle_t xPidNamespaceGetTaskNamespace(TaskHandle_t xTask);

/**
 * pid_namespace.h
 * @brief Create a task in a specific PID namespace
 *
 * @param xNamespace Target namespace
 * @param pxTaskCode Pointer to the task function
 * @param pcName Descriptive name for the task
 * @param usStackDepth Stack size in words
 * @param pvParameters Parameters for the task
 * @param uxPriority Priority for the task
 * @param pxCreatedTask Handle to store the created task
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xTaskCreateInNamespace(PidNamespaceHandle_t xNamespace,
                                  TaskFunction_t       pxTaskCode,
                                  const char *const    pcName,
                                  const uint16_t       usStackDepth,
                                  void *const          pvParameters,
                                  UBaseType_t          uxPriority,
                                  TaskHandle_t *const  pxCreatedTask);

/**
 * pid_namespace.h
 * @brief Get namespace information
 *
 * @param xNamespace Handle to the namespace
 * @param pulTaskCount Pointer to store task count
 * @param pulNextPid Pointer to store next available PID
 * @param pulMaxPid Pointer to store maximum PID
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xPidNamespaceGetInfo(PidNamespaceHandle_t xNamespace,
                                UBaseType_t         *pulTaskCount,
                                UBaseType_t         *pulNextPid,
                                UBaseType_t         *pulMaxPid);

/**
 * pid_namespace.h
 * @brief Get the root PID namespace
 *
 * @return Root namespace handle
 */
PidNamespaceHandle_t xPidNamespaceGetRoot(void);

/*-----------------------------------------------------------
 * INTERNAL FUNCTIONS
 *----------------------------------------------------------*/

/**
 * @brief Initialize PID namespace system
 * Called during FreeRTOS initialization
 */
void prvPidNamespaceInit(void);

/**
 * @brief Called when a task is deleted
 * @param xTask Handle to the task being deleted
 */
void prvPidNamespaceTaskDelete(TaskHandle_t xTask);

#else /* configUSE_PID_NAMESPACE == 0 */

    /* When PID namespaces are disabled, provide empty macros */
    #define xPidNamespaceCreate(pcNamespaceName) NULL
    #define xPidNamespaceDelete(xNamespace) pdFAIL
    #define xPidNamespaceAddTask(xNamespace, xTask) pdFAIL
    #define xPidNamespaceRemoveTask(xNamespace, xTask) pdFAIL
    #define xPidNamespaceGetTaskVirtualPid(xTask) 0
    #define xPidNamespaceFindTaskByVirtualPid(xNamespace, ulVirtualPid) NULL
    #define xPidNamespaceGetTaskNamespace(xTask) NULL
    #define xTaskCreateInNamespace(xNamespace, pxTaskCode, pcName, usStackDepth, pvParameters,     \
                                   uxPriority, pxCreatedTask)                                      \
        pdFAIL
    #define xPidNamespaceGetInfo(xNamespace, pulTaskCount, pulNextPid, pulMaxPid) pdFAIL

    /* Internal functions - empty when disabled */
    #define prvPidNamespaceInit()                                                                  \
        do {                                                                                       \
        } while (0)
    #define prvPidNamespaceTaskDelete(xTask)                                                       \
        do {                                                                                       \
        } while (0)

#endif /* configUSE_PID_NAMESPACE */

#ifdef __cplusplus
}
#endif

#endif /* INC_PID_NAMESPACE_H */
