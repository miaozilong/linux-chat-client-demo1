#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <gtk/gtk.h>
#define SERVER_NAME "s1"      // 服务器主机名（非IP地址）
#define SERVER_PORT 8080      // 服务器端口号
#define BUFFER_SIZE 1024      // 缓冲区大小，用于发送和接收数据

void *receive_messages(void *arg) {
    int sock = *(int *) arg;
    char buffer[BUFFER_SIZE];

    for (;;) {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            printf("与服务器断开连接。\n");
            break;
        }
        printf("\n[服务器]: %s\n> ", buffer);
        fflush(stdout);
    }

    return NULL;
}

int main(int argc, char **argv) {

    int sock = -1;                    // 客户端 socket 描述符，初始化为 -1 表示未创建
    struct addrinfo hints, *res, *p; // 地址信息结构体，用于解析主机名
    char buffer[BUFFER_SIZE];         // 存储用户输入的缓冲区

    // 1. 初始化 hints 结构体，指定需要 IPv4 和 TCP 套接字
    memset(&hints, 0, sizeof(hints));  // 清空结构体内容
    hints.ai_family = AF_INET;          // IPv4
    hints.ai_socktype = SOCK_STREAM;    // TCP 连接（流式套接字）

    // 2. 将端口号从整数转换成字符串，供 getaddrinfo 使用
    char port_str[6];                  // 端口号字符串，最大5位数字加终止符
    snprintf(port_str, sizeof(port_str), "%d", SERVER_PORT);

    // 3. 调用 getaddrinfo 解析主机名和端口，获取服务器的地址信息链表
    int status = getaddrinfo(SERVER_NAME, port_str, &hints, &res);
    if (status != 0) {  // 解析失败时返回错误信息
        fprintf(stderr, "getaddrinfo 错误: %s\n", gai_strerror(status));
        return 1;       // 结束程序
    }

    // 4. 遍历 getaddrinfo 返回的所有地址，尝试逐个连接
    for (p = res; p != NULL; p = p->ai_next) {
        // 创建 socket
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock == -1) {                // 创建失败则打印错误，尝试下一个地址
            perror("socket 创建失败");
            continue;
        }

        // 尝试连接服务器
        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) {
            // 连接成功，跳出循环
            break;
        }

        // 连接失败，关闭当前 socket，继续尝试下一个地址
        close(sock);
        sock = -1;
    }

    // 5. 释放 getaddrinfo 分配的内存
    freeaddrinfo(res);

    // 6. 如果所有地址都连接失败，则打印错误，退出程序
    if (sock == -1) {
        fprintf(stderr, "无法连接到服务器 %s:%d\n", SERVER_NAME, SERVER_PORT);
        return 1;
    }

    printf("已连接到服务器 %s:%d\n", SERVER_NAME, SERVER_PORT);
    printf("请输入消息，按回车发送，输入 'exit' 退出。\n");
    fflush(stdout);

    // 创建接收消息的线程
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_messages, &sock) != 0) {
        perror("接收线程创建失败");
        close(sock);
        return 1;
    }

    // 7. 主循环，读取用户输入，发送给服务器
    while (1) {
        printf("> ");                   // 显示提示符
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            // 读取失败（如EOF），退出循环
            printf("读取输入失败\n");
            break;
        }

        // 8. 去除 fgets 读取到的字符串末尾换行符
        buffer[strcspn(buffer, "\n")] = 0;

        // 9. 判断是否输入退出命令
        if (strcmp(buffer, "exit") == 0) {
            printf("退出客户端\n");
            break;                     // 跳出循环，结束程序
        }

        // 10. 发送数据到服务器
        ssize_t sent = send(sock, buffer, strlen(buffer), 0);
        if (sent < 0) {
            perror("发送失败");
            break;                     // 发送失败，退出循环
        }
    }

    // 11. 关闭 socket，释放资源
    close(sock);

    return status;
}
