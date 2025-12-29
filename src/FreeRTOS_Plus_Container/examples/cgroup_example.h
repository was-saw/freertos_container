/*
 * CGroup Example Header
 * Demonstrates both manual and automatic cgroup integration
 */

#ifndef CGROUP_EXAMPLE_H
#define CGROUP_EXAMPLE_H

#include "FreeRTOS.h"
#include "task.h"

#if (configUSE_CGROUPS == 1)
/**
 * @brief Initialize the automatic cgroup example
 *
 * This creates tasks that are automatically limited by the kernel's
 * integrated cgroup system. No manual checking required by applications.
 */
void vCGroupAutomaticExampleInit(void);

/**
 * @brief Cleanup the automatic cgroup example
 */
void vCGroupAutomaticExampleCleanup(void);

#else
    #define vCGroupAutomaticExampleInit()                                                          \
        do {                                                                                       \
        } while (0)
    #define vCGroupAutomaticExampleCleanup()                                                       \
        do {                                                                                       \
        } while (0)

#endif /* configUSE_CGROUPS == 1 */

#endif /* CGROUP_EXAMPLE_H */
