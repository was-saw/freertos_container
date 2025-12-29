/*
 * FreeRTOS PID Namespace Example
 * Demonstrates PID namespace functionality with task isolation
 */

#include "pidnamespace_example.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "pid_namespace.h"
#include "projdefs.h"
#include "task.h"

#if (configUSE_PID_NAMESPACE == 1)

/* Task handles */
static TaskHandle_t xNamespace1Task1 = NULL;
static TaskHandle_t xNamespace1Task2 = NULL;
static TaskHandle_t xNamespace2Task1 = NULL;
static TaskHandle_t xNamespace2Task2 = NULL;
static TaskHandle_t xMonitorTask = NULL;

/* Namespace handles */
static PidNamespaceHandle_t xNamespace1 = NULL;
static PidNamespaceHandle_t xNamespace2 = NULL;

/*-----------------------------------------------------------*/

/* Task in namespace 1 */
static void vNamespace1Task(void *pvParameters) {
    UBaseType_t ulCounter = 0;
    UBaseType_t ulVirtualPid;
    UBaseType_t ulRealPid;

    (void)pvParameters;

    /* Use new convenience functions */
    ulVirtualPid = uxGetPid();
    ulRealPid = uxGetRealPid();

    uart_puts("Namespace1 Task Started - Virtual PID: ");
    uart_puthex(ulVirtualPid);
    uart_puts(", Real PID: ");
    uart_puthex(ulRealPid);
    uart_puts("\r\n");

    for (;;) {
        ulCounter++;

        if (ulCounter % 2000 == 0) {
            // uart_puts("NS1 Task (vPID:");
            // uart_puthex(ulVirtualPid);
            // uart_puts(", rPID:");
            // uart_puthex(ulRealPid);
            // uart_puts(") Counter: ");
            // uart_puthex(ulCounter);
            // uart_puts("\r\n");
        }

        // /* Delay to prevent overwhelming output */
        if (ulCounter % 2000 == 0) {
            // vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

/*-----------------------------------------------------------*/

/* Task in namespace 2 */
static void vNamespace2Task(void *pvParameters) {
    UBaseType_t ulCounter = 0;
    UBaseType_t ulVirtualPid;
    UBaseType_t ulRealPid;

    (void)pvParameters;

    /* Use new convenience functions */
    ulVirtualPid = uxGetPid();
    ulRealPid = uxGetRealPid();

    uart_puts("Namespace2 Task Started - Virtual PID: ");
    uart_puthex(ulVirtualPid);
    uart_puts(", Real PID: ");
    uart_puthex(ulRealPid);
    uart_puts("\r\n");

    for (;;) {
        ulCounter++;

        if (ulCounter % 2500 == 0) {
            // uart_puts("NS2 Task (vPID:");
            // uart_puthex(ulVirtualPid);
            // uart_puts(", rPID:");
            // uart_puthex(ulRealPid);
            // uart_puts(") Counter: ");
            // uart_puthex(ulCounter);
            // uart_puts("\r\n");
        }

        /* Delay to prevent overwhelming output */
        if (ulCounter % 2500 == 0) {
            vTaskDelay(pdMS_TO_TICKS(1500));
        }
    }
}

/*-----------------------------------------------------------*/

/* Monitor task - demonstrates namespace isolation */
static void vNamespaceMonitorTask(void *pvParameters) {
    TickType_t   xLastWakeTime;
    UBaseType_t  ulTaskCount1, ulNextPid1, ulMaxPid1;
    UBaseType_t  ulTaskCount2, ulNextPid2, ulMaxPid2;
    TaskHandle_t xFoundTask1, xFoundTask2;

    (void)pvParameters;

    uart_puts("Namespace Monitor Task Started\r\n");
    xLastWakeTime = xTaskGetTickCount();


    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(3000));

        uart_puts("\r\n=== Namespace Status ===\r\n");

        /* Get namespace 1 information */
        if (xPidNamespaceGetInfo(xNamespace1, &ulTaskCount1, &ulNextPid1, &ulMaxPid1) == pdPASS) {
            uart_puts("Namespace1: Tasks=");
            uart_puthex(ulTaskCount1);
            uart_puts(", NextPID=");
            uart_puthex(ulNextPid1);
            uart_puts(", MaxPID=");
            uart_puthex(ulMaxPid1);
            uart_puts("\r\n");
        }

        /* Get namespace 2 information */
        if (xPidNamespaceGetInfo(xNamespace2, &ulTaskCount2, &ulNextPid2, &ulMaxPid2) == pdPASS) {
            uart_puts("Namespace2: Tasks=");
            uart_puthex(ulTaskCount2);
            uart_puts(", NextPID=");
            uart_puthex(ulNextPid2);
            uart_puts(", MaxPID=");
            uart_puthex(ulMaxPid2);
            uart_puts("\r\n");
        }

        /* Test namespace isolation - try to find tasks across namespaces */
        uart_puts("Testing isolation:\r\n");

        /* Try to find NS1 task from NS2 (should fail) */
        xFoundTask1 = xPidNamespaceFindTaskByVirtualPid(xNamespace2, 1);
        uart_puts("NS2 search for vPID 1: ");
        uart_puthex((UBaseType_t)xFoundTask1);

        /* Try to find NS2 task from NS1 (should fail) */
        xFoundTask2 = xPidNamespaceFindTaskByVirtualPid(xNamespace1, 1);
        uart_puts("NS1 search for vPID 1: ");
        uart_puthex((UBaseType_t)xFoundTask2);
        uart_puts(xFoundTask1 == xFoundTask2 ? " (EQUAL - BAD!)\r\n" : " (NOT-EQUAL - GOOD!)\r\n");

        uart_puts("========================\r\n\r\n");
    }
}

/*-----------------------------------------------------------*/

void vPidNamespaceExampleInit(void) {
    int result = pdPASS;
    uart_puts("Initializing PID Namespace Example...\r\n");

    /* Create namespaces */
    xNamespace1 = xPidNamespaceCreate("TestNS1");
    if (xNamespace1 == NULL) {
        uart_puts("Failed to create Namespace1\r\n");
        return;
    }
    uart_puts("Created Namespace1\r\n");

    xNamespace2 = xPidNamespaceCreate("TestNS2");
    if (xNamespace2 == NULL) {
        uart_puts("Failed to create Namespace2\r\n");
        xPidNamespaceDelete(xNamespace1);
        xNamespace1 = NULL;
        return;
    }
    uart_puts("Created Namespace2\r\n");

    /* Create tasks in namespace 1 */
    if (xTaskCreateInNamespace(xNamespace1, vNamespace1Task, "NS1Task1",
                               configMINIMAL_STACK_SIZE * 2, (void *)"Task1", tskIDLE_PRIORITY + 2,
                               &xNamespace1Task1) != pdPASS) {
        vPidNamespaceExampleCleanup();
        uart_puts("Failed to create NS1 Task1\r\n");
        return;
    }

    if (xTaskCreateInNamespace(xNamespace1, vNamespace1Task, "NS1Task2",
                               configMINIMAL_STACK_SIZE * 2, (void *)"Task2", tskIDLE_PRIORITY + 2,
                               &xNamespace1Task2) != pdPASS) {
        vPidNamespaceExampleCleanup();
        uart_puts("Failed to create NS1 Task2\r\n");
        return;
    }

    /* Create tasks in namespace 2 */
    if (xTaskCreateInNamespace(xNamespace2, vNamespace2Task, "NS2Task1",
                               configMINIMAL_STACK_SIZE * 2, (void *)"Task1", tskIDLE_PRIORITY + 2,
                               &xNamespace2Task1) != pdPASS) {
        vPidNamespaceExampleCleanup();
        uart_puts("Failed to create NS2 Task1\r\n");
        return;
    }
    result = xTaskCreateInNamespace(xNamespace2, vNamespace2Task, "NS2Task2",
                                    configMINIMAL_STACK_SIZE * 2, (void *)"Task2",
                                    tskIDLE_PRIORITY + 2, &xNamespace2Task2);
    if (result != pdPASS) {
        vPidNamespaceExampleCleanup();
        uart_puthex(result);
        uart_puts("Failed to create NS2 Task2\r\n");
        return;
    }

    /* Create monitor task (not in any namespace) */
    if (xTaskCreate(vNamespaceMonitorTask, "NSMonitor", configMINIMAL_STACK_SIZE * 3, NULL,
                    tskIDLE_PRIORITY + 3, &xMonitorTask) != pdPASS) {
        vPidNamespaceExampleCleanup();
        uart_puts("Failed to create NSMonitor Task\r\n");
        return;
    } else {
        uart_puts("Created NSMonitor Task\r\n");
    }

    uart_puts("PID Namespace Example Initialized!\r\n");
}

/*-----------------------------------------------------------*/

void vPidNamespaceExampleCleanup(void) {
    /* Remove tasks from namespaces */
    if (xNamespace1 != NULL) {
        if (xNamespace1Task1 != NULL) {
            xPidNamespaceRemoveTask(xNamespace1, xNamespace1Task1);
        }
        if (xNamespace1Task2 != NULL) {
            xPidNamespaceRemoveTask(xNamespace1, xNamespace1Task2);
        }
    }

    if (xNamespace2 != NULL) {
        if (xNamespace2Task1 != NULL) {
            xPidNamespaceRemoveTask(xNamespace2, xNamespace2Task1);
        }
        if (xNamespace2Task2 != NULL) {
            xPidNamespaceRemoveTask(xNamespace2, xNamespace2Task2);
        }
    }

    /* Delete namespaces */
    if (xNamespace1 != NULL) {
        xPidNamespaceDelete(xNamespace1);
        xNamespace1 = NULL;
    }

    if (xNamespace2 != NULL) {
        xPidNamespaceDelete(xNamespace2);
        xNamespace2 = NULL;
    }
}

/*-----------------------------------------------------------*/

/* Application-friendly API demonstration */
void vDemonstratePidApi(void) {
    UBaseType_t ulVirtualPid, ulRealPid;
    void       *pxCurrentNamespace;

    uart_puts("\r\n=== PID API Demonstration ===\r\n");

    /* Use simple API functions that applications should use */
    ulVirtualPid = uxGetPid();
    ulRealPid = uxGetRealPid();
    pxCurrentNamespace = pvGetPidNamespace();

    uart_puts("Current Task Information:\r\n");
    uart_puts("  Virtual PID: ");
    uart_puthex(ulVirtualPid);
    uart_puts("\r\n  Real PID: ");
    uart_puthex(ulRealPid);
    uart_puts("\r\n  Namespace: ");
    if (pxCurrentNamespace != NULL) {
        uart_puthex((UBaseType_t)pxCurrentNamespace);
    } else {
        uart_puts("NULL (no namespace)");
    }
    uart_puts("\r\n");

    uart_puts("This demonstrates the simple API that applications should use.\r\n");
    uart_puts("No need to get task handles or call complex functions.\r\n");
    uart_puts("============================\r\n\r\n");
}

#endif /* configUSE_PID_NAMESPACE == 1 */
