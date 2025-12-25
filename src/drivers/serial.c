/* 这里的代码逻辑适用于 ZCU102 (PSU UART) */
#include "serial.h"
#include "xparameters.h"
#include "xuartps.h"

// 静态驱动实例
static XUartPs xUartInstance;

xComPortHandle xSerialPortInitMinimal(unsigned long ulWantedBaud,
                                      unsigned portBASE_TYPE uxQueueLength) {
  (void) uxQueueLength;
  XUartPs_Config *Config;

  // 1. 查找配置
  Config = XUartPs_LookupConfig(XPAR_XUARTPS_0_BASEADDR);
  if (Config == NULL) {
    return NULL;
  }

  // 2. 不调用 CfgInitialize（它会重置硬件）
  //    只填充软件结构，复用 BSP 已有的硬件配置
  xUartInstance.Config = *Config;
  xUartInstance.IsReady = XIL_COMPONENT_IS_READY;

  // 3. 只设置波特率（如果需要的话，通常 BSP 已设置为 115200）
  if (ulWantedBaud != 115200) {
    XUartPs_SetBaudRate(&xUartInstance, ulWantedBaud);
  }

  return (xComPortHandle)&xUartInstance;
}

signed portBASE_TYPE xSerialGetChar(xComPortHandle pxPort,
                                    signed char *pcRxedChar,
                                    TickType_t xBlockTime) {
  (void) pxPort;
  (void) pcRxedChar;
  (void) xBlockTime;
  // 轮询或中断方式读取字符
  // 在简单的 CLI 实现中，可以使用 XUartPs_RecvByte
  if (XUartPs_IsReceiveData(xUartInstance.Config.BaseAddress)) {
    *pcRxedChar =
        XUartPs_ReadReg(xUartInstance.Config.BaseAddress, XUARTPS_FIFO_OFFSET);
    return pdTRUE;
  }
  return pdFALSE;
}

signed portBASE_TYPE xSerialPutChar(xComPortHandle pxPort, signed char cOutChar,
                                    TickType_t xBlockTime) {
  (void) pxPort;
  (void) xBlockTime;
  // 发送单个字符
  XUartPs_SendByte(xUartInstance.Config.BaseAddress, cOutChar);
  return pdTRUE;
}

void vSerialPutString(xComPortHandle pxPort, const signed char *const pcString,
                      unsigned short usStringLength) {
  (void) pxPort;
  unsigned short i;

  // 逐个字符发送字符串
  for (i = 0; i < usStringLength; i++) {
    XUartPs_SendByte(xUartInstance.Config.BaseAddress, pcString[i]);

    // 等待发送完成
    while (XUartPs_IsTransmitFull(xUartInstance.Config.BaseAddress))
      ;
  }
}

portBASE_TYPE xSerialWaitForSemaphore(xComPortHandle xPort) {
  (void)xPort;
  // 简化实现：在轮询模式下，此函数可以直接返回成功
  // 如果使用中断模式，这里需要等待信号量
  return pdTRUE;
}

void vSerialClose(xComPortHandle xPort) {
  (void)xPort;
  // 关闭串口：禁用UART
  // 在嵌入式系统中通常不需要关闭，但可以禁用中断
  XUartPs_DisableUart(&xUartInstance);
}