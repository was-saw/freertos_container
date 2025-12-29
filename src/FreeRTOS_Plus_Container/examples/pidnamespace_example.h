/*
 * FreeRTOS PID Namespace Example
 * Demonstrates PID namespace functionality with task isolation
 */

#ifndef PIDNAMESPACE_EXAMPLE_H
#define PIDNAMESPACE_EXAMPLE_H

#include "FreeRTOS.h"
#include "task.h"

#if (configUSE_PID_NAMESPACE == 1)

/**
 * @brief Initialize and demonstrate PID namespace functionality
 */
void vPidNamespaceExampleInit(void);

/**
 * @brief Clean up PID namespace example resources
 */
void vPidNamespaceExampleCleanup(void);

/**
 * @brief Demonstrate the simple PID API that applications should use
 */
void vDemonstratePidApi(void);

#else

    #define vPidNamespaceExampleInit()                                                             \
        do {                                                                                       \
        } while (0)
    #define vPidNamespaceExampleCleanup()                                                          \
        do {                                                                                       \
        } while (0)

#endif /* configUSE_PID_NAMESPACE */

#endif /* PIDNAMESPACE_EXAMPLE_H */
