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

#include "cgroup.h"
#include "FreeRTOS.h"

#if (configUSE_CGROUPS == 1)

/*-----------------------------------------------------------
 * PRIVATE DATA
 *----------------------------------------------------------*/

/* Simple task-to-cgroup mapping table */
typedef struct {
    TaskHandle_t   xTask;
    CGroupHandle_t xCGroup;
} TaskCGroupMap_t;

/* Array to store all cgroups */
static CGroup_t xCGroups[configMAX_CGROUPS];

/* Bitmap to track which cgroup slots are in use */
static UBaseType_t uxCGroupBitmap = 0U;

/* Task to CGroup mapping */
static TaskCGroupMap_t xTaskCGroupMap[configMAX_CGROUPS * 8];
static UBaseType_t     uxMapCount = 0U;

/*-----------------------------------------------------------
 * PRIVATE FUNCTION PROTOTYPES
 *----------------------------------------------------------*/

static BaseType_t prvFindFreeCGroupSlot(void);
static BaseType_t prvGetCGroupIndex(CGroupHandle_t xCGroup);
static CGroup_t  *prvGetCGroupFromTask(TaskHandle_t xTask);
static BaseType_t prvAddTaskToMap(TaskHandle_t xTask, CGroupHandle_t xCGroup);
static BaseType_t prvRemoveTaskFromMap(TaskHandle_t xTask);
static void       prvUpdateCpuWindow(CGroup_t *pxCGroup);
static void       prvCalculatePenalty(CGroup_t *pxCGroup);

/* Integration functions called by FreeRTOS kernel */
void       prvCGroupTaskSwitchOut(TaskHandle_t xTask);
BaseType_t prvCGroupCanTaskRun(TaskHandle_t xTask);
void       prvCGroupUpdateTick(void);

/*-----------------------------------------------------------
 * PRIVATE FUNCTIONS
 *----------------------------------------------------------*/

static BaseType_t prvFindFreeCGroupSlot(void) {
    BaseType_t xIndex;

    for (xIndex = 0; xIndex < configMAX_CGROUPS; xIndex++) {
        if ((uxCGroupBitmap & (1U << xIndex)) == 0U) {
            return xIndex;
        }
    }

    return -1;
}

static BaseType_t prvGetCGroupIndex(CGroupHandle_t xCGroup) {
    BaseType_t xIndex;

    if (xCGroup == NULL) {
        return -1;
    }

    for (xIndex = 0; xIndex < configMAX_CGROUPS; xIndex++) {
        if (&xCGroups[xIndex] == (CGroup_t *)xCGroup) {
            return xIndex;
        }
    }

    return -1;
}

static CGroup_t *prvGetCGroupFromTask(TaskHandle_t xTask) {
    UBaseType_t ux;

    if (xTask == NULL) {
        return NULL;
    }

    /* Search the mapping table */
    for (ux = 0; ux < uxMapCount; ux++) {
        if (xTaskCGroupMap[ux].xTask == xTask) {
            return (CGroup_t *)xTaskCGroupMap[ux].xCGroup;
        }
    }

    return NULL;
}

static BaseType_t prvAddTaskToMap(TaskHandle_t xTask, CGroupHandle_t xCGroup) {
    if (uxMapCount >= (configMAX_CGROUPS * 8)) {
        return pdFAIL;
    }

    /* Check if task is already mapped */
    if (prvGetCGroupFromTask(xTask) != NULL) {
        return pdFAIL;
    }

    xTaskCGroupMap[uxMapCount].xTask = xTask;
    xTaskCGroupMap[uxMapCount].xCGroup = xCGroup;
    uxMapCount++;

    return pdPASS;
}

static BaseType_t prvRemoveTaskFromMap(TaskHandle_t xTask) {
    UBaseType_t ux;

    /* Find and remove the mapping */
    for (ux = 0; ux < uxMapCount; ux++) {
        if (xTaskCGroupMap[ux].xTask == xTask) {
            /* Move the last entry to fill the gap */
            if (ux < (uxMapCount - 1)) {
                xTaskCGroupMap[ux] = xTaskCGroupMap[uxMapCount - 1];
            }
            uxMapCount--;
            return pdPASS;
        }
    }

    return pdFAIL;
}

static void prvUpdateCpuWindow(CGroup_t *pxCGroup) {
    TickType_t xCurrentTime;

    if (pxCGroup == NULL) {
        return;
    }

    xCurrentTime = xTaskGetTickCount();

    /* Check if we need to start a new time window */
    if ((xCurrentTime - pxCGroup->xCpuLimits.xWindowStartTime) >=
        pxCGroup->xCpuLimits.xWindowDuration) {
        /* Calculate penalty for the ending window */
        prvCalculatePenalty(pxCGroup);

        /* Start new window */
        pxCGroup->xCpuLimits.xWindowStartTime = xCurrentTime;
        pxCGroup->xCpuLimits.ulTicksUsed = 0U;

        /* Reduce penalty slices if any */
        if (pxCGroup->xCpuLimits.ulPenaltyTicksLeft > 0U) {
            pxCGroup->xCpuLimits.ulPenaltyTicksLeft =
                (pxCGroup->xCpuLimits.ulPenaltyTicksLeft > 1U)
                    ? (pxCGroup->xCpuLimits.ulPenaltyTicksLeft - 1U)
                    : 0U;
        }
    }
}

static void prvCalculatePenalty(CGroup_t *pxCGroup) {
    UBaseType_t ulExcess;

    if (pxCGroup == NULL) {
        return;
    }

    /* Check if quota was exceeded */
    if ((pxCGroup->xCpuLimits.ulTicksQuota != CGROUP_NO_LIMIT) &&
        (pxCGroup->xCpuLimits.ulTicksUsed > pxCGroup->xCpuLimits.ulTicksQuota)) {
        ulExcess = pxCGroup->xCpuLimits.ulTicksUsed - pxCGroup->xCpuLimits.ulTicksQuota;

        /* Apply penalty: penalize configCGROUP_CPU_PENALTY_FACTOR times the excess */
        pxCGroup->xCpuLimits.ulPenaltyTicksLeft +=
            (ulExcess * configCGROUP_CPU_WINDOW_DURATION / pxCGroup->xCpuLimits.ulTicksQuota);
    }
}

/*-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *----------------------------------------------------------*/

CGroupHandle_t
xCGroupCreate(const char *const pcGroupName, UBaseType_t ulMemoryLimit, UBaseType_t ulCpuQuota) {
    BaseType_t  xIndex;
    CGroup_t   *pxNewCGroup;
    UBaseType_t uxLength;
    TickType_t  xCurrentTime;

    configASSERT(pcGroupName != NULL);

    /* Find a free cgroup slot */
    portENTER_CRITICAL();
    xIndex = prvFindFreeCGroupSlot();
    if (xIndex < 0) {
        portEXIT_CRITICAL();
        return NULL;
    }

    /* Mark this slot as used */
    uxCGroupBitmap |= (1U << xIndex);
    portEXIT_CRITICAL();

    pxNewCGroup = &xCGroups[xIndex];

    /* Initialize the cgroup structure */
    uxLength = 0;
    while ((pcGroupName[uxLength] != '\0') && (uxLength < (configMAX_CGROUP_NAME_LEN - 1))) {
        pxNewCGroup->pcGroupName[uxLength] = pcGroupName[uxLength];
        uxLength++;
    }
    pxNewCGroup->pcGroupName[uxLength] = '\0';

    /* Initialize memory limits */
    pxNewCGroup->xMemoryLimits.ulMemoryLimit = ulMemoryLimit;
    pxNewCGroup->xMemoryLimits.ulMemoryUsed = 0U;
    pxNewCGroup->xMemoryLimits.ulMemoryPeak = 0U;

    /* Initialize CPU limits with tick-based quota enforcement */
    xCurrentTime = xTaskGetTickCount();
    pxNewCGroup->xCpuLimits.ulCpuQuota = ulCpuQuota;
    pxNewCGroup->xCpuLimits.ulTicksUsed = 0U;
    /* Calculate max ticks per window based on CPU quota percentage */
    pxNewCGroup->xCpuLimits.ulTicksQuota =
        (ulCpuQuota == CGROUP_CPU_QUOTA_MAX) ? CGROUP_NO_LIMIT : ulCpuQuota;
    pxNewCGroup->xCpuLimits.ulPenaltyTicksLeft = 0U;
    pxNewCGroup->xCpuLimits.xWindowStartTime = xCurrentTime;
    pxNewCGroup->xCpuLimits.xWindowDuration = configCGROUP_CPU_WINDOW_DURATION;

    /* Initialize task list */
    vListInitialise(&(pxNewCGroup->xTaskList));
    pxNewCGroup->uxTaskCount = 0U;
    pxNewCGroup->xActive = pdTRUE;

    return (CGroupHandle_t)pxNewCGroup;
}

BaseType_t xCGroupDelete(CGroupHandle_t xCGroup) {
    BaseType_t xIndex;
    CGroup_t  *pxCGroup;

    if (xCGroup == NULL) {
        return pdFAIL;
    }

    pxCGroup = (CGroup_t *)xCGroup;
    xIndex = prvGetCGroupIndex(xCGroup);

    if (xIndex < 0) {
        return pdFAIL;
    }

    portENTER_CRITICAL();

    /* Check if cgroup is empty */
    if (pxCGroup->uxTaskCount > 0U) {
        portEXIT_CRITICAL();
        return pdFAIL;
    }

    /* Mark as inactive and free the slot */
    pxCGroup->xActive = pdFALSE;
    uxCGroupBitmap &= ~(1U << xIndex);

    portEXIT_CRITICAL();

    return pdPASS;
}

BaseType_t xCGroupAddTask(CGroupHandle_t xCGroup, TaskHandle_t xTask) {
    CGroup_t  *pxCGroup;
    BaseType_t xResult;

    if ((xCGroup == NULL) || (xTask == NULL)) {
        return pdFAIL;
    }

    pxCGroup = (CGroup_t *)xCGroup;

    if (pxCGroup->xActive == pdFALSE) {
        return pdFAIL;
    }

    portENTER_CRITICAL();

    /* Add task to mapping */
    xResult = prvAddTaskToMap(xTask, xCGroup);
    if (xResult == pdPASS) {
        pxCGroup->uxTaskCount++;
    }

    portEXIT_CRITICAL();

    return xResult;
}

BaseType_t xCGroupRemoveTask(CGroupHandle_t xCGroup, TaskHandle_t xTask) {
    CGroup_t  *pxCGroup;
    CGroup_t  *pxCurrentCGroup;
    BaseType_t xResult;

    if ((xCGroup == NULL) || (xTask == NULL)) {
        return pdFAIL;
    }

    pxCGroup = (CGroup_t *)xCGroup;

    portENTER_CRITICAL();

    /* Check if task is in this cgroup */
    pxCurrentCGroup = prvGetCGroupFromTask(xTask);
    if (pxCurrentCGroup != pxCGroup) {
        portEXIT_CRITICAL();
        return pdFAIL;
    }

    /* Remove task from mapping */
    xResult = prvRemoveTaskFromMap(xTask);
    if (xResult == pdPASS) {
        pxCGroup->uxTaskCount--;
    }

    portEXIT_CRITICAL();

    return xResult;
}

BaseType_t xCGroupCheckMemoryLimit(TaskHandle_t xTask, UBaseType_t ulSize) {
    CGroup_t   *pxCGroup;
    UBaseType_t ulNewUsage;

    if (xTask == NULL) {
        return pdTRUE;
    }

    pxCGroup = prvGetCGroupFromTask(xTask);
    if (pxCGroup == NULL) {
        /* Task not in any cgroup, allow allocation */
        return pdTRUE;
    }

    /* Check if there's a memory limit */
    if (pxCGroup->xMemoryLimits.ulMemoryLimit == CGROUP_NO_LIMIT) {
        return pdTRUE;
    }

    ulNewUsage = pxCGroup->xMemoryLimits.ulMemoryUsed + ulSize;

    return (ulNewUsage <= pxCGroup->xMemoryLimits.ulMemoryLimit) ? pdTRUE : pdFALSE;
}

BaseType_t xCGroupUpdateMemoryUsage(TaskHandle_t xTask, BaseType_t lMemoryDelta) {
    CGroup_t *pxCGroup;

    if (xTask == NULL) {
        return pdPASS;
    }

    pxCGroup = prvGetCGroupFromTask(xTask);
    if (pxCGroup == NULL) {
        /* Task not in any cgroup */
        return pdPASS;
    }

    portENTER_CRITICAL();

    if (lMemoryDelta > 0) {
        /* Memory allocation */
        pxCGroup->xMemoryLimits.ulMemoryUsed += (UBaseType_t)lMemoryDelta;
        if (pxCGroup->xMemoryLimits.ulMemoryUsed > pxCGroup->xMemoryLimits.ulMemoryPeak) {
            pxCGroup->xMemoryLimits.ulMemoryPeak = pxCGroup->xMemoryLimits.ulMemoryUsed;
        }
    } else {
        /* Memory deallocation */
        UBaseType_t ulDealloc = (UBaseType_t)(-lMemoryDelta);
        if (ulDealloc > pxCGroup->xMemoryLimits.ulMemoryUsed) {
            pxCGroup->xMemoryLimits.ulMemoryUsed = 0U;
        } else {
            pxCGroup->xMemoryLimits.ulMemoryUsed -= ulDealloc;
        }
    }

    portEXIT_CRITICAL();

    return pdPASS;
}

BaseType_t
xCGroupGetStats(CGroupHandle_t xCGroup, MemoryLimits_t *pxMemoryLimits, CpuLimits_t *pxCpuLimits) {
    CGroup_t *pxCGroup;

    if ((xCGroup == NULL) || (pxMemoryLimits == NULL) || (pxCpuLimits == NULL)) {
        return pdFAIL;
    }

    pxCGroup = (CGroup_t *)xCGroup;

    if (pxCGroup->xActive == pdFALSE) {
        return pdFAIL;
    }

    portENTER_CRITICAL();

    /* Update CPU window */
    prvUpdateCpuWindow(pxCGroup);

    /* Copy memory statistics */
    pxMemoryLimits->ulMemoryLimit = pxCGroup->xMemoryLimits.ulMemoryLimit;
    pxMemoryLimits->ulMemoryUsed = pxCGroup->xMemoryLimits.ulMemoryUsed;
    pxMemoryLimits->ulMemoryPeak = pxCGroup->xMemoryLimits.ulMemoryPeak;

    /* Copy CPU statistics */
    pxCpuLimits->ulCpuQuota = pxCGroup->xCpuLimits.ulCpuQuota;
    pxCpuLimits->ulTicksUsed = pxCGroup->xCpuLimits.ulTicksUsed;
    pxCpuLimits->ulTicksQuota = pxCGroup->xCpuLimits.ulTicksQuota;
    pxCpuLimits->ulPenaltyTicksLeft = pxCGroup->xCpuLimits.ulPenaltyTicksLeft;
    pxCpuLimits->xWindowStartTime = pxCGroup->xCpuLimits.xWindowStartTime;
    pxCpuLimits->xWindowDuration = pxCGroup->xCpuLimits.xWindowDuration;

    portEXIT_CRITICAL();

    return pdPASS;
}

BaseType_t xCGroupSetMemoryLimit(CGroupHandle_t xCGroup, UBaseType_t ulMemoryLimit) {
    CGroup_t *pxCGroup;

    if (xCGroup == NULL) {
        return pdFAIL;
    }

    pxCGroup = (CGroup_t *)xCGroup;

    if (pxCGroup->xActive == pdFALSE) {
        return pdFAIL;
    }

    portENTER_CRITICAL();
    pxCGroup->xMemoryLimits.ulMemoryLimit = ulMemoryLimit;
    portEXIT_CRITICAL();

    return pdPASS;
}

BaseType_t xCGroupSetCpuQuota(CGroupHandle_t xCGroup, UBaseType_t ulCpuQuota) {
    CGroup_t *pxCGroup;

    if (xCGroup == NULL) {
        return pdFAIL;
    }

    pxCGroup = (CGroup_t *)xCGroup;

    if (pxCGroup->xActive == pdFALSE) {
        return pdFAIL;
    }

    /* Validate CPU quota range */
    if (ulCpuQuota > CGROUP_CPU_QUOTA_MAX) {
        return pdFAIL;
    }

    portENTER_CRITICAL();
    pxCGroup->xCpuLimits.ulCpuQuota = ulCpuQuota;
    /* Recalculate quota in ticks based on CPU quota percentage */
    pxCGroup->xCpuLimits.ulTicksQuota =
        (ulCpuQuota == CGROUP_CPU_QUOTA_MAX) ? CGROUP_NO_LIMIT : ulCpuQuota;
    portEXIT_CRITICAL();

    return pdPASS;
}

CGroupHandle_t xCGroupGetTaskGroup(TaskHandle_t xTask) {
    CGroup_t *pxCGroup;

    if (xTask == NULL) {
        return NULL;
    }

    pxCGroup = prvGetCGroupFromTask(xTask);

    return (CGroupHandle_t)pxCGroup;
}

BaseType_t xCGroupGetMemoryInfo(CGroupHandle_t xCGroup,
                                UBaseType_t   *pulMemoryUsed,
                                UBaseType_t   *pulMemoryLimit,
                                UBaseType_t   *pulMemoryPeak) {
    CGroup_t *pxCGroup;

    if ((xCGroup == NULL) || (pulMemoryUsed == NULL) || (pulMemoryLimit == NULL) ||
        (pulMemoryPeak == NULL)) {
        return pdFAIL;
    }

    pxCGroup = (CGroup_t *)xCGroup;

    if (pxCGroup->xActive == pdFALSE) {
        return pdFAIL;
    }

    portENTER_CRITICAL();
    *pulMemoryUsed = pxCGroup->xMemoryLimits.ulMemoryUsed;
    *pulMemoryLimit = pxCGroup->xMemoryLimits.ulMemoryLimit;
    *pulMemoryPeak = pxCGroup->xMemoryLimits.ulMemoryPeak;
    portEXIT_CRITICAL();

    return pdPASS;
}

BaseType_t xCGroupResetMemoryStats(CGroupHandle_t xCGroup) {
    CGroup_t *pxCGroup;

    if (xCGroup == NULL) {
        return pdFAIL;
    }

    pxCGroup = (CGroup_t *)xCGroup;

    if (pxCGroup->xActive == pdFALSE) {
        return pdFAIL;
    }

    portENTER_CRITICAL();
    /* Reset current usage but keep the limit */
    pxCGroup->xMemoryLimits.ulMemoryUsed = 0U;
    pxCGroup->xMemoryLimits.ulMemoryPeak = 0U;
    portEXIT_CRITICAL();

    return pdPASS;
}

UBaseType_t xCGroupGetTotalMemoryUsage(void) {
    BaseType_t  xIndex;
    CGroup_t   *pxCGroup;
    UBaseType_t ulTotalUsage = 0U;

    portENTER_CRITICAL();

    /* Sum up memory usage from all active cgroups */
    for (xIndex = 0; xIndex < configMAX_CGROUPS; xIndex++) {
        if ((uxCGroupBitmap & (1U << xIndex)) != 0U) {
            pxCGroup = &xCGroups[xIndex];
            if (pxCGroup->xActive == pdTRUE) {
                ulTotalUsage += pxCGroup->xMemoryLimits.ulMemoryUsed;
            }
        }
    }

    portEXIT_CRITICAL();

    return ulTotalUsage;
}

/*-----------------------------------------------------------
 * INTEGRATION FUNCTIONS
 *----------------------------------------------------------*/

void prvCGroupTaskSwitchOut(TaskHandle_t xTask) {
    CGroup_t *pxCGroup;

    if (xTask == NULL) {
        return;
    }

    pxCGroup = prvGetCGroupFromTask(xTask);
    if (pxCGroup == NULL) {
        return;
    }

    /* Now that we're using tick-based accounting, the tick usage
     * is updated in prvCGroupUpdateTick() for the currently running task */
    /* This function is kept for potential future extensions */
}

BaseType_t prvCGroupCanTaskRun(TaskHandle_t xTask) {
    CGroup_t *pxCGroup;

    if (xTask == NULL) {
        return pdTRUE;
    }

    pxCGroup = prvGetCGroupFromTask(xTask);
    if (pxCGroup == NULL) {
        /* Task not in any cgroup, allow to run */
        return pdTRUE;
    }

    /* Only check current state, don't update window here */
    /* Window updates are handled centrally in prvCGroupUpdateTick() */

    /* Check if task is in penalty period */
    if (pxCGroup->xCpuLimits.ulPenaltyTicksLeft > 0U) {
        return pdFALSE;
    }

    /* Check if current window has exceeded quota */
    if ((pxCGroup->xCpuLimits.ulTicksQuota != CGROUP_NO_LIMIT) &&
        (pxCGroup->xCpuLimits.ulTicksUsed >= pxCGroup->xCpuLimits.ulTicksQuota)) {
        return pdFALSE;
    }

    return pdTRUE;
}

void prvCGroupUpdateTick(void) {
    BaseType_t   xIndex;
    CGroup_t    *pxCGroup;
    TaskHandle_t xCurrentTask;
    CGroup_t    *pxCurrentTaskCGroup;

    /* Get the currently running task and update its cgroup's tick usage */
    xCurrentTask = xTaskGetCurrentTaskHandle();
    if (xCurrentTask != NULL) {
        pxCurrentTaskCGroup = prvGetCGroupFromTask(xCurrentTask);
        if (pxCurrentTaskCGroup != NULL) {
            /* Increment tick usage for the currently running task's cgroup */
            pxCurrentTaskCGroup->xCpuLimits.ulTicksUsed++;
        }
    }

    /* Update all active cgroups - check windows less frequently to avoid constant resets */
    /* Only check windows every 10 ticks to reduce overhead and provide stable statistics */
    for (xIndex = 0; xIndex < configMAX_CGROUPS; xIndex++) {
        if ((uxCGroupBitmap & (1U << xIndex)) != 0U) {
            pxCGroup = &xCGroups[xIndex];
            if (pxCGroup->xActive == pdTRUE) {
                /* Update CPU window for this cgroup */
                prvUpdateCpuWindow(pxCGroup);

                /* Reduce penalty if in penalty period */
                if (pxCGroup->xCpuLimits.ulPenaltyTicksLeft > 0U) {
                    pxCGroup->xCpuLimits.ulPenaltyTicksLeft--;
                }
            }
        }
    }
}

#endif /* configUSE_CGROUPS == 1 */
