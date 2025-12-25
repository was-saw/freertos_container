/*
 * platform_config.h - 平台配置头文件
 * 
 * 定义网络接口的基地址和其他平台相关配置
 */

#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H

#include "xparameters.h"

/* 
 * 网络接口基地址配置
 * 根据你的硬件平台选择正确的宏
 */

/* ZynqMP (ZCU102等) - GEM接口 */
#if defined(XPAR_XEMACPS_0_BASEADDR)
#define PLATFORM_EMAC_BASEADDR  XPAR_XEMACPS_0_BASEADDR
#elif defined(XPAR_XEMACPS_1_BASEADDR)
#define PLATFORM_EMAC_BASEADDR  XPAR_XEMACPS_1_BASEADDR

/* Zynq-7000 - GEM接口 */
#elif defined(XPAR_PS7_ETHERNET_0_BASEADDR)
#define PLATFORM_EMAC_BASEADDR  XPAR_PS7_ETHERNET_0_BASEADDR
#elif defined(XPAR_PS7_ETHERNET_1_BASEADDR)
#define PLATFORM_EMAC_BASEADDR  XPAR_PS7_ETHERNET_1_BASEADDR

/* AXI Ethernet */
#elif defined(XPAR_AXI_ETHERNET_0_BASEADDR)
#define PLATFORM_EMAC_BASEADDR  XPAR_AXI_ETHERNET_0_BASEADDR

/* 默认 - 需要手动配置 */
#else
#warning "PLATFORM_EMAC_BASEADDR not automatically detected. Please define it manually."
#define PLATFORM_EMAC_BASEADDR  0
#endif

#endif /* PLATFORM_CONFIG_H */
