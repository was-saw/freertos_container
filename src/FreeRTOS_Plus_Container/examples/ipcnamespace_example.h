/*
 * IPC Namespace Example Header
 * Demonstrates IPC namespace functionality and isolation
 */

#ifndef INC_IPCNAMESPACE_EXAMPLE_H
#define INC_IPCNAMESPACE_EXAMPLE_H

#include "FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#if (configUSE_IPC_NAMESPACE == 1)

/**
 * @brief Start the IPC namespace example
 * Creates namespaces, IPC objects, and tasks to demonstrate IPC isolation
 *
 * @return pdPASS on success, pdFAIL on failure
 */
BaseType_t xStartIpcNamespaceExample(void);

/**
 * @brief Stop the IPC namespace example
 * Cleans up all created resources including tasks, IPC objects, and namespaces
 */
void vStopIpcNamespaceExample(void);

#else /* configUSE_IPC_NAMESPACE == 0 */

    /* Provide stub functions when IPC namespaces are disabled */
    #define xStartIpcNamespaceExample() pdFAIL
    #define vStopIpcNamespaceExample()                                                             \
        do {                                                                                       \
        } while (0)

#endif /* configUSE_IPC_NAMESPACE */

#ifdef __cplusplus
}
#endif

#endif /* INC_IPCNAMESPACE_EXAMPLE_H */
