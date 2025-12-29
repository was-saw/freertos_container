/*
 * IPC Namespace Example for FreeRTOS
 * Demonstrates IPC namespace functionality and isolation
 *
 * This example shows:
 * 1. Creating IPC namespaces
 * 2. Assigning tasks to different namespaces
 * 3. IPC object isolation between namespaces
 * 4. Cross-namespace communication restrictions
 */

#include "ipcnamespace_example.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "ipc_namespace.h"
#include "portmacro.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#if (configUSE_IPC_NAMESPACE == 1)

/* Task handles */
static TaskHandle_t xNamespaceATask = NULL;
static TaskHandle_t xNamespaceBTask = NULL;
static TaskHandle_t xRootTask = NULL;
static TaskHandle_t xIpcMonitorTask = NULL;

/* IPC Namespace handles */
static IpcNamespaceHandle_t xNamespaceA = NULL;
static IpcNamespaceHandle_t xNamespaceB = NULL;

/* IPC Objects for testing isolation */
static QueueHandle_t     xQueueA = NULL;
static QueueHandle_t     xQueueB = NULL;
static QueueHandle_t     xQueueUnregistered = NULL; /* For testing unregistered object access */
static SemaphoreHandle_t xSemaphoreA = NULL;
static SemaphoreHandle_t xSemaphoreB = NULL;

/*-----------------------------------------------------------*/

/* Task running in Namespace A */
static void vNamespaceATask(void *pvParameters) {
    uint32_t ulCounter = 0;
    uint32_t ulData;

    (void)pvParameters; /* Suppress unused parameter warning */

    uart_puts("Namespace A Task started\n");

    /* Set this task to run in Namespace A */
    if (xIpcNamespaceSetTaskNamespace(NULL, xNamespaceA) != pdPASS) {
        uart_puts("ERROR: Failed to set task to Namespace A\n");
        /* vTaskDelete( NULL ); */
        return;
    }
    uart_puts("Namespace A Task: Set to Namespace A\n");

    /* Create IPC objects in Namespace A using isolated APIs */
    xQueueA = xQueueCreateIsolated(5, sizeof(uint32_t), "QueueA");
    xSemaphoreA = xSemaphoreCreateBinaryIsolated("SemaphoreA");

    if (xQueueA == NULL || xSemaphoreA == NULL) {
        uart_puts("ERROR: Failed to create IPC objects in Namespace A\n");
        /* vTaskDelete( NULL ); */
        return;
    }

    /* Give semaphore initially */
    xSemaphoreGive(xSemaphoreA);
    uart_puts("Namespace A Task: Created Queue A and Semaphore A\n");

    for (;;) {
        ulCounter++;

        /* Try to send data to Queue A (should succeed) */
        ulData = ulCounter;
        if (xQueueSend(xQueueA, &ulData, pdMS_TO_TICKS(10)) == pdPASS) {
            uart_puts("Namespace A: Sent data to Queue A\n");
        }

        /* Try to take Semaphore A (should succeed) */
        if (xSemaphoreTake(xSemaphoreA, pdMS_TO_TICKS(10)) == pdTRUE) {
            uart_puts("Namespace A: Acquired Semaphore A\n");
            vTaskDelay(pdMS_TO_TICKS(50));
            xSemaphoreGive(xSemaphoreA);
            uart_puts("Namespace A: Released Semaphore A\n");
        }

        /* Wait for Namespace B to create its objects before testing cross-namespace access */
        if (ulCounter > 3 && xQueueB != NULL) {
            /* Try to access Queue B (this should be blocked by namespace isolation) */
            uart_puts("Namespace A: Attempting to access Queue B (should be blocked)...\n");
            ulData = ulCounter + 2000;
            if (xQueueSend(xQueueB, &ulData, pdMS_TO_TICKS(10)) == pdPASS) {
                uart_puts(
                    "Namespace A: WARNING - Successfully sent to Queue B (isolation breach!)\n");
            } else {
                uart_puts("Namespace A: GOOD - Access to Queue B blocked (isolation working)\n");
            }

            /* Try to access Semaphore B (should also be blocked) */
            uart_puts("Namespace A: Attempting to access Semaphore B (should be blocked)...\n");
            if (xSemaphoreTake(xSemaphoreB, pdMS_TO_TICKS(10)) == pdTRUE) {
                uart_puts("Namespace A: WARNING - Successfully acquired Semaphore B (isolation "
                          "breach!)\n");
                xSemaphoreGive(xSemaphoreB);
            } else {
                uart_puts(
                    "Namespace A: GOOD - Access to Semaphore B blocked (isolation working)\n");
            }
        }

        /* Test access to unregistered queue (should succeed) */
        if (xQueueUnregistered != NULL) {
            uart_puts("Namespace A: Accessing unregistered queue (should succeed)...\n");
            ulData = ulCounter + 3000;
            if (xQueueSend(xQueueUnregistered, &ulData, pdMS_TO_TICKS(10)) == pdPASS) {
                uart_puts("Namespace A: GOOD - Successfully sent to unregistered queue\n");
            } else {
                uart_puts("Namespace A: ERROR - Failed to access unregistered queue\n");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); /* Wait 2 seconds */
    }
}

/* Task running in Namespace B */
static void vNamespaceBTask(void *pvParameters) {
    uint32_t ulCounter = 0;
    uint32_t ulData;

    (void)pvParameters; /* Suppress unused parameter warning */

    uart_puts("Namespace B Task started\n");

    /* Set this task to run in Namespace B */
    if (xIpcNamespaceSetTaskNamespace(NULL, xNamespaceB) != pdPASS) {
        uart_puts("ERROR: Failed to set task to Namespace B\n");
        /* vTaskDelete( NULL ); */
        return;
    }
    uart_puts("Namespace B Task: Set to Namespace B\n");

    /* Create IPC objects in Namespace B using isolated APIs */
    xQueueB = xQueueCreateIsolated(5, sizeof(uint32_t), "QueueB");
    xSemaphoreB = xSemaphoreCreateBinaryIsolated("SemaphoreB");

    if (xQueueB == NULL || xSemaphoreB == NULL) {
        uart_puts("ERROR: Failed to create IPC objects in Namespace B\n");
        /* vTaskDelete( NULL ); */
        return;
    }

    /* Give semaphore initially */
    xSemaphoreGive(xSemaphoreB);
    uart_puts("Namespace B Task: Created Queue B and Semaphore B\n");

    for (;;) {
        ulCounter++;

        /* Try to send data to Queue B (should succeed) */
        ulData = ulCounter + 1000;
        if (xQueueSend(xQueueB, &ulData, pdMS_TO_TICKS(10)) == pdPASS) {
            uart_puts("Namespace B: Sent data to Queue B\n");
        }

        /* Try to take Semaphore B (should succeed) */
        if (xSemaphoreTake(xSemaphoreB, pdMS_TO_TICKS(10)) == pdTRUE) {
            uart_puts("Namespace B: Acquired Semaphore B\n");
            vTaskDelay(pdMS_TO_TICKS(50));
            xSemaphoreGive(xSemaphoreB);
            uart_puts("Namespace B: Released Semaphore B\n");
        }

        /* Wait for Namespace A to create its objects before testing cross-namespace access */
        if (ulCounter > 3 && xQueueA != NULL) {
            /* Try to access Queue A (this should be blocked by namespace isolation) */
            uart_puts("Namespace B: Attempting to access Queue A (should be blocked)...\n");
            if (xQueueSend(xQueueA, &ulData, pdMS_TO_TICKS(10)) == pdPASS) {
                uart_puts(
                    "Namespace B: WARNING - Successfully sent to Queue A (isolation breach!)\n");
            } else {
                uart_puts("Namespace B: GOOD - Access to Queue A blocked (isolation working)\n");
            }

            /* Try to access Semaphore A (should also be blocked) */
            uart_puts("Namespace B: Attempting to access Semaphore A (should be blocked)...\n");
            if (xSemaphoreTake(xSemaphoreA, pdMS_TO_TICKS(10)) == pdTRUE) {
                uart_puts("Namespace B: WARNING - Successfully acquired Semaphore A (isolation "
                          "breach!)\n");
                xSemaphoreGive(xSemaphoreA);
            } else {
                uart_puts(
                    "Namespace B: GOOD - Access to Semaphore A blocked (isolation working)\n");
            }
        }

        /* Test access to unregistered queue (should succeed) */
        if (xQueueUnregistered != NULL) {
            uart_puts("Namespace B: Accessing unregistered queue (should succeed)...\n");
            if (xQueueReceive(xQueueUnregistered, &ulData, pdMS_TO_TICKS(10)) == pdPASS) {
                uart_puts("Namespace B: GOOD - Successfully received from unregistered queue\n");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2500)); /* Wait 2.5 seconds */
    }
}

/* Task running in root namespace (no isolation) */
static void vRootTask(void *pvParameters) {
    uint32_t ulData;

    (void)pvParameters; /* Suppress unused parameter warning */

    uart_puts("Root Task started (no namespace isolation)\n");

    for (;;) {
        uart_puts("Root Task: Verifying access to all namespaced objects...\n");

        /* Root task should be able to access both queues */
        if (xQueueReceive(xQueueA, &ulData, pdMS_TO_TICKS(100)) == pdPASS) {
            uart_puts("Root Task: Received data from Queue A: ");
            uart_puthex(ulData);
            uart_puts("\n");
        } else {
            uart_puts("Root Task: No data available in Queue A\n");
        }

        if (xQueueReceive(xQueueB, &ulData, pdMS_TO_TICKS(100)) == pdPASS) {
            uart_puts("Root Task: Received data from Queue B: ");
            uart_puthex(ulData);
            uart_puts("\n");
        } else {
            uart_puts("Root Task: No data available in Queue B\n");
        }

        /* Test access to semaphores */
        uart_puts("Root Task: Testing semaphore access...\n");
        if (xSemaphoreTake(xSemaphoreA, pdMS_TO_TICKS(100)) == pdTRUE) {
            uart_puts("Root Task: Successfully acquired Semaphore A\n");
            vTaskDelay(pdMS_TO_TICKS(20));
            xSemaphoreGive(xSemaphoreA);
            uart_puts("Root Task: Released Semaphore A\n");
        }

        if (xSemaphoreTake(xSemaphoreB, pdMS_TO_TICKS(100)) == pdTRUE) {
            uart_puts("Root Task: Successfully acquired Semaphore B\n");
            vTaskDelay(pdMS_TO_TICKS(20));
            xSemaphoreGive(xSemaphoreB);
            uart_puts("Root Task: Released Semaphore B\n");
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/* Monitor task to show IPC namespace status */
static void vIpcMonitorTask(void *pvParameters) {
    UBaseType_t uxObjectCountA, uxObjectCountB;
    UBaseType_t uxNextObjectIdA, uxNextObjectIdB;

    (void)pvParameters; /* Suppress unused parameter warning */

    uart_puts("IPC Monitor Task started\n");

    for (;;) {
        uart_puts("\n=== IPC Namespace Status ===\n");

        /* Get namespace information */
        if (xIpcNamespaceGetInfo(xNamespaceA, &uxObjectCountA, &uxNextObjectIdA) == pdPASS) {
            uart_puts("Namespace A - Objects: ");
            uart_puthex(uxObjectCountA);
            uart_puts(", Next ID: ");
            uart_puthex(uxNextObjectIdA);
            uart_puts("\n");
        }

        if (xIpcNamespaceGetInfo(xNamespaceB, &uxObjectCountB, &uxNextObjectIdB) == pdPASS) {
            uart_puts("Namespace B - Objects: ");
            uart_puthex(uxObjectCountB);
            uart_puts(", Next ID: ");
            uart_puthex(uxNextObjectIdB);
            uart_puts("\n");
        }

        /* Check task namespace assignments */
        uart_puts("Task Namespace Assignments:\n");
        uart_puts("  Namespace A Task: ");
        if (pvTaskGetIpcNamespace(xNamespaceATask) != NULL) {
            uart_puts("Assigned");
        } else {
            uart_puts("Root");
        }
        uart_puts("\n  Namespace B Task: ");
        if (pvTaskGetIpcNamespace(xNamespaceBTask) != NULL) {
            uart_puts("Assigned");
        } else {
            uart_puts("Root");
        }
        uart_puts("\n  Root Task: ");
        if (pvTaskGetIpcNamespace(xRootTask) != NULL) {
            uart_puts("Assigned");
        } else {
            uart_puts("Root");
        }
        uart_puts("\n");

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/*-----------------------------------------------------------*/

BaseType_t xStartIpcNamespaceExample(void) {
    BaseType_t xReturn = pdPASS;

    uart_puts("Starting IPC Namespace Example...\n");

    /* Initialize IPC namespace system */
    vIpcNamespaceInit();

    /* Create IPC namespaces */
    xNamespaceA = xIpcNamespaceCreate("NamespaceA");
    xNamespaceB = xIpcNamespaceCreate("NamespaceB");

    if (xNamespaceA == NULL || xNamespaceB == NULL) {
        uart_puts("ERROR: Failed to create IPC namespaces\n");
        return pdFAIL;
    }

    uart_puts("Created IPC namespaces A and B\n");

    /* Create an unregistered queue for testing (created by root/system) */
    xQueueUnregistered = xQueueCreate(5, sizeof(uint32_t));
    if (xQueueUnregistered == NULL) {
        uart_puts("ERROR: Failed to create unregistered queue\n");
        return pdFAIL;
    }
    uart_puts("Created unregistered queue for testing\n");

    /* Create tasks - they will create their own IPC objects within their namespaces */
    xReturn &= xTaskCreate(vNamespaceATask, "NSA_Task", configMINIMAL_STACK_SIZE, NULL,
                           tskIDLE_PRIORITY + 1, &xNamespaceATask);

    xReturn &= xTaskCreate(vNamespaceBTask, "NSB_Task", configMINIMAL_STACK_SIZE, NULL,
                           tskIDLE_PRIORITY + 1, &xNamespaceBTask);

    xReturn &= xTaskCreate(vRootTask, "Root_Task", configMINIMAL_STACK_SIZE, NULL,
                           tskIDLE_PRIORITY + 1, &xRootTask);

    xReturn &= xTaskCreate(vIpcMonitorTask, "IPC_Monitor", configMINIMAL_STACK_SIZE, NULL,
                           tskIDLE_PRIORITY + 2, &xIpcMonitorTask);

    if (xReturn != pdPASS) {
        uart_puts("ERROR: Failed to create tasks\n");
        return pdFAIL;
    }

    /* Assign tasks to namespaces */
    if (xTaskSetIpcNamespace(xNamespaceATask, xNamespaceA) != pdPASS) {
        uart_puts("ERROR: Failed to assign task to Namespace A\n");
        return pdFAIL;
    }

    if (xTaskSetIpcNamespace(xNamespaceBTask, xNamespaceB) != pdPASS) {
        uart_puts("ERROR: Failed to assign task to Namespace B\n");
        return pdFAIL;
    }

    /* Root task remains in root namespace (no assignment needed) */

    uart_puts("IPC Namespace Example started successfully\n");
    uart_puts("Tasks created and assigned to namespaces\n");

    return pdPASS;
}

void vStopIpcNamespaceExample(void) {
    /* Delete tasks */
    if (xNamespaceATask != NULL) {
        // vTaskDelete( xNamespaceATask );
        xNamespaceATask = NULL;
    }

    if (xNamespaceBTask != NULL) {
        // vTaskDelete( xNamespaceBTask );
        xNamespaceBTask = NULL;
    }

    if (xRootTask != NULL) {
        // vTaskDelete( xRootTask );
        xRootTask = NULL;
    }

    if (xIpcMonitorTask != NULL) {
        // vTaskDelete( xIpcMonitorTask );
        xIpcMonitorTask = NULL;
    }

    /* Delete IPC objects */
    if (xQueueA != NULL) {
        vQueueDelete(xQueueA);
        xQueueA = NULL;
    }

    if (xQueueB != NULL) {
        vQueueDelete(xQueueB);
        xQueueB = NULL;
    }

    if (xQueueUnregistered != NULL) {
        vQueueDelete(xQueueUnregistered);
        xQueueUnregistered = NULL;
    }

    if (xSemaphoreA != NULL) {
        vSemaphoreDelete(xSemaphoreA);
        xSemaphoreA = NULL;
    }

    if (xSemaphoreB != NULL) {
        vSemaphoreDelete(xSemaphoreB);
        xSemaphoreB = NULL;
    }

    /* Delete namespaces */
    if (xNamespaceA != NULL) {
        xIpcNamespaceDelete(xNamespaceA);
        xNamespaceA = NULL;
    }

    if (xNamespaceB != NULL) {
        xIpcNamespaceDelete(xNamespaceB);
        xNamespaceB = NULL;
    }

    uart_puts("IPC Namespace Example stopped\n");
}

#endif /* configUSE_IPC_NAMESPACE == 1 */
