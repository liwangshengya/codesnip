#include <cstring>                     // 包含内存操作函数，如 memset
#include <iostream>                    // 包含标准输入输出流，用于 cout
#include <string>                      // 包含 std::string
#include <sys/socket.h>                // 包含套接字相关的函数和结构体定义
#include <netinet/in.h>                // 包含互联网地址族结构体（sockaddr_in）
#include <unistd.h>                    // 包含 POSIX 操作系统 API，如 close, read, write, fork
#include <arpa/inet.h>                 // 包含网络地址转换函数，如 htonl, htons, inet_addr
#include <errno.h>                     // 包含错误码定义 (errno)
#include <sys/epoll.h>                 // 包含 Linux 特有的 I/O 多路复用机制 epoll 的相关函数和宏

const int BUF_SIZE = 3;//1024; // 定义缓冲区大小，用于存储客户端发送的消息
const int MAX_EVENTS = 1024; // 定义 epoll 单次调用最多可返回的事件数量

#define ISPRINT true // 宏定义，用于控制错误信息是否打印到控制台

/**
 * @brief 统一的错误处理函数。
 * 打印指定的错误信息并终止程序运行。
 *
 * @param message 需要打印的错误信息字符串。
 */
void error_handling(std::string message) {
    #if ISPRINT
    std::cerr << message << " (errno: " << errno << ")" << std::endl; // 使用 cerr 输出错误流，并附带 errno
    #endif
    exit(1); // 异常退出程序
}

/**
 * @brief 主函数，实现一个基于 epoll 的高并发 TCP Echo 服务器。
 * 该服务器能够同时处理多个客户端连接，并将任何收到的数据原样回送给客户端。
 *
 * @param argc 命令行参数的数量。
 * @param argv 命令行参数数组，argv[1] 应为服务器绑定的端口号。
 * @return int 程序的退出状态码。
 */
int main(int argc, char** argv) {

    int serv_sock;               // 服务器监听套接字的文件描述符
    int clnt_sock;               // 与特定客户端通信的套接字文件描述符

    struct sockaddr_in serv_addr; // 服务器地址信息结构体
    struct sockaddr_in clnt_addr; // 客户端地址信息结构体
    socklen_t clnt_addr_size;    // 客户端地址结构体的长度，用于 accept 函数
    char message[BUF_SIZE];       // 用于接收和发送数据的缓冲区
    
    // --- 1. 初始化和参数检查 ---
    if (argc != 2) {
        // 检查命令行参数，确保用户提供了端口号
        std::cout << "Usage: " << argv[0] << " <port>" << std::endl;
        error_handling("Incorrect number of arguments");
    }

    // --- 2. 创建服务器套接字 ---
    // 使用 socket() 函数创建一个 TCP 套接字：
    // PF_INET: 指定使用 IPv4 协议族。
    // SOCK_STREAM: 指定使用流式套接字，即 TCP 协议。
    // 0: 表示由系统自动选择协议类型（TCP）。
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    // 设置套接字选项 SO_REUSEADDR，防止服务器重启时因端口处于 TIME_WAIT 状态而绑定失败
    int optval = 1;
    if(setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, 
                  (void*)&optval, sizeof(optval)) == -1) {
        error_handling("setsockopt() error");
    }

    if(serv_sock == -1) {
        error_handling("socket() error");
    }

    // --- 3. 绑定地址和端口 ---
    // 初始化服务器地址结构体
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; // 地址族为 IPv4
    // INADDR_ANY 表示服务器将监听（绑定）所有可用的网络接口（IP地址）
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    // 将命令行传入的端口号字符串转为整数，并从主机字节序转换到网络字节序
    serv_addr.sin_port = htons(atoi(argv[1]));

    // 将套接字与指定的 IP 地址和端口号绑定
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        error_handling("bind() error");
    }

    // --- 4. 开始监听连接 ---
    // 将套接字设置为监听模式，准备接受客户端的连接请求。
    // 第二个参数 5 是连接请求队列（backlog）的最大长度，即最多允许 5 个客户端等待连接。
    if(listen(serv_sock, 5) == -1) {
        error_handling("listen() error");
    }

    // --- 5. 初始化 epoll ---
    // epoll_create() 创建一个 epoll 实例，并返回其文件描述符。
    // 参数已不再使用，但必须大于等于 0。
    int epoll_fd = epoll_create(1); // 建议使用更明确的变量名 epoll_fd
    if(epoll_fd == -1) {
        error_handling("epoll_create() error");
    }

    // 定义 epoll 事件结构体，用于描述要监控的事件
    struct epoll_event ev;
    // 设置要监控的事件类型：
    // EPOLLIN: 表示对应的文件描述符可读（包括对端正常关闭连接）。
    ev.events = EPOLLIN;
    // ev.data 是一个联合体，我们使用其 fd 成员来存储与事件关联的文件描述符。
    // 当 epoll_wait 返回时，我们可以通过 events[i].data.fd 知道是哪个 fd 产生了事件。
    ev.data.fd = serv_sock;

    // 使用 epoll_ctl() 将服务器监听套接字 加入到 epoll 实例的监控列表中。
    // EPOLL_CTL_ADD: 表示添加一个新的文件描述符到监控列表。
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serv_sock, &ev) == -1) {
        error_handling("epoll_ctl() add serv_sock error");
    }

    // 分配一个事件数组，用于存储 epoll_wait() 返回的就绪事件
    struct epoll_event *events = new epoll_event[MAX_EVENTS]; // 使用 new/delete 更符合C++风格

    // --- 6. 服务器主循环 ---
    std::cout << "Server started on port " << argv[1] << ", waiting for connections..." << std::endl;
    while(true) {
        // 等待事件发生。
        // epoll_fd: epoll 实例的文件描述符。
        // events: 指向一个 epoll_event 数组，内核将就绪的事件复制到这个数组中。
        // MAX_EVENTS: 告诉内核本次最多可以返回多少个事件。
        // -1: 表示永久阻塞，直到有事件发生。如果设置为 0，则非阻塞；如果为正数，则超时（毫秒）。
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        
        if(nfds == -1) {
            // 如果 epoll_wait 被信号中断，可以继续等待
            if (errno == EINTR) {
                continue;
            }
            error_handling("epoll_wait() error");
        }

        // 遍历所有发生的事件
        for(int i = 0; i < nfds; i++) {
            // 从就绪事件列表中获取文件描述符
            int current_fd = events[i].data.fd;

            if(current_fd == serv_sock) {
                // --- 处理新连接 ---
                // 如果是服务器监听套接字可读，表明有新的客户端连接请求到达。
                
                clnt_addr_size = sizeof(clnt_addr);
                // accept() 会从监听队列中取出第一个连接请求，创建一个新的套接字用于与该客户端通信。
                clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
                
                if(clnt_sock == -1) {
                    error_handling("accept() error");
                }

                // 获取客户端的IP和端口信息并打印
                char clnt_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clnt_addr.sin_addr, clnt_ip, INET_ADDRSTRLEN);
                std::cout << "New client connected: IP=" << clnt_ip 
                          << ", Port=" << ntohs(clnt_addr.sin_port) 
                          << ", Socket=" << clnt_sock << std::endl;

                // 将新创建的客户端套接字也加入到 epoll 的监控中，以便接收其数据。
                ev.events = EPOLLIN;
                ev.data.fd = clnt_sock;
                if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, clnt_sock, &ev) == -1) {
                    error_handling("epoll_ctl() add clnt_sock error");
                }

            } else {
                // --- 处理客户端数据 ---
                // 否则是某个客户端套接字可读，表明有数据可读或连接已关闭。
                
                int str_len = read(current_fd, message, sizeof(message) - 1); // 预留一个位置给'\0'
                
                if(str_len == -1) {
                    // 读取出错
                    error_handling("read() error");
                } else if(str_len == 0) {
                    // read() 返回 0 表示对端（客户端）已正常关闭连接（发送了 FIN）。
                    std::cout << "Client disconnected (socket " << current_fd << ")" << std::endl;
                    
                    // 从 epoll 实例中移除对该文件描述符的监控。
                    // EPOLL_CTL_DEL: 表示从监控列表中删除一个文件描述符。
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                    // 关闭与该客户端通信的套接字，释放资源。
                    close(current_fd);
                } else if(str_len > 0) {
                    // 成功读取到数据
                    message[str_len] = '\0'; // 添加字符串结束符，以便安全输出
                    
                    std::cout << "Message from client " << current_fd << ": " << message << std::endl;
                    
                    // 将收到的数据原样回写给客户端。
                    // 注意：一个完整的回显服务可能需要处理 write 的部分写入情况。
                    if(write(current_fd, message, str_len) == -1) {
                         error_handling("write() error");
                    }
                }
            }
        }
    }

    // --- 7. 清理资源 ---
    // 这部分代码在无限循环中是不可达的，但作为良好实践，保留它们以便程序能优雅退出（例如通过信号）。
    close(serv_sock);         // 关闭服务器监听套接字
    close(epoll_fd);          // 关闭 epoll 实例
    delete[] events;          // 释放事件数组内存
    
    return 0;
}
