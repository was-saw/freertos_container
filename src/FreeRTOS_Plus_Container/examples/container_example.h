/*
 * Container example header
 * Copyright (C) 2025
 */

#ifndef CONTAINER_EXAMPLE_H
#define CONTAINER_EXAMPLE_H

/* Example container functions following cgroup and namespace patterns */
void vHighResourceContainer(void *pvParameters);
void vLowResourceContainer(void *pvParameters);
void vCommunicationContainer(void *pvParameters);

/* Legacy example container functions (for backward compatibility) */
void vExampleContainer1(void *pvParameters);
void vExampleContainer2(void *pvParameters);

/* Initialize example containers */
void vInitializeExampleContainers(void);

#endif /* CONTAINER_EXAMPLE_H */
