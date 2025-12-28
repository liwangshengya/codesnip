#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>

// ===================================================================================
// 全局变量定义 (已简化)
// ===================================================================================

const int BUF_SIZE = 1024; // 消息缓冲区的大小
// 注意：不再需要 MAX_CLNT, clnt_count, clnt_socks, mtx

// ===================================================================================
// 辅助函数定义
// ===================================================================================

void error_handling(std::string message) {
    std::cout << message << std::endl;
    exit(1);
}

// 注意：不再需要 send_msg_to_all 函数

/**
 * @brief 处理单个客户端通信的线程函数 (已修改)
 * @param clnt_sock 该线程负责的客户端的套接字文件描述符
 * 现在这个函数只做一件事：接收客户端消息，然后原样发回。
 */
void handle_client(int clnt_sock) {
    char message[BUF_SIZE];
    int str_len;

    // 循环从客户端套接字读取数据
    while ((str_len = read(clnt_sock, message, sizeof(message))) > 0) {
        // 【核心修改】: 将收到的数据原封不动地写回给同一个客户端
        write(clnt_sock, message, str_len);   
    }

    // 如果while循环结束，说明客户端已断开连接
    std::cout << "Client disconnected: Socket FD=" << clnt_sock << std::endl;

    // 注意：不再需要从全局列表中删除客户端的操作
    // 因为线程之间是独立的，这个线程结束后，它的资源会被自动回收

    // 关闭与该客户端通信的套接字
    close(clnt_sock);
}

// ===================================================================================
// 主函数 (已简化)
// ===================================================================================

int main(int argc, char** argv) {
    int serv_sock;
    int clnt_sock;

    struct sockaddr_in serv_addr;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size;

    if (argc != 2) {
        error_handling("Usage: <port>");
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1) {
        error_handling("socket() error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        error_handling("bind() error");
    }

    if(listen(serv_sock, 5) == -1) {
        error_handling("listen() error");
    }
    
    std::cout << "Echo Server started. Waiting for client connections..." << std::endl;
    
    clnt_addr_size = sizeof(clnt_addr);

    while(1) {
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if(clnt_sock == -1) {
            error_handling("accept() error");
        }
        
        // 【核心修改】: 不再将客户端套接字加入全局列表，因为不需要了
        // mtx.lock(); // <- 不再需要
        // clnt_socks[clnt_count++] = clnt_sock; // <- 不再需要
        // mtx.unlock(); // <- 不再需要

        // 创建工作线程，处理该客户端的回显请求
        std::thread t(handle_client, clnt_sock);
        t.detach();
                        
        char clnt_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clnt_addr.sin_addr, clnt_ip, INET_ADDRSTRLEN);
        std::cout << "New client connected: IP=" << clnt_ip 
                    << ", Port=" << ntohs(clnt_addr.sin_port) 
                    << ", Socket FD=" << clnt_sock << std::endl;
    }
    
    close(serv_sock); // 这行代码同样不会被执行
    return 0;
}
