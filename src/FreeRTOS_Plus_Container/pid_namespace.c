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

#include "pid_namespace.h"
#include "FreeRTOS.h"
#include "projdefs.h"

#if (configUSE_PID_NAMESPACE == 1)

/*-----------------------------------------------------------
 * PRIVATE DATA STRUCTURES AND VARIABLES
 *----------------------------------------------------------*/

/* Simple string utility functions */
static size_t prvStrlen(const char *pcString) {
    size_t xLength = 0U;

    if (pcString != NULL) {
        while (pcString[xLength] != '\0') {
            xLength++;
        }
    }

    return xLength;
}

static void prvMemcpy(void *pvDest, const void *pvSrc, size_t xLength) {
    uint8_t       *pucDest = (uint8_t *)pvDest;
    const uint8_t *pucSrc = (const uint8_t *)pvSrc;
    size_t         ux;

    if ((pvDest != NULL) && (pvSrc != NULL)) {
        for (ux = 0U; ux < xLength; ux++) {
            pucDest[ux] = pucSrc[ux];
        }
    }
}

/* Array to hold all PID namespaces */
static PidNamespace_t xPidNamespaces[configMAX_PID_NAMESPACES];

/* Bitmap to track which namespace slots are in use */
static UBaseType_t uxNamespaceBitmap = 0U;

/* Root namespace handle */
static PidNamespaceHandle_t xRootNamespace = NULL;

/* Next namespace ID */
static UBaseType_t ulNextNamespaceId = 1U;

/*-----------------------------------------------------------
 * PRIVATE FUNCTION PROTOTYPES
 *----------------------------------------------------------*/

static BaseType_t  prvFindFreeNamespaceSlot(UBaseType_t *puxSlot);
static UBaseType_t prvAllocateVirtualPid(PidNamespace_t *pxNamespace);
static void        prvInitializeNamespace(PidNamespace_t *pxNamespace, const char *pcName);

/*-----------------------------------------------------------
 * PRIVATE FUNCTIONS
 *----------------------------------------------------------*/

static BaseType_t prvFindFreeNamespaceSlot(UBaseType_t *puxSlot) {
    UBaseType_t ux;

    for (ux = 0U; ux < configMAX_PID_NAMESPACES; ux++) {
        if ((uxNamespaceBitmap & (1U << ux)) == 0U) {
            *puxSlot = ux;
            return pdPASS;
        }
    }

    return pdFAIL;
}

static UBaseType_t prvAllocateVirtualPid(PidNamespace_t *pxNamespace) {
    UBaseType_t ulPid;

    if (pxNamespace == NULL) {
        return 0U;
    }

    if (pxNamespace->ulNextPid > pxNamespace->ulMaxPid) {
        return 0U;
    }

    ulPid = pxNamespace->ulNextPid;
    pxNamespace->ulNextPid++;


    return ulPid;
}

static void prvInitializeNamespace(PidNamespace_t *pxNamespace, const char *pcName) {
    size_t xNameLength;

    /* Initialize namespace structure */
    pxNamespace->ulNamespaceId = ulNextNamespaceId++;
    pxNamespace->ulNextPid = 1U;
    pxNamespace->ulMaxPid = configPID_NAMESPACE_MAX_PID;
    pxNamespace->uxTaskCount = 0U;
    pxNamespace->xActive = pdTRUE;

    /* Copy namespace name */
    if (pcName != NULL) {
        xNameLength = prvStrlen(pcName);
        if (xNameLength >= configMAX_PID_NAMESPACE_NAME_LEN) {
            xNameLength = configMAX_PID_NAMESPACE_NAME_LEN - 1U;
        }
        prvMemcpy(pxNamespace->pcNamespaceName, pcName, xNameLength);
        pxNamespace->pcNamespaceName[xNameLength] = '\0';
    } else {
        pxNamespace->pcNamespaceName[0] = '\0';
    }

    /* Initialize task list */
    for (int i = 0; i < configPID_NAMESPACE_MAX_PID; i++) {
        pxNamespace->xTasks[i] = NULL;
    }
}

/*-----------------------------------------------------------
 * PUBLIC API FUNCTIONS
 *----------------------------------------------------------*/

void prvPidNamespaceInit(void) {
    UBaseType_t ux;

    /* Initialize all namespace slots as inactive */
    for (ux = 0U; ux < configMAX_PID_NAMESPACES; ux++) {
        xPidNamespaces[ux].xActive = pdFALSE;
    }

    /* Initialize namespace system */
    uxNamespaceBitmap = 0U;
    ulNextNamespaceId = 1U;

    /* Create root namespace */
    xRootNamespace = xPidNamespaceCreate("root");
    configASSERT(xRootNamespace != NULL);
}

PidNamespaceHandle_t xPidNamespaceCreate(const char *const pcNamespaceName) {
    UBaseType_t     uxSlot;
    PidNamespace_t *pxNamespace;

    /* Find a free namespace slot */
    if (prvFindFreeNamespaceSlot(&uxSlot) != pdPASS) {
        return NULL;
    }

    pxNamespace = &xPidNamespaces[uxSlot];

    portENTER_CRITICAL();

    /* Initialize the namespace */
    prvInitializeNamespace(pxNamespace, pcNamespaceName);

    /* Mark slot as used */
    uxNamespaceBitmap |= (1U << uxSlot);

    portEXIT_CRITICAL();

    return (PidNamespaceHandle_t)pxNamespace;
}

BaseType_t xPidNamespaceDelete(PidNamespaceHandle_t xNamespace) {
    PidNamespace_t *pxNamespace = (PidNamespace_t *)xNamespace;
    UBaseType_t     ux;

    if ((xNamespace == NULL) || (pxNamespace->xActive == pdFALSE)) {
        return pdFAIL;
    }

    portENTER_CRITICAL();

    /* Namespace must be empty to be deleted */
    if (pxNamespace->uxTaskCount > 0U) {
        portEXIT_CRITICAL();
        return pdFAIL;
    }

    /* Find and free the namespace slot */
    for (ux = 0U; ux < configMAX_PID_NAMESPACES; ux++) {
        if (&xPidNamespaces[ux] == pxNamespace) {
            pxNamespace->xActive = pdFALSE;
            uxNamespaceBitmap &= ~(1U << ux);
            break;
        }
    }

    portEXIT_CRITICAL();

    return pdPASS;
}

BaseType_t xPidNamespaceAddTask(PidNamespaceHandle_t xNamespace, TaskHandle_t xTask) {
    PidNamespace_t *pxNamespace = (PidNamespace_t *)xNamespace;
    UBaseType_t     ulVirtualPid;
    BaseType_t      xResult = pdFAIL;

    if ((xNamespace == NULL) || (xTask == NULL) || (pxNamespace->xActive == pdFALSE)) {
        return pdFAIL;
    }

    /* Check if task is already in a namespace */
    if (pvTaskGetPidNamespace(xTask) != NULL) {
        return pdFAIL;
    }

    portENTER_CRITICAL();

    /* Allocate virtual PID */
    ulVirtualPid = prvAllocateVirtualPid(pxNamespace);
    if (ulVirtualPid != 0U) {
        /* Set the namespace and virtual PID in the TCB */
        if (xTaskSetPidNamespace(xTask, xNamespace, ulVirtualPid) == pdPASS) {
            pxNamespace->uxTaskCount++;
            xResult = pdPASS;
        }
    }

    portEXIT_CRITICAL();

    return xResult;
}

BaseType_t xPidNamespaceRemoveTask(PidNamespaceHandle_t xNamespace, TaskHandle_t xTask) {
    PidNamespace_t *pxNamespace = (PidNamespace_t *)xNamespace;
    BaseType_t      xResult = pdFAIL;

    if ((xNamespace == NULL) || (xTask == NULL)) {
        return pdFAIL;
    }

    portENTER_CRITICAL();

    /* Check if task belongs to this namespace */
    if (pvTaskGetPidNamespace(xTask) == xNamespace) {
        /* Remove task from namespace by clearing TCB fields */
        if (xTaskSetPidNamespace(xTask, NULL, 0U) == pdPASS) {
            if (pxNamespace->uxTaskCount > 0U) {
                pxNamespace->uxTaskCount--;
            }
            xResult = pdPASS;
        }
    }

    portEXIT_CRITICAL();

    return xResult;
}

UBaseType_t xPidNamespaceGetTaskVirtualPid(TaskHandle_t xTask) {
    if (xTask == NULL) {
        return 0U;
    }

    /* Use the new TCB-based function */
    return uxTaskGetVirtualPid(xTask);
}

TaskHandle_t xPidNamespaceFindTaskByVirtualPid(PidNamespaceHandle_t xNamespace,
                                               UBaseType_t          ulVirtualPid) {
    PidNamespace_t *pxNamespace = (PidNamespace_t *)xNamespace;
    TaskHandle_t    xFoundTask = NULL;

    if ((xNamespace == NULL) || (pxNamespace->xActive == pdFALSE) || (ulVirtualPid == 0U)) {
        return NULL;
    }

    portENTER_CRITICAL();

    /* Search through the namespace task list */
    for (UBaseType_t i = 0; i < configPID_NAMESPACE_MAX_PID; i++) {
        TaskHandle_t xTask = pxNamespace->xTasks[i];
        if (xTask != NULL) {
            UBaseType_t ulPid = uxTaskGetVirtualPid(xTask);
            if (ulPid == ulVirtualPid) {
                xFoundTask = xTask;
                break;
            }
        }
    }

    portEXIT_CRITICAL();

    return xFoundTask;
}

PidNamespaceHandle_t xPidNamespaceGetTaskNamespace(TaskHandle_t xTask) {
    if (xTask == NULL) {
        return NULL;
    }

    /* Use the new TCB-based function */
    return pvTaskGetPidNamespace(xTask);
}

BaseType_t xTaskCreateInNamespace(PidNamespaceHandle_t xNamespace,
                                  TaskFunction_t       pxTaskCode,
                                  const char *const    pcName,
                                  const uint16_t       usStackDepth,
                                  void *const          pvParameters,
                                  UBaseType_t          uxPriority,
                                  TaskHandle_t *const  pxCreatedTask) {
    TaskHandle_t xNewTask;
    BaseType_t   xResult;

    if (xNamespace == NULL) {
        return pdFAIL;
    }

    /* Create the task first */
    xResult = xTaskCreate(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, &xNewTask);

    if (xResult == pdPASS) {
        /* Add the task to the namespace */
        xResult = xPidNamespaceAddTask(xNamespace, xNewTask);

        if (xResult != pdPASS) {
            /* If adding to namespace failed, delete the task */
            // vTaskDelete( xNewTask );
            xNewTask = NULL;
        }
    } else {
        return xResult;
    }

    if (pxCreatedTask != NULL) {
        *pxCreatedTask = xNewTask;
    }

    return xResult;
}

BaseType_t xPidNamespaceGetInfo(PidNamespaceHandle_t xNamespace,
                                UBaseType_t         *pulTaskCount,
                                UBaseType_t         *pulNextPid,
                                UBaseType_t         *pulMaxPid) {
    PidNamespace_t *pxNamespace = (PidNamespace_t *)xNamespace;

    if ((xNamespace == NULL) || (pxNamespace->xActive == pdFALSE)) {
        return pdFAIL;
    }

    portENTER_CRITICAL();

    if (pulTaskCount != NULL) {
        *pulTaskCount = pxNamespace->uxTaskCount;
    }

    if (pulNextPid != NULL) {
        *pulNextPid = pxNamespace->ulNextPid;
    }

    if (pulMaxPid != NULL) {
        *pulMaxPid = pxNamespace->ulMaxPid;
    }

    portEXIT_CRITICAL();

    return pdPASS;
}

void prvPidNamespaceTaskDelete(TaskHandle_t xTask) {
    PidNamespaceHandle_t xNamespace;

    if (xTask != NULL) {
        /* Get the task's namespace and remove it */
        xNamespace = pvTaskGetPidNamespace(xTask);
        if (xNamespace != NULL) {
            (void)xPidNamespaceRemoveTask(xNamespace, xTask);
        }
    }
}

PidNamespaceHandle_t xPidNamespaceGetRoot(void) { return xRootNamespace; }

#endif /* configUSE_PID_NAMESPACE == 1 */
