/*
 * FreeRTOS + lwIP Raw API Echo Server
 * 
 * 使用FreeRTOS任务和lwIP raw API实现TCP echo服务器
 */

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/init.h"
#include "lwip/tcp.h"
#include "lwip/err.h"
#include "netif/xadapter.h"

#include "xparameters.h"
#include "xil_printf.h"
#include "platform_config.h"
#include "echo_server.h"

/* 安装 FreeRTOS 向量表的函数声明 (定义在 port_asm_vectors.S) */
extern void vPortInstallFreeRTOSVectorTable(void);

/* 网络配置 */
#define ECHO_SERVER_PORT    7

/* 任务配置 */
#define MAIN_TASK_STACK_SIZE    1024
#define MAIN_TASK_PRIORITY      (tskIDLE_PRIORITY + 2)

/* MAC 地址 - 请根据实际情况修改 */
static unsigned char mac_addr[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

/* 网络接口 */
static struct netif server_netif;

/* 前向声明 */
static void main_task(void *pvParameters);
static void network_init(void);
static err_t echo_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t echo_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t echo_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len);
static void echo_err_callback(void *arg, err_t err);
static void echo_close_connection(struct tcp_pcb *tpcb);

/*
 * main - 程序入口
 */
int main(void)
{
    xil_printf("\r\n--- FreeRTOS + lwIP Raw API Echo Server ---\r\n");

    /* 安装 FreeRTOS 向量表，替换 standalone BSP 的向量表 */
    vPortInstallFreeRTOSVectorTable();
    
    /* 创建主任务 */
    xTaskCreate(main_task, 
                "main_task", 
                MAIN_TASK_STACK_SIZE, 
                NULL, 
                MAIN_TASK_PRIORITY, 
                NULL);
    
    /* 启动调度器 */
    vTaskStartScheduler();
    
    /* 正常情况下不会运行到这里 */
    while (1);
    
    return 0;
}

/*
 * main_task - 主任务，负责初始化网络并启动echo服务器
 */
static void main_task(void *pvParameters)
{
    (void)pvParameters;
    
    /* 初始化网络 */
    network_init();
    
    /* 启动 echo 服务器 */
    echo_server_init();
    
    xil_printf("Echo server started on port %d\r\n", ECHO_SERVER_PORT);
    xil_printf("Use: telnet <board_ip> %d\r\n\r\n", ECHO_SERVER_PORT);
    
    /* 主循环 - 处理lwIP定时器和网络事件 */
    while (1) {
        /* 处理接收到的数据包 */
        xemacif_input(&server_netif);
        
        /* 适当延时，让其他任务有机会运行 */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/*
 * network_init - 初始化lwIP和网络接口
 */
static void network_init(void)
{
    ip_addr_t ipaddr, netmask, gw;
    
    /* 初始化 lwIP */
    lwip_init();
    
    /* 设置静态IP地址 */
    IP4_ADDR(&ipaddr,  192, 168, 1, 10);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw,      192, 168, 1, 1);
    
    xil_printf("Configuring network:\r\n");
    xil_printf("  IP Address : %d.%d.%d.%d\r\n", 
               ip4_addr1(&ipaddr), ip4_addr2(&ipaddr),
               ip4_addr3(&ipaddr), ip4_addr4(&ipaddr));
    xil_printf("  Netmask    : %d.%d.%d.%d\r\n",
               ip4_addr1(&netmask), ip4_addr2(&netmask),
               ip4_addr3(&netmask), ip4_addr4(&netmask));
    xil_printf("  Gateway    : %d.%d.%d.%d\r\n",
               ip4_addr1(&gw), ip4_addr2(&gw),
               ip4_addr3(&gw), ip4_addr4(&gw));
    
    /* 添加网络接口 */
    if (!xemac_add(&server_netif, &ipaddr, &netmask, &gw, 
                   mac_addr, PLATFORM_EMAC_BASEADDR)) {
        xil_printf("ERROR: Failed to add network interface\r\n");
        return;
    }
    
    /* 设置为默认网络接口 */
    netif_set_default(&server_netif);
    
    /* 启用网络接口 */
    netif_set_up(&server_netif);
    
    xil_printf("Network interface initialized\r\n\r\n");
}

/*
 * echo_server_init - 初始化TCP echo服务器（使用raw API）
 */
void echo_server_init(void)
{
    struct tcp_pcb *pcb;
    err_t err;
    
    /* 创建新的TCP控制块 */
    pcb = tcp_new();
    if (pcb == NULL) {
        xil_printf("ERROR: Failed to create TCP PCB\r\n");
        return;
    }
    
    /* 绑定到echo端口 */
    err = tcp_bind(pcb, IP_ADDR_ANY, ECHO_SERVER_PORT);
    if (err != ERR_OK) {
        xil_printf("ERROR: Failed to bind to port %d (err=%d)\r\n", 
                   ECHO_SERVER_PORT, err);
        tcp_close(pcb);
        return;
    }
    
    /* 开始监听 */
    pcb = tcp_listen(pcb);
    if (pcb == NULL) {
        xil_printf("ERROR: Failed to listen\r\n");
        return;
    }
    
    /* 设置接受连接的回调函数 */
    tcp_accept(pcb, echo_accept_callback);
    
    xil_printf("Echo server listening on port %d\r\n", ECHO_SERVER_PORT);
}

/*
 * echo_accept_callback - 接受新连接时的回调函数
 */
static err_t echo_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    (void)arg;
    
    if (err != ERR_OK || newpcb == NULL) {
        return ERR_VAL;
    }
    
    xil_printf("New connection accepted\r\n");
    
    /* 设置接收回调 */
    tcp_recv(newpcb, echo_recv_callback);
    
    /* 设置发送完成回调 */
    tcp_sent(newpcb, echo_sent_callback);
    
    /* 设置错误回调 */
    tcp_err(newpcb, echo_err_callback);
    
    /* 设置优先级 */
    tcp_setprio(newpcb, TCP_PRIO_MIN);
    
    return ERR_OK;
}

/*
 * echo_recv_callback - 接收数据时的回调函数
 * 将收到的数据原封不动地发回
 */
static err_t echo_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    (void)arg;
    
    /* 检查错误或连接关闭 */
    if (err != ERR_OK || p == NULL) {
        if (p != NULL) {
            pbuf_free(p);
        }
        echo_close_connection(tpcb);
        return ERR_OK;
    }
    
    /* 确认收到数据 */
    tcp_recved(tpcb, p->tot_len);
    
    /* Echo: 将收到的数据发回 */
    err = tcp_write(tpcb, p->payload, p->tot_len, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        xil_printf("ERROR: tcp_write failed (err=%d)\r\n", err);
        pbuf_free(p);
        echo_close_connection(tpcb);
        return err;
    }
    
    /* 立即发送 */
    err = tcp_output(tpcb);
    if (err != ERR_OK) {
        xil_printf("ERROR: tcp_output failed (err=%d)\r\n", err);
    }
    
    /* 释放pbuf */
    pbuf_free(p);
    
    return ERR_OK;
}

/*
 * echo_sent_callback - 数据发送完成时的回调函数
 */
static err_t echo_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    (void)arg;
    (void)tpcb;
    (void)len;
    
    /* 数据发送成功，这里可以添加额外处理 */
    return ERR_OK;
}

/*
 * echo_err_callback - 连接错误时的回调函数
 */
static void echo_err_callback(void *arg, err_t err)
{
    (void)arg;
    
    xil_printf("Connection error (err=%d)\r\n", err);
    /* 注意：发生错误时，PCB已经被lwIP释放，不需要手动关闭 */
}

/*
 * echo_close_connection - 关闭TCP连接
 */
static void echo_close_connection(struct tcp_pcb *tpcb)
{
    if (tpcb == NULL) {
        return;
    }
    
    xil_printf("Closing connection\r\n");
    
    /* 清除所有回调 */
    tcp_recv(tpcb, NULL);
    tcp_sent(tpcb, NULL);
    tcp_err(tpcb, NULL);
    
    /* 关闭连接 */
    tcp_close(tpcb);
}
