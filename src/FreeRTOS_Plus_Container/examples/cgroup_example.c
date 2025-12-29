/*
 * FreeRTOS CGroup Automatic Example
 * Demonstrates automatic CPU resource limiting through kernel integration
 *
 * This example shows how cgroups automatically limit CPU usage without
 * requiring application-level manual calls to check quotas.
 * The CPU limiting is now handled automatically by the FreeRTOS kernel.
 */

#include "cgroup_example.h"
#include "FreeRTOS.h"
#include "cgroup.h"
#include "portmacro.h"
#include "task.h"


#if (configUSE_CGROUPS == 1)

/* Task handles */
static TaskHandle_t xHighQuotaTask = NULL;
static TaskHandle_t xLowQuotaTask = NULL;
static TaskHandle_t xMonitorTask = NULL;
static TaskHandle_t xMemoryTestTask = NULL;

/* CGroup handles */
static CGroupHandle_t xHighQuotaCGroup = NULL;
static CGroupHandle_t xLowQuotaCGroup = NULL;

/*-----------------------------------------------------------*/

static UBaseType_t highCount = 0;
static UBaseType_t lowCount = 0;


/* High quota task - allowed to use 70% CPU automatically */
static void vHighQuotaTask(void *pvParameters) {
    UBaseType_t ulCounter = 0;

    for (;;) {
        highCount++;
        // if (highCount % 100000 == 0) {
        //     uart_puts("HighQuota Task Count: ");
        //     uart_puthex(highCount);
        //     uart_puts("\n");
        // }
    }
}

/*-----------------------------------------------------------*/

/* Low quota task - allowed to use 20% CPU automatically */
static void vLowQuotaTask(void *pvParameters) {
    UBaseType_t ulCounter = 0;

    for (;;) {
        lowCount++;
        // if (lowCount % 100000 == 0) {
        //     uart_puts("LowQuota Task Count: ");
        //     uart_puthex(lowCount);
        //     uart_puts("\n");
        // }
    }
}

/*-----------------------------------------------------------*/

/* Monitor task - reports cgroup statistics */
static void vMonitorTask(void *pvParameters) {
    MemoryLimits_t   xMemoryStats;
    CpuLimits_t      xCpuStats;
    TickType_t       xLastWakeTime;
    const TickType_t xMonitorFrequency =
        pdMS_TO_TICKS(500); /* Every 0.5 seconds - faster than window reset */
    BaseType_t last_count = 0;
    (void)pvParameters;

    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        if (highCount - last_count > 100) {
            last_count = highCount;
            uart_puts("Monitor Task Running\n");
            /* Get and display high quota cgroup statistics */
            if (xCGroupGetStats(xHighQuotaCGroup, &xMemoryStats, &xCpuStats) == pdPASS) {
                /* Display CPU and Memory statistics */
                volatile UBaseType_t ulHighQuotaUsed = xCpuStats.ulTicksUsed;
                volatile UBaseType_t ulHighQuotaLimit = xCpuStats.ulTicksQuota;
                volatile UBaseType_t ulHighPenalty = xCpuStats.ulPenaltyTicksLeft;
                volatile UBaseType_t ulHighMemUsed = xMemoryStats.ulMemoryUsed;
                volatile UBaseType_t ulHighMemLimit = xMemoryStats.ulMemoryLimit;
                volatile UBaseType_t ulHighMemPeak = xMemoryStats.ulMemoryPeak;

                uart_puts("HighQuota Task Stats:\n");
                uart_puts("CPU - Used: ");
                uart_puthex(ulHighQuotaUsed);
                uart_puts(", Limit: ");
                uart_puthex(ulHighQuotaLimit);
                uart_puts(", Penalty: ");
                uart_puthex(ulHighPenalty);
                uart_puts("\nMEM - Used: ");
                uart_puthex(ulHighMemUsed);
                uart_puts(", Limit: ");
                uart_puthex(ulHighMemLimit);
                uart_puts(", Peak: ");
                uart_puthex(ulHighMemPeak);
                uart_puts(", Count: ");
                uart_puthex(highCount);
                uart_puts("\n");
            }

            /* Get and display low quota cgroup statistics */
            if (xCGroupGetStats(xLowQuotaCGroup, &xMemoryStats, &xCpuStats) == pdPASS) {
                volatile UBaseType_t ulLowQuotaUsed = xCpuStats.ulTicksUsed;
                volatile UBaseType_t ulLowQuotaLimit = xCpuStats.ulTicksQuota;
                volatile UBaseType_t ulLowPenalty = xCpuStats.ulPenaltyTicksLeft;
                volatile UBaseType_t ulLowMemUsed = xMemoryStats.ulMemoryUsed;
                volatile UBaseType_t ulLowMemLimit = xMemoryStats.ulMemoryLimit;
                volatile UBaseType_t ulLowMemPeak = xMemoryStats.ulMemoryPeak;

                uart_puts("LowQuota Task Stats:\n");
                uart_puts("CPU - Used: ");
                uart_puthex(ulLowQuotaUsed);
                uart_puts(", Limit: ");
                uart_puthex(ulLowQuotaLimit);
                uart_puts(", Penalty: ");
                uart_puthex(ulLowPenalty);
                uart_puts("\nMEM - Used: ");
                uart_puthex(ulLowMemUsed);
                uart_puts(", Limit: ");
                uart_puthex(ulLowMemLimit);
                uart_puts(", Peak: ");
                uart_puthex(ulLowMemPeak);
                uart_puts(", Count: ");
                uart_puthex(lowCount);
                uart_puts("\n\n");
            }
            uart_puts("rate of high_task/low_task: ");
            uart_puthex(highCount / (lowCount + 1));
            uart_puts("\n");
        }
    }
}

/*-----------------------------------------------------------*/

/* Memory test task - tests memory allocation limits */
static void vMemoryTestTask(void *pvParameters) {
    UBaseType_t ulCounter = 0;
    (void)pvParameters;

    uart_puts("MemoryTest Task Started!\n");

    /* Test memory allocation after a few iterations */
    for (;;) {
        ulCounter++;

        if (ulCounter % 1000 == 0) {
            // uart_puts("MemTest: Running, counter = ");
            // uart_puthex(ulCounter);
            // uart_puts("\n");

            /* Try to allocate some memory to test cgroup limits */
            if (ulCounter == 2000) /* After 2000 iterations */
            {
                uart_puts("Testing memory allocation...\n");
                void *ptr1 = pvPortMalloc(512);
                void *ptr2 = pvPortMalloc(1024);
                void *ptr3 = pvPortMalloc(8192);

                uart_puts("Allocation results: ptr1=");
                uart_puthex((UBaseType_t)ptr1);
                uart_puts(", ptr2=");
                uart_puthex((UBaseType_t)ptr2);
                uart_puts(", ptr3=");
                uart_puthex((UBaseType_t)ptr3);
                uart_puts("\n");

                /* Check cgroup memory stats */
                TaskHandle_t   xCurrentTask = xTaskGetCurrentTaskHandle();
                CGroupHandle_t xCGroup = xCGroupGetTaskGroup(xCurrentTask);
                if (xCGroup != NULL) {
                    UBaseType_t ulUsed, ulLimit, ulPeak;
                    if (xCGroupGetMemoryInfo(xCGroup, &ulUsed, &ulLimit, &ulPeak) == pdPASS) {
                        uart_puts("CGroup Memory: Used=");
                        uart_puthex(ulUsed);
                        uart_puts(", Limit=");
                        uart_puthex(ulLimit);
                        uart_puts(", Peak=");
                        uart_puthex(ulPeak);
                        uart_puts("\n");
                    }
                }
            }
        }

        /* Simple delay to prevent overwhelming output */
        if (ulCounter % 1000 == 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

/*-----------------------------------------------------------*/

void vCGroupAutomaticExampleInit(void) {
    BaseType_t xReturn;

    uart_puts("Creating CGroups...\n");

    /* Create high quota cgroup (70% CPU, 16KB memory limit) */
    xHighQuotaCGroup = xCGroupCreate("HighQuota", 16384, 300);
    configASSERT(xHighQuotaCGroup != NULL);
    uart_puts("HighQuota CGroup created with 16KB memory limit\n");

    /* Create low quota cgroup (20% CPU, 8KB memory limit) */
    xLowQuotaCGroup = xCGroupCreate("LowQuota", 8192, 20);
    configASSERT(xLowQuotaCGroup != NULL);
    uart_puts("LowQuota CGroup created with 8KB memory limit\n");

    // /* Create the high quota task */
    xReturn = xTaskCreate(vHighQuotaTask, "HighQuotaAuto", configMINIMAL_STACK_SIZE * 2, NULL,
                          tskIDLE_PRIORITY + 2, &xHighQuotaTask);
    configASSERT(xReturn == pdPASS);

    // /* Create the low quota task */
    xReturn = xTaskCreate(vLowQuotaTask, "LowQuotaAuto", configMINIMAL_STACK_SIZE * 2, NULL,
                          tskIDLE_PRIORITY + 2, &xLowQuotaTask);
    configASSERT(xReturn == pdPASS);

    /* Create the monitor task */
    xReturn = xTaskCreate(vMonitorTask, "MonitorAuto", configMINIMAL_STACK_SIZE * 2, NULL,
                          tskIDLE_PRIORITY + 2, &xMonitorTask);
    configASSERT(xReturn == pdPASS);

    /* Create the memory test task */
    // uart_puts("Creating MemoryTest task...\n");
    // uart_puts("Task stack size will be: ");
    // uart_puthex(configMINIMAL_STACK_SIZE * 2 * sizeof(StackType_t));
    // uart_puts(" bytes\n");

    // xReturn = xTaskCreate(vMemoryTestTask, "MemoryTest", configMINIMAL_STACK_SIZE * 2, NULL,
    //                       tskIDLE_PRIORITY + 2, &xMemoryTestTask);
    // configASSERT(xReturn == pdPASS);
    // uart_puts("MemoryTest Task Created: ");
    // uart_puthex((UBaseType_t)xMemoryTestTask);
    // uart_puts("\n");

    // /* Add tasks to their respective cgroups */
    // /* Once added, the kernel will automatically enforce CPU limits */
    xReturn = xCGroupAddTask(xHighQuotaCGroup, xHighQuotaTask);
    configASSERT(xReturn == pdPASS);

    xReturn = xCGroupAddTask(xLowQuotaCGroup, xLowQuotaTask);
    configASSERT(xReturn == pdPASS);

    // uart_puts("Adding MemoryTest task to HighQuota CGroup...\n");
    // xReturn = xCGroupAddTask(xLowQuotaCGroup, xMemoryTestTask);
    // configASSERT(xReturn == pdPASS);
    uart_puts("Task added to CGroup successfully!\n");

    /* Monitor task is not added to any cgroup - it runs unrestricted */
}

/*-----------------------------------------------------------*/

void vCGroupAutomaticExampleCleanup(void) {
    /* Remove tasks from cgroups first */
    if (xHighQuotaCGroup != NULL && xHighQuotaTask != NULL) {
        xCGroupRemoveTask(xHighQuotaCGroup, xHighQuotaTask);
    }

    if (xLowQuotaCGroup != NULL && xLowQuotaTask != NULL) {
        xCGroupRemoveTask(xLowQuotaCGroup, xLowQuotaTask);
    }

    /* Delete tasks */
    if (xHighQuotaTask != NULL) {
        // vTaskDelete( xHighQuotaTask );
        xHighQuotaTask = NULL;
    }

    if (xLowQuotaTask != NULL) {
        // vTaskDelete( xLowQuotaTask );
        xLowQuotaTask = NULL;
    }

    if (xMonitorTask != NULL) {
        // vTaskDelete( xMonitorTask );
        xMonitorTask = NULL;
    }

    /* Delete cgroups */
    if (xHighQuotaCGroup != NULL) {
        xCGroupDelete(xHighQuotaCGroup);
        xHighQuotaCGroup = NULL;
    }

    if (xLowQuotaCGroup != NULL) {
        xCGroupDelete(xLowQuotaCGroup);
        xLowQuotaCGroup = NULL;
    }
}

#endif /* configUSE_CGROUPS == 1 */
