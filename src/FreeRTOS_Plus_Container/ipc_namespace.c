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
#include "ipc_namespace.h"

#if (configUSE_IPC_NAMESPACE == 1)

/*-----------------------------------------------------------
 * PRIVATE DATA STRUCTURES AND VARIABLES
 *----------------------------------------------------------*/

/* Array of IPC namespaces */
static IpcNamespace_t xIpcNamespaces[configMAX_IPC_NAMESPACES];

/* Bitmap to track which namespace slots are in use */
static UBaseType_t uxNamespaceBitmap = 0U;

/* Array to hold IPC object entries */
static IpcObjectEntry_t
    xIpcObjectEntries[configMAX_IPC_NAMESPACES * configMAX_IPC_OBJECTS_PER_NAMESPACE];

/* Number of active object entries */
static UBaseType_t uxObjectEntryCount = 0U;

/* Root namespace handle */
static IpcNamespaceHandle_t xRootNamespace = NULL;

/* Next namespace ID */
static UBaseType_t ulNextNamespaceId = 1U;

/*-----------------------------------------------------------
 * PRIVATE FUNCTION PROTOTYPES
 *----------------------------------------------------------*/

static size_t            prvStrlen(const char *pcString);
static void              prvStrcpy(char *pcDestination, const char *pcSource, size_t xMaxLength);
static BaseType_t        prvFindFreeNamespaceSlot(UBaseType_t *puxSlot);
static BaseType_t        prvFindFreeObjectEntry(IpcObjectEntry_t **ppxEntry);
static IpcObjectEntry_t *prvFindObjectEntry(IpcNamespaceHandle_t xNamespace, void *pvIpcObject);
static void              prvInitializeNamespace(IpcNamespace_t *pxNamespace, const char *pcName);

/*-----------------------------------------------------------
 * PRIVATE FUNCTIONS
 *----------------------------------------------------------*/

static size_t prvStrlen(const char *pcString) {
    size_t xLength = 0U;

    if (pcString != NULL) {
        while (pcString[xLength] != '\0') {
            xLength++;
        }
    }

    return xLength;
}

static void prvStrcpy(char *pcDestination, const char *pcSource, size_t xMaxLength) {
    size_t xIndex = 0U;

    if ((pcDestination != NULL) && (pcSource != NULL) && (xMaxLength > 0U)) {
        while ((xIndex < (xMaxLength - 1U)) && (pcSource[xIndex] != '\0')) {
            pcDestination[xIndex] = pcSource[xIndex];
            xIndex++;
        }
        pcDestination[xIndex] = '\0';
    }
}

static BaseType_t prvFindFreeNamespaceSlot(UBaseType_t *puxSlot) {
    UBaseType_t ux;

    for (ux = 0U; ux < configMAX_IPC_NAMESPACES; ux++) {
        if ((uxNamespaceBitmap & (1U << ux)) == 0U) {
            *puxSlot = ux;
            return pdPASS;
        }
    }

    return pdFAIL;
}

static BaseType_t prvFindFreeObjectEntry(IpcObjectEntry_t **ppxEntry) {
    UBaseType_t ux;
    UBaseType_t uxMaxEntries = configMAX_IPC_NAMESPACES * configMAX_IPC_OBJECTS_PER_NAMESPACE;

    for (ux = 0U; ux < uxMaxEntries; ux++) {
        if (xIpcObjectEntries[ux].pvIpcObject == NULL) {
            *ppxEntry = &xIpcObjectEntries[ux];
            return pdPASS;
        }
    }

    return pdFAIL;
}

static IpcObjectEntry_t *prvFindObjectEntry(IpcNamespaceHandle_t xNamespace, void *pvIpcObject) {
    UBaseType_t ux;
    UBaseType_t uxMaxEntries = configMAX_IPC_NAMESPACES * configMAX_IPC_OBJECTS_PER_NAMESPACE;

    for (ux = 0U; ux < uxMaxEntries; ux++) {
        if ((xIpcObjectEntries[ux].xNamespace == xNamespace) &&
            (xIpcObjectEntries[ux].pvIpcObject == pvIpcObject)) {
            return &xIpcObjectEntries[ux];
        }
    }

    return NULL;
}

static void prvInitializeNamespace(IpcNamespace_t *pxNamespace, const char *pcName) {
    if (pxNamespace != NULL) {
        prvStrcpy(pxNamespace->pcNamespaceName, pcName, configMAX_IPC_NAMESPACE_NAME_LEN);
        pxNamespace->ulNamespaceId = ulNextNamespaceId++;
        pxNamespace->ulNextObjectId = 1U;
        vListInitialise(&(pxNamespace->xObjectList));
        pxNamespace->uxObjectCount = 0U;
        pxNamespace->xActive = pdTRUE;
    }
}

/*-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *----------------------------------------------------------*/

void vIpcNamespaceInit(void) {
    UBaseType_t ux;
    UBaseType_t uxMaxEntries = configMAX_IPC_NAMESPACES * configMAX_IPC_OBJECTS_PER_NAMESPACE;

    /* Initialize namespace array */
    for (ux = 0U; ux < configMAX_IPC_NAMESPACES; ux++) {
        xIpcNamespaces[ux].xActive = pdFALSE;
    }

    /* Initialize object entry array */
    for (ux = 0U; ux < uxMaxEntries; ux++) {
        xIpcObjectEntries[ux].pvIpcObject = NULL;
        xIpcObjectEntries[ux].xNamespace = NULL;
    }

    /* Initialize namespace system */
    uxNamespaceBitmap = 0U;
    uxObjectEntryCount = 0U;
    ulNextNamespaceId = 1U;

    /* Create root namespace */
    xRootNamespace = xIpcNamespaceCreate("root");
    configASSERT(xRootNamespace != NULL);
}

IpcNamespaceHandle_t xIpcNamespaceCreate(const char *const pcNamespaceName) {
    UBaseType_t     uxSlot;
    IpcNamespace_t *pxNamespace = NULL;

    if ((pcNamespaceName == NULL) || (prvStrlen(pcNamespaceName) == 0U)) {
        return NULL;
    }

    portENTER_CRITICAL();

    if (prvFindFreeNamespaceSlot(&uxSlot) == pdPASS) {
        pxNamespace = &xIpcNamespaces[uxSlot];
        uxNamespaceBitmap |= (1U << uxSlot);
        prvInitializeNamespace(pxNamespace, pcNamespaceName);
    }

    portEXIT_CRITICAL();

    return (IpcNamespaceHandle_t)pxNamespace;
}

BaseType_t xIpcNamespaceDelete(IpcNamespaceHandle_t xNamespace) {
    IpcNamespace_t *pxNamespace = (IpcNamespace_t *)xNamespace;
    UBaseType_t     uxSlot;

    if ((xNamespace == NULL) || (pxNamespace->xActive == pdFALSE)) {
        return pdFAIL;
    }

    /* Don't allow deletion of root namespace */
    if (xNamespace == xRootNamespace) {
        return pdFAIL;
    }

    portENTER_CRITICAL();

    /* Check if namespace is empty */
    if (pxNamespace->uxObjectCount > 0U) {
        portEXIT_CRITICAL();
        return pdFAIL;
    }

    /* Find the slot and mark it as free */
    for (uxSlot = 0U; uxSlot < configMAX_IPC_NAMESPACES; uxSlot++) {
        if (&xIpcNamespaces[uxSlot] == pxNamespace) {
            uxNamespaceBitmap &= ~(1U << uxSlot);
            pxNamespace->xActive = pdFALSE;
            break;
        }
    }

    portEXIT_CRITICAL();

    return pdPASS;
}

UBaseType_t ulIpcNamespaceRegisterObject(IpcNamespaceHandle_t xNamespace,
                                         void                *pvIpcObject,
                                         IpcObjectType_t      xObjectType,
                                         const char *const    pcObjectName) {
    IpcNamespace_t   *pxNamespace = (IpcNamespace_t *)xNamespace;
    IpcObjectEntry_t *pxEntry;
    UBaseType_t       ulObjectId = 0U;

    if ((xNamespace == NULL) || (pvIpcObject == NULL) || (pxNamespace->xActive == pdFALSE)) {
        return 0U;
    }

    portENTER_CRITICAL();

    /* Check if namespace has room for more objects */
    if (pxNamespace->uxObjectCount >= configMAX_IPC_OBJECTS_PER_NAMESPACE) {
        portEXIT_CRITICAL();
        return 0U;
    }

    /* Find a free object entry */
    if (prvFindFreeObjectEntry(&pxEntry) == pdPASS) {
        /* Initialize the entry */
        pxEntry->pvIpcObject = pvIpcObject;
        pxEntry->xObjectType = xObjectType;
        pxEntry->xNamespace = xNamespace;
        pxEntry->ulObjectId = pxNamespace->ulNextObjectId++;
        ulObjectId = pxEntry->ulObjectId;

        /* Copy object name */
        if (pcObjectName != NULL) {
            prvStrcpy(pxEntry->pcObjectName, pcObjectName, configMAX_TASK_NAME_LEN);
        } else {
            pxEntry->pcObjectName[0] = '\0';
        }

        /* Add to namespace list */
        vListInitialiseItem(&(pxEntry->xNamespaceListItem));
        listSET_LIST_ITEM_OWNER(&(pxEntry->xNamespaceListItem), pxEntry);
        listSET_LIST_ITEM_VALUE(&(pxEntry->xNamespaceListItem), ulObjectId);
        vListInsert(&(pxNamespace->xObjectList), &(pxEntry->xNamespaceListItem));

        pxNamespace->uxObjectCount++;
        uxObjectEntryCount++;
    }

    portEXIT_CRITICAL();

    return ulObjectId;
}

BaseType_t xIpcNamespaceUnregisterObject(IpcNamespaceHandle_t xNamespace, void *pvIpcObject) {
    IpcNamespace_t   *pxNamespace = (IpcNamespace_t *)xNamespace;
    IpcObjectEntry_t *pxEntry;
    BaseType_t        xResult = pdFAIL;

    if ((xNamespace == NULL) || (pvIpcObject == NULL) || (pxNamespace->xActive == pdFALSE)) {
        return pdFAIL;
    }

    portENTER_CRITICAL();

    pxEntry = prvFindObjectEntry(xNamespace, pvIpcObject);
    if (pxEntry != NULL) {
        /* Remove from namespace list */
        (void)uxListRemove(&(pxEntry->xNamespaceListItem));

        /* Clear the entry */
        pxEntry->pvIpcObject = NULL;
        pxEntry->xNamespace = NULL;

        pxNamespace->uxObjectCount--;
        uxObjectEntryCount--;
        xResult = pdPASS;
    }

    portEXIT_CRITICAL();

    return xResult;
}

void *pvIpcNamespaceFindObject(IpcNamespaceHandle_t xNamespace,
                               UBaseType_t          ulObjectId,
                               IpcObjectType_t     *pxObjectType) {
    IpcNamespace_t   *pxNamespace = (IpcNamespace_t *)xNamespace;
    ListItem_t       *pxListItem;
    IpcObjectEntry_t *pxEntry;
    void             *pvObject = NULL;

    if ((xNamespace == NULL) || (pxNamespace->xActive == pdFALSE) || (ulObjectId == 0U)) {
        return NULL;
    }

    portENTER_CRITICAL();

    /* Search through the namespace's object list */
    if (!listLIST_IS_EMPTY(&(pxNamespace->xObjectList))) {
        pxListItem = listGET_HEAD_ENTRY(&(pxNamespace->xObjectList));

        do {
            pxEntry = (IpcObjectEntry_t *)listGET_LIST_ITEM_OWNER(pxListItem);
            if (pxEntry->ulObjectId == ulObjectId) {
                pvObject = pxEntry->pvIpcObject;
                if (pxObjectType != NULL) {
                    *pxObjectType = pxEntry->xObjectType;
                }
                break;
            }
            pxListItem = listGET_NEXT(pxListItem);
        } while (pxListItem != listGET_END_MARKER(&(pxNamespace->xObjectList)));
    }

    portEXIT_CRITICAL();

    return pvObject;
}

BaseType_t xIpcNamespaceCheckAccess(TaskHandle_t xTask, void *pvIpcObject) {
    IpcNamespaceHandle_t xTaskNamespace;
    IpcObjectEntry_t    *pxEntry;
    UBaseType_t          ux;
    UBaseType_t uxMaxEntries = configMAX_IPC_NAMESPACES * configMAX_IPC_OBJECTS_PER_NAMESPACE;
    BaseType_t  xObjectRegistered = pdFALSE;

    if (pvIpcObject == NULL) {
        return pdFALSE;
    }

    /* Get the task's IPC namespace */
    xTaskNamespace = xIpcNamespaceGetTaskNamespace(xTask);
    if (xTaskNamespace == NULL) {
        /* Task not in any namespace - use root namespace */
        xTaskNamespace = xRootNamespace;
    }

    portENTER_CRITICAL();

    /* Find if the object is registered in any namespace */
    for (ux = 0U; ux < uxMaxEntries; ux++) {
        pxEntry = &xIpcObjectEntries[ux];
        if (pxEntry->pvIpcObject == pvIpcObject) {
            xObjectRegistered = pdTRUE;

            /* Object found - check namespace access */
            if (pxEntry->xNamespace == xTaskNamespace) {
                /* Object is in task's namespace - allow access */
                portEXIT_CRITICAL();
                return pdTRUE;
            }

            /* Root namespace can access all registered objects */
            if (xTaskNamespace == xRootNamespace) {
                portEXIT_CRITICAL();
                return pdTRUE;
            }

            /* Object is in different namespace - deny access */
            portEXIT_CRITICAL();
            return pdFALSE;
        }
    }

    portEXIT_CRITICAL();

    /* Object not registered in any namespace - allow normal access for compatibility */
    if (xObjectRegistered == pdFALSE) {
        return pdTRUE;
    }

    return pdFALSE;
}

IpcNamespaceHandle_t xIpcNamespaceGetTaskNamespace(TaskHandle_t xTask) {
    /* Use the TCB function to get IPC namespace */
    return pvTaskGetIpcNamespace(xTask);
}

BaseType_t xIpcNamespaceSetTaskNamespace(TaskHandle_t xTask, IpcNamespaceHandle_t xNamespace) {
    /* Use the TCB function to set IPC namespace */
    return xTaskSetIpcNamespace(xTask, xNamespace);
}

BaseType_t xIpcNamespaceGetInfo(IpcNamespaceHandle_t xNamespace,
                                UBaseType_t         *puxObjectCount,
                                UBaseType_t         *puxNextObjectId) {
    IpcNamespace_t *pxNamespace = (IpcNamespace_t *)xNamespace;

    if ((xNamespace == NULL) || (pxNamespace->xActive == pdFALSE)) {
        return pdFAIL;
    }

    if (puxObjectCount != NULL) {
        *puxObjectCount = pxNamespace->uxObjectCount;
    }

    if (puxNextObjectId != NULL) {
        *puxNextObjectId = pxNamespace->ulNextObjectId;
    }

    return pdPASS;
}

IpcNamespaceHandle_t xIpcNamespaceGetRoot(void) { return xRootNamespace; }

/*-----------------------------------------------------------
 * ISOLATED IPC API WRAPPERS
 *----------------------------------------------------------*/

QueueHandle_t xQueueCreateIsolated(UBaseType_t       uxQueueLength,
                                   UBaseType_t       uxItemSize,
                                   const char *const pcQueueName) {
    QueueHandle_t        xQueue;
    IpcNamespaceHandle_t xNamespace;

    /* Create the queue using standard API */
    xQueue = xQueueCreate(uxQueueLength, uxItemSize);
    if (xQueue != NULL) {
        /* Get current task's namespace */
        xNamespace = xIpcNamespaceGetTaskNamespace(NULL);
        if (xNamespace == NULL) {
            xNamespace = xRootNamespace;
        }

        /* Register with namespace */
        if (ulIpcNamespaceRegisterObject(xNamespace, (void *)xQueue, IPC_TYPE_QUEUE, pcQueueName) ==
            0U) {
            /* Registration failed - delete the queue */
            vQueueDelete(xQueue);
            xQueue = NULL;
        }
    }

    return xQueue;
}

SemaphoreHandle_t xSemaphoreCreateBinaryIsolated(const char *const pcSemaphoreName) {
    SemaphoreHandle_t    xSemaphore;
    IpcNamespaceHandle_t xNamespace;

    /* Create the semaphore using standard API */
    xSemaphore = xSemaphoreCreateBinary();
    if (xSemaphore != NULL) {
        /* Get current task's namespace */
        xNamespace = xIpcNamespaceGetTaskNamespace(NULL);
        if (xNamespace == NULL) {
            xNamespace = xRootNamespace;
        }

        /* Register with namespace */
        if (ulIpcNamespaceRegisterObject(xNamespace, (void *)xSemaphore, IPC_TYPE_SEMAPHORE,
                                         pcSemaphoreName) == 0U) {
            /* Registration failed - delete the semaphore */
            vSemaphoreDelete(xSemaphore);
            xSemaphore = NULL;
        }
    }

    return xSemaphore;
}

SemaphoreHandle_t xSemaphoreCreateMutexIsolated(const char *const pcMutexName) {
    SemaphoreHandle_t    xMutex;
    IpcNamespaceHandle_t xNamespace;

    /* Create the mutex using standard API */
    xMutex = xSemaphoreCreateMutex();
    if (xMutex != NULL) {
        /* Get current task's namespace */
        xNamespace = xIpcNamespaceGetTaskNamespace(NULL);
        if (xNamespace == NULL) {
            xNamespace = xRootNamespace;
        }

        /* Register with namespace */
        if (ulIpcNamespaceRegisterObject(xNamespace, (void *)xMutex, IPC_TYPE_MUTEX, pcMutexName) ==
            0U) {
            /* Registration failed - delete the mutex */
            vSemaphoreDelete(xMutex);
            xMutex = NULL;
        }
    }

    return xMutex;
}

/*-----------------------------------------------------------
 * PRIVATE FUNCTIONS (FOR FREERTOS KERNEL INTEGRATION)
 *----------------------------------------------------------*/

void prvIpcNamespaceTaskDelete(TaskHandle_t xTask) {
    /* Currently, we don't need to do anything special when a task is deleted
     * from an IPC namespace perspective. IPC objects (queues, semaphores, etc.)
     * are typically owned by the system and shared between tasks.
     *
     * In a more advanced implementation, we might:
     * - Clean up any task-specific IPC object registrations
     * - Remove the task from any namespace membership lists
     * - Handle orphaned IPC objects
     *
     * For now, we simply clear the task's IPC namespace handle which
     * will happen automatically when the TCB is cleaned up.
     */
    (void)xTask; /* Suppress unused parameter warning */
}

BaseType_t prvIpcNamespaceSetTaskNamespace(TaskHandle_t xTask, IpcNamespaceHandle_t xNamespace) {
    /* This function is intended for internal FreeRTOS kernel use.
     * It directly sets the IPC namespace for a task using the TCB function.
     */
    return xTaskSetIpcNamespace(xTask, xNamespace);
}

#endif /* configUSE_IPC_NAMESPACE */
