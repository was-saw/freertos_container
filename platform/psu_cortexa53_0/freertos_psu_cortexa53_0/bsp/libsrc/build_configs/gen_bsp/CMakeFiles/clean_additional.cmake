# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "/home/was/code/FreeRTOS_Container/platform/psu_cortexa53_0/freertos_psu_cortexa53_0/bsp/include/sleep.h"
  "/home/was/code/FreeRTOS_Container/platform/psu_cortexa53_0/freertos_psu_cortexa53_0/bsp/include/xiltimer.h"
  "/home/was/code/FreeRTOS_Container/platform/psu_cortexa53_0/freertos_psu_cortexa53_0/bsp/include/xtimer_config.h"
  "/home/was/code/FreeRTOS_Container/platform/psu_cortexa53_0/freertos_psu_cortexa53_0/bsp/lib/libxiltimer.a"
  )
endif()
