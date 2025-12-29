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

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#ifndef INC_CGROUP_H
#define INC_CGROUP_H

#ifndef INC_FREERTOS_H
    #error "include FreeRTOS.h must appear in source files before include cgroup.h"
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

/* CGroup configuration */
#ifndef configUSE_CGROUPS
    #define configUSE_CGROUPS 0
#endif

#ifndef configMAX_CGROUPS
    #define configMAX_CGROUPS 8
#endif

#ifndef configMAX_CGROUP_NAME_LEN
    #define configMAX_CGROUP_NAME_LEN 16
#endif

/* CGroup task item for tracking tasks in cgroups */
typedef struct xCGROUP_TASK_ITEM {
    ListItem_t   xCGroupListItem; /* List item for cgroup membership */
    TaskHandle_t xTask;           /* Handle to the task */
} CGroupTaskItem_t;

/* CGroup types */
typedef void *CGroupHandle_t;

/* Memory limits in bytes */
typedef struct xMEMORY_LIMITS {
    UBaseType_t ulMemoryLimit; /* Maximum memory allowed (bytes) */
    UBaseType_t ulMemoryUsed;  /* Current memory usage (bytes) */
    UBaseType_t ulMemoryPeak;  /* Peak memory usage (bytes) */
} MemoryLimits_t;

/* CPU limits - tick based quota enforcement */
typedef struct xCPU_LIMITS {
    UBaseType_t ulCpuQuota;         /* CPU time quota (percentage * 100, e.g., 5000 = 50%) */
    UBaseType_t ulTicksUsed;        /* Number of ticks used in current window */
    UBaseType_t ulTicksQuota;       /* Max number of ticks allowed per window */
    UBaseType_t ulPenaltyTicksLeft; /* Remaining penalty ticks */
    TickType_t  xWindowStartTime;   /* Start time of current window (in ticks) */
    TickType_t  xWindowDuration;    /* Duration of time window (in ticks) */
} CpuLimits_t;

/* CGroup structure */
typedef struct xCGROUP {
    char           pcGroupName[configMAX_CGROUP_NAME_LEN]; /* CGroup name */
    MemoryLimits_t xMemoryLimits;                          /* Memory constraints */
    CpuLimits_t    xCpuLimits;                             /* CPU constraints */
    List_t         xTaskList;                              /* List of tasks in this cgroup */
    UBaseType_t    uxTaskCount;                            /* Number of tasks in cgroup */
    BaseType_t     xActive;                                /* CGroup active flag */
} CGroup_t;

/*-----------------------------------------------------------
 * CPU TICK CONTROL MACROS
 *----------------------------------------------------------*/

/* Default time window duration for CPU quota enforcement (in ticks) */
#ifndef configCGROUP_CPU_WINDOW_DURATION
    #define configCGROUP_CPU_WINDOW_DURATION pdMS_TO_TICKS(1000) /* 1 second window */
#endif

/* Default penalty factor - how many ticks to penalize when quota exceeded */
#ifndef configCGROUP_CPU_PENALTY_FACTOR
    #define configCGROUP_CPU_PENALTY_FACTOR 2 /* Penalize 2x the excess ticks */
#endif

/* Helper macros */
#define CGROUP_NO_LIMIT ((UBaseType_t) - 1)
#define CGROUP_CPU_QUOTA_MAX (1000U) /* 100% = 10000 */

/*-----------------------------------------------------------
 * CGROUP API FUNCTIONS
 *----------------------------------------------------------*/

#if (configUSE_CGROUPS == 1)

/**
 * cgroup.h
 * @brief Create a new cgroup with specified resource limits
 *
 * @param pcGroupName Name of the cgroup
 * @param ulMemoryLimit Maximum memory allowed (bytes), use CGROUP_NO_LIMIT for no limit, note that
 *                      memory limits are for all groups, including the kernel
 *                      and other system tasks. So if it's not enough, the task may not be able to
 * run.
 * @param ulCpuQuota CPU quota (percentage * 100), use CGROUP_CPU_QUOTA_MAX for no limit
 * @return CGroup handle on success, NULL on failure
 */
CGroupHandle_t
xCGroupCreate(const char *const pcGroupName, UBaseType_t ulMemoryLimit, UBaseType_t ulCpuQuota);

/**
 * cgroup.h
 * @brief Delete a cgroup (must be empty)
 *
 * @param xCGroup Handle to the cgroup
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xCGroupDelete(CGroupHandle_t xCGroup);

/**
 * cgroup.h
 * @brief Add a task to a cgroup
 *
 * @param xCGroup Handle to the cgroup
 * @param xTask Handle to the task
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xCGroupAddTask(CGroupHandle_t xCGroup, TaskHandle_t xTask);

/**
 * cgroup.h
 * @brief Remove a task from a cgroup
 *
 * @param xCGroup Handle to the cgroup
 * @param xTask Handle to the task
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xCGroupRemoveTask(CGroupHandle_t xCGroup, TaskHandle_t xTask);

/**
 * cgroup.h
 * @brief Check if a task can allocate memory within cgroup limits
 *
 * @param xTask Handle to the task
 * @param ulSize Size of memory to allocate
 * @return pdTRUE if allowed, pdFALSE if would exceed limits
 */
BaseType_t xCGroupCheckMemoryLimit(TaskHandle_t xTask, UBaseType_t ulSize);

/**
 * cgroup.h
 * @brief Update memory usage for a task's cgroup
 *
 * @param xTask Handle to the task
 * @param lMemoryDelta Memory change (positive for allocation, negative for free)
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xCGroupUpdateMemoryUsage(TaskHandle_t xTask, BaseType_t lMemoryDelta);

/**
 * cgroup.h
 * @brief Get cgroup statistics
 *
 * @param xCGroup Handle to the cgroup
 * @param pxMemoryLimits Pointer to store memory statistics
 * @param pxCpuLimits Pointer to store CPU statistics
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t
xCGroupGetStats(CGroupHandle_t xCGroup, MemoryLimits_t *pxMemoryLimits, CpuLimits_t *pxCpuLimits);

/**
 * cgroup.h
 * @brief Set memory limit for a cgroup
 *
 * @param xCGroup Handle to the cgroup
 * @param ulMemoryLimit New memory limit in bytes
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xCGroupSetMemoryLimit(CGroupHandle_t xCGroup, UBaseType_t ulMemoryLimit);

/**
 * cgroup.h
 * @brief Set CPU quota for a cgroup
 *
 * @param xCGroup Handle to the cgroup
 * @param ulCpuQuota New CPU quota (percentage * 100)
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xCGroupSetCpuQuota(CGroupHandle_t xCGroup, UBaseType_t ulCpuQuota);

/**
 * cgroup.h
 * @brief Get the cgroup handle for a task
 *
 * @param xTask Handle to the task
 * @return CGroup handle, or NULL if task is not in any cgroup
 */
CGroupHandle_t xCGroupGetTaskGroup(TaskHandle_t xTask);

/**
 * cgroup.h
 * @brief Get memory usage statistics for a specific cgroup
 *
 * @param xCGroup Handle to the cgroup
 * @param pulMemoryUsed Pointer to store current memory usage
 * @param pulMemoryLimit Pointer to store memory limit
 * @param pulMemoryPeak Pointer to store peak memory usage
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xCGroupGetMemoryInfo(CGroupHandle_t xCGroup,
                                UBaseType_t   *pulMemoryUsed,
                                UBaseType_t   *pulMemoryLimit,
                                UBaseType_t   *pulMemoryPeak);

/**
 * cgroup.h
 * @brief Reset memory usage statistics for a cgroup
 *
 * @param xCGroup Handle to the cgroup
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xCGroupResetMemoryStats(CGroupHandle_t xCGroup);

/**
 * cgroup.h
 * @brief Get total memory usage across all cgroups
 *
 * @return Total memory usage in bytes
 */
UBaseType_t xCGroupGetTotalMemoryUsage(void);

/*-----------------------------------------------------------
 * INTERNAL KERNEL INTEGRATION FUNCTIONS
 * These functions are used internally by FreeRTOS kernel
 *----------------------------------------------------------*/

/**
 * @brief Called by kernel on each tick to update cgroup time windows
 * This function is called from xTaskIncrementTick()
 */
void prvCGroupUpdateTick(void);

/**
 * @brief Called by kernel when a task is switched out
 * This function is called from vTaskSwitchContext()
 *
 * @param xTask Handle to the task being switched out
 */
void prvCGroupTaskSwitchOut(TaskHandle_t xTask);

/**
 * @brief Called by kernel to check if a task can run based on cgroup limits
 * This function is called from vTaskSwitchContext()
 *
 * @param xTask Handle to the task to check
 * @return pdTRUE if task can run, pdFALSE if throttled by cgroup
 */
BaseType_t prvCGroupCanTaskRun(TaskHandle_t xTask);

#else /* configUSE_CGROUPS == 0 */

    /* When cgroups are disabled, provide empty macros */
    #define xCGroupCreate(pcGroupName, ulMemoryLimit, ulCpuQuota) NULL
    #define xCGroupDelete(xCGroup) pdFAIL
    #define xCGroupAddTask(xCGroup, xTask) pdFAIL
    #define xCGroupRemoveTask(xCGroup, xTask) pdFAIL
    #define xCGroupCheckMemoryLimit(xTask, ulSize) pdTRUE
    #define xCGroupUpdateMemoryUsage(xTask, lMemoryDelta) pdPASS
    #define xCGroupGetStats(xCGroup, pxMemoryLimits, pxCpuLimits) pdFAIL
    #define xCGroupSetMemoryLimit(xCGroup, ulMemoryLimit) pdFAIL
    #define xCGroupSetCpuQuota(xCGroup, ulCpuQuota) pdFAIL
    #define xCGroupGetTaskGroup(xTask) NULL

    /* Internal kernel integration functions - empty when disabled */
    #define prvCGroupUpdateTick()                                                                  \
        do {                                                                                       \
        } while (0)
    #define prvCGroupTaskSwitchOut(xTask)                                                          \
        do {                                                                                       \
        } while (0)
    #define prvCGroupCanTaskRun(xTask) pdTRUE

#endif /* configUSE_CGROUPS */

#ifdef __cplusplus
}
#endif

#endif /* INC_CGROUP_H */
