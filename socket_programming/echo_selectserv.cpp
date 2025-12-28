#include <bits/types/struct_timeval.h> // 包含 timeval 结构体的定义
#include <cstring> // 包含内存操作函数，如 memset
#include <iostream> // 包含标准输入输出流
#include <string> // 包含 std::string
#include <sys/socket.h> // 包含套接字相关的函数和结构体
#include <netinet/in.h> // 包含互联网地址族结构体（sockaddr_in）
#include <sys/wait.h> // 包含 waitpid 等待子进程的函数
#include <unistd.h> // 包含 POSIX 操作系统 API，如 close, read, write
#include <arpa/inet.h> // 包含网络地址转换函数，如 htonl, htons
#include <wait.h> // 包含 wait 相关的函数（通常 sys/wait.h 已经包含）
#include <signal.h> // 包含信号处理函数
#include <errno.h> // 包含错误码定义
#include <sys/select.h> // 包含 select 函数和 fd_set 相关的宏

const int BUF_SIZE = 1024; // 定义缓冲区大小
#define ISPRINT true // 定义是否打印错误信息

/**
 * @brief 错误处理函数，打印错误信息并退出程序。
 * 
 * @param message 要打印的错误信息。
 */
void error_handling(std::string message) {
    #if ISPRINT
    std::cout << message << std::endl;
    #endif
    exit(1);
}

/**
 * @brief 主函数，实现一个基于 select 的多连接 TCP 服务器。
 * 
 * @param argc 参数个数。
 * @param argv 参数数组。第一个参数应为端口号。
 * @return int 返回程序退出状态。
 */
int main(int argc, char** argv) {

    int serv_sock; // 服务器套接字文件描述符
    int clnt_sock; // 客户端套接字文件描述符
    fd_set read_fds; // 文件描述符集合，用于 select 监控可读事件
    struct timeval timeout; // select 的超时时间结构体

    struct sockaddr_in serv_addr; // 服务器地址信息结构体
    struct sockaddr_in clnt_addr; // 客户端地址信息结构体
    socklen_t clnt_addr_size; // 客户端地址结构体大小
    char message[BUF_SIZE]; // 用于接收和发送数据的缓冲区
    
    
    if (argc != 2) {
        // 检查参数数量，确保用户提供了端口号
        error_handling("Usage: <port>");
    }

    // 1. 创建服务器套接字：使用 IPv4 (PF_INET)，TCP 协议 (SOCK_STREAM)，默认协议 (0)
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    // 设置套接字选项 SO_REUSEADDR，允许地址重用，防止 TIME_WAIT 状态导致重启失败
    int optval = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, 
                (void*)&optval, sizeof(optval));

    if(serv_sock == -1) {
        error_handling("socket() error");
    }

    // 2. 初始化服务器地址信息
    memset(&serv_addr, 0, sizeof(serv_addr)); // 清零
    serv_addr.sin_family = AF_INET; // 设置为 IPv4
    // INADDR_ANY 表示接受来自任何 IP 地址的连接请求，使用网络字节序
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    // 设置端口号，从命令行参数获取并转换为网络字节序
    serv_addr.sin_port = htons(atoi(argv[1]));

    // 3. 绑定套接字到指定地址和端口
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        error_handling("bind() error");
    }

    // 4. 监听连接请求，设置等待队列最大长度为 5
    if(listen(serv_sock, 5) == -1) {
        error_handling("listen() error");
    }

    // 初始化文件描述符集合 (read_fds)
    FD_ZERO(&read_fds);
    // 将服务器套接字添加到监控集合中，因为它负责接收新的连接
    FD_SET(serv_sock, &read_fds);
    // 初始化当前最大文件描述符为服务器套接字
    int max_fd = serv_sock;

    // 设置 select 的超时时间为 5 秒
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;


    // 5. 主循环：使用 select 监听事件
    while(1) {
        // 每次调用 select 前必须重新复制 read_fds，因为 select 会修改其内容
        fd_set tmp_fds = read_fds;
        
        // 每次循环需要重置 timeout，因为一些系统（如 Linux）select 可能会修改 timeout
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        // 调用 select 进行阻塞等待，监听 max_fd + 1 个描述符的读事件
        int result = select(max_fd + 1, &tmp_fds, NULL, NULL, &timeout);
        
        if(result == -1) {
            // select 错误
            error_handling("select() error");
        } else if(result == 0) {
            // 超时，没有发生任何事件
            std::cout << "Time out" << std::endl;
            continue;
        }

        // 遍历所有可能的 fd，检查哪个 fd 产生了事件
        for(int fd = 0; fd <= max_fd; fd++) { 
            // 检查当前 fd 是否在 select 返回的集合中（即可读）
            if(FD_ISSET(fd, &tmp_fds)) { 
                
                if(fd == serv_sock) {
                    // 如果是服务器套接字可读，表示有新的连接请求
                    std::cout << "Connected client (socket " << fd << ")" << std::endl;
                    
                    clnt_addr_size = sizeof(clnt_addr);
                    // 接受新的连接
                    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
                    
                    if(clnt_sock == -1) {
                        // 如果 accept 被信号中断 (EINTR)，则继续循环
                        if (errno == EINTR) {
                            continue;
                        }
                        error_handling("accept() error");
                    }
                    
                    // 将新的客户端套接字加入到总的监控集合 read_fds 中
                    FD_SET(clnt_sock, &read_fds);
                    // 更新最大文件描述符
                    max_fd = std::max(max_fd, clnt_sock);

                } else {
                    // 如果是客户端套接字可读，表示有数据到达或连接关闭

                    // 读取数据
                    int str_len = read(fd, message, sizeof(message));
                    
                    if(str_len == 0) {
                        // read() 返回 0 表示客户端关闭了连接 (EOF)
                        std::cout << "Client " << fd << " disconnected" << std::endl;
                        
                        // 从监控集合中移除该套接字
                        FD_CLR(fd, &read_fds);
                        // 关闭该套接字
                        close(fd);
                        // 注意：此处如果断开的 fd 是 max_fd，理论上需要重新计算 max_fd，
                        // 但由于 max_fd 只是 select 的上限，不影响正确性，只是效率略有降低。
                    } else {
                        // 读到数据，进行回显
                        message[str_len] = '\0'; // 确保数据以 null 结尾（如果需要按字符串输出）
                        std::cout << "Message from client " << fd << ": " << message << std::endl;
                        
                        // 将收到的数据回写给客户端
                        write(fd, message, str_len);
                    }
                }
            } 
        }

    }
    // 6. 关闭服务器套接字（实际上循环是无限的，这行代码不会执行到）
    close(serv_sock);
    return 0;
}
