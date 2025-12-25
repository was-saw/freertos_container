/*
 * echo_server.h - lwIP Raw API Echo Server 头文件
 */

#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

#include "lwip/tcp.h"
#include "lwip/err.h"

/* Echo服务器端口号 */
#define ECHO_SERVER_PORT    7

/*
 * echo_server_init - 初始化并启动echo服务器
 * 
 * 使用lwIP raw API创建TCP服务器，监听指定端口，
 * 将所有收到的数据原封不动地发回给客户端。
 */
void echo_server_init(void);

#endif /* ECHO_SERVER_H */
