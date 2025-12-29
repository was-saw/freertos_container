/*
 * Container example implementation
 * Copyright (C) 2025
 *
 * This example demonstrates container creation with proper isolation
 * using CGroups and Namespaces, following the patterns from other examples.
 *
 * NOTE: The container functions below (vHighResourceContainer, vLowResourceContainer, etc.)
 * are kept for reference and legacy compatibility. The new container system uses
 * image-based containers where container logic is loaded from ELF files, not
 * function pointers.
 *
 * To use these examples with the new system:
 * 1. Compile the container logic into separate ELF executables
 * 2. Pack them into container images using container-save command
 * 3. Load images using container-load command
 * 4. Create containers from images using container-create command
 */

#include "FreeRTOS.h"
#include "container.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "xil_printf.h"
#include <stdint.h>

/* Example container function 1 - High resource usage
 * NOTE: This is kept for reference. In the new system, this would be
 * compiled as a standalone ELF and loaded from an image. */
void vHighResourceContainer(void *pvParameters) {
    TickType_t       xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); /* Run every 1 second */
    UBaseType_t      ulCounter = 0;

    (void)pvParameters;

    xil_printf("High Resource Container Started\r\n");

#if (configUSE_PID_NAMESPACE == 1)
    /* Show PID namespace information - following pidnamespace_example pattern */
    UBaseType_t ulVirtualPid = uxGetPid();
    UBaseType_t ulRealPid = uxGetRealPid();
    xil_printf("Container PID info - Virtual: %lu, Real: %lu\r\n",
        (unsigned long)ulVirtualPid, (unsigned long)ulRealPid);
#endif

    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        ulCounter++;

        /* Simulate high CPU usage */
        volatile uint32_t i;
        for (i = 0; i < 100000; i++) { /* CPU intensive loop */
        }

        /* Test memory allocation - following cgroup_example pattern */
        if (ulCounter % 5 == 0) {
            void *ptr = pvPortMalloc(1024); /* Try to allocate 1KB */
            xil_printf("HighRes Container: Allocated 1KB at 0x%08lx\r\n",
                (unsigned long)ptr);

/* Check CGroup memory stats if available */
#if (configUSE_CGROUPS == 1)
            TaskHandle_t   xCurrentTask = xTaskGetCurrentTaskHandle();
            CGroupHandle_t xCGroup = xCGroupGetTaskGroup(xCurrentTask);
            if (xCGroup != NULL) {
                UBaseType_t ulUsed, ulLimit, ulPeak;
                if (xCGroupGetMemoryInfo(xCGroup, &ulUsed, &ulLimit, &ulPeak) == pdPASS) {
                    xil_printf("  CGroup Memory: Used=%lu, Limit=%lu, Peak=%lu\r\n",
                        (unsigned long)ulUsed, (unsigned long)ulLimit, (unsigned long)ulPeak);
                }
            }
#endif
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/* Example container function 2 - Low resource usage */
void vLowResourceContainer(void *pvParameters) {
    TickType_t       xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(2000); /* Run every 2 seconds */
    UBaseType_t      ulCounter = 0;
    QueueHandle_t    xLocalQueue = NULL;

    (void)pvParameters;

    xil_printf("Low Resource Container Started\r\n");

#if (configUSE_PID_NAMESPACE == 1)
    /* Show PID namespace information */
    UBaseType_t ulVirtualPid = uxGetPid();
    UBaseType_t ulRealPid = uxGetRealPid();
    xil_printf("Container PID info - Virtual: %lu, Real: %lu\r\n",
        (unsigned long)ulVirtualPid, (unsigned long)ulRealPid);
#endif

/* Create IPC objects using isolated APIs - following ipcnamespace_example pattern */
#if (configUSE_IPC_NAMESPACE == 1)
    xLocalQueue = xQueueCreateIsolated(5, sizeof(uint32_t), "ContainerQueue");
    if (xLocalQueue != NULL) {
        xil_printf("LowRes Container: Created isolated queue\r\n");
    }
#endif

    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        ulCounter++;

        /* Light CPU usage */
        xil_printf("LowRes Container running, counter: %lu\r\n", (unsigned long)ulCounter);

/* Test queue operations if available */
#if (configUSE_IPC_NAMESPACE == 1)
        if (xLocalQueue != NULL) {
            uint32_t ulData = ulCounter;
            if (xQueueSend(xLocalQueue, &ulData, pdMS_TO_TICKS(10)) == pdPASS) {
                xil_printf("  Sent data to isolated queue\r\n");
            }
        }
#endif

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/* Example container function 3 - Communication test */
void vCommunicationContainer(void *pvParameters) {
    TickType_t        xLastWakeTime;
    const TickType_t  xFrequency = pdMS_TO_TICKS(3000); /* Run every 3 seconds */
    UBaseType_t       ulCounter = 0;
    SemaphoreHandle_t xLocalSemaphore = NULL;

    (void)pvParameters;

    xil_printf("Communication Container Started\r\n");

/* Create IPC objects - following ipcnamespace_example pattern */
#if (configUSE_IPC_NAMESPACE == 1)
    xLocalSemaphore = xSemaphoreCreateBinaryIsolated("ContainerSem");
    if (xLocalSemaphore != NULL) {
        xSemaphoreGive(xLocalSemaphore);
        xil_printf("Comm Container: Created isolated semaphore\r\n");
    }
#endif

    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        ulCounter++;

        xil_printf("Comm Container running, counter: %lu\r\n", (unsigned long)ulCounter);

/* Test semaphore operations */
#if (configUSE_IPC_NAMESPACE == 1)
        if (xLocalSemaphore != NULL) {
            if (xSemaphoreTake(xLocalSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
                xil_printf("  Acquired isolated semaphore\r\n");
                vTaskDelay(pdMS_TO_TICKS(50));
                xSemaphoreGive(xLocalSemaphore);
                xil_printf("  Released isolated semaphore\r\n");
            }
        }
#endif

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/* Initialize example containers - demonstrating proper cgroup and namespace usage */
void vInitializeExampleContainers(void) {
    xContainerManagerInit();
    vRegisterContainerCLICommands();
    xil_printf("Initializing Container Examples...\r\n");

    xil_printf("Container Examples Initialized!\r\n");
    xil_printf("Use CLI commands to manage containers:\r\n");
    xil_printf("  container-ls              - List all containers\r\n");
    xil_printf("  container-start <id>      - Start container\r\n");
    xil_printf("  container-stop <id>       - Stop container\r\n");
    xil_printf("  container-create <image>  - Create from image\r\n");
    xil_printf("  container-run <name> <program>- Create and run\r\n");
    xil_printf("  container-delete <id>     - Delete container\r\n");
    xil_printf("  container-load <path>     - Load image file\r\n");
    xil_printf("  container-save <id> <path>- Save container to image\r\n");
    xil_printf("  container-image           - List all images\r\n");
}

/* Legacy example container functions for backward compatibility */
void vExampleContainer1(void *pvParameters) {
    /* Just redirect to the new low resource container */
    vLowResourceContainer(pvParameters);
}

void vExampleContainer2(void *pvParameters) {
    /* Just redirect to the new communication container */
    vCommunicationContainer(pvParameters);
}
