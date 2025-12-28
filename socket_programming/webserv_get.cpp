// ===================================================================================
// 头文件部分
// ===================================================================================

#include <cstddef>
#include <cstdio>
#include <cstring>      // C风格字符串操作，如 memset
#include <ios>
#include <iostream>     // 标准输入输出流，如 std::cout
#include <string>       // C++ string 类
#include <sys/socket.h> // Linux/Unix 的套接字编程核心函数，如 socket, bind, listen, accept
#include <netinet/in.h> // 包含互联网协议地址结构，如 sockaddr_in
#include <unistd.h>     // Unix 标准函数定义，如 read, write, close
#include <arpa/inet.h>  // IP地址转换函数，如 inet_ntop, htons, htonl
#include <thread>       // C++11 多线程库
#include <fstream>
#include <sstream>
// ===================================================================================
// 全局变量定义
// ===================================================================================

const int BUF_SIZE = 1024; // 消息缓冲区的大小，用于存储从客户端读取的数据
// ===================================================================================
// 辅助函数定义
// ===================================================================================

/**
 * @brief 错误处理函数
 * @param message 要打印的错误信息
 * 打印错误信息并强制退出整个程序。
 * 注意：在多线程程序中，任何一个线程调用exit都会终止整个进程。
 */
void error_handling(std::string message) {
    std::cout << message << std::endl;
    exit(1);
}

std::string socket_read_line(int socket_fd) { 
    std::string line;
    char c;
    ssize_t len;
    while((len  = recv(socket_fd, &c, 1, 0)) > 0) {
       line += c;
       if(c == '\n') {
           break;
       }
    }
    return line;
}


/**
 * @brief 发送一个格式正确的 HTTP 400 错误响应
 * @param socket_fd 客户端的套接字文件描述符
 */
void send_error(int socket_fd) {

    std::stringstream body_ss;
    body_ss << "<!DOCTYPE html>\n"
            "<html>\n"
            "<head><meta charset=\"utf-8\"><title>400 Bad Request</title></head>\n"
            "<body>\n"
            "<h1>400 Bad Request</h1>\n"
            "<p>发生错误，请检查请求的资源。</p>\n"
            "</body>\n"
            "</html>";

    std::string body = body_ss.str();

    std::stringstream header_ss;
    header_ss << "HTTP/1.0 400 Bad Request\r\n"
              << "Server: Linux Web Server\r\n"
              << "Content-Length: " << body.size() << "\r\n"
              << "Content-Type: text/html; charset=utf-8\r\n"
              << "\r\n";

    std::string header = header_ss.str();

    // 发送头和体
    // data() 返回 char* 指针，size() 返回长度
    send(socket_fd, header.c_str(), header.size(), 0);
    send(socket_fd, body.c_str(), body.size(), 0);

}

void send_data(int clnt_sock, std::string &ct, const std::string &file_name) {
   

    // --- 1. 打开文件并检查 ---
    std::ifstream send_file(file_name, std::ios::ate |  std::ios::binary);
    if (!send_file) {
        std::cout << "File not found" << std::endl;
        send_error(clnt_sock);
        return;
    }
    // --- 2. 动态计算文件大小 ---
   std::streamsize file_size = send_file.tellg();
   send_file.seekg(0, std::ios::beg); // 将文件指针移到文件开头

    // --- 3. 构建并发送完整的、正确的HTTP头部 ---
   std::stringstream header_ss;
   header_ss << "HTTP/1.0 200 OK\r\n"
             << "Server: Linux Web Server\r\n"
             << "Content-Length: " << file_size << "\r\n"
             << "Content-Type: " << ct << "\r\n"
             << "\r\n";
    
    
    // --- 4. 发送头部和正文之间的空行
    std::string header = header_ss.str();
    send(clnt_sock, header.c_str(), header.size(), 0);
    
    // --- 5. 发送文件内容 ---
    // 确保所有内容都被发送出去
    char buf[BUF_SIZE];
    while (send_file.read(buf, BUF_SIZE) || send_file.gcount() > 0) {
        send(clnt_sock, buf, send_file.gcount(), 0);
    }
  


  
}



/**
 * @brief 根据文件名确定 Content-Type
 * @param file 文件名
 * @return 对应的 MIME 类型字符串
 * 这个版本更健壮，能正确处理无扩展名或以点开头的文件。
 */
std::string content_type(const std::string& file) {
   size_t dot_pos = file.find_last_of(".");
   if (dot_pos == std::string::npos || dot_pos == 0) {
       return "text/plain";
   }


   std::string extension = file.substr(dot_pos);
   if (extension == ".html" || extension == ".htm") {
       return "text/html";
   }
   else if (extension == ".css") return "text/css";
   else if (extension == ".js")  return "application/javascript";
   else if (extension == ".jpg")  return "image/jpeg";
   else if (extension == ".png")  return "image/png";
   else {
       return "text/plain";
   }
}


void request_handle(int clnt_sock) {
    std::cout << "Request Handle Start (Thread ID: " << std::this_thread::get_id() << ")" << std::endl;

    std::string req_line = socket_read_line(clnt_sock);

    if (req_line.empty()) {
        close(clnt_sock);
        return;
    }
    std::cout << "Received: " << req_line; // req_line 包含 \n
    
    // ============================================================
    // 【新增代码】读取并丢弃剩余的请求头
    // ============================================================
    while (true) {
        std::string header = socket_read_line(clnt_sock);
        // HTTP 协议中，请求头结束的标志是一个单独的 "\r\n"
        if (header == "\r\n" || header == "\n" || header.empty()) {
            break;
        }
        // 也可以选择打印出来看看浏览器发了什么
        std::cout << "[Header] " << header; 
    }

    // 2. 解析请求行 (使用 stringstream 进行分割)

    std::stringstream ss(req_line);
    std::string method, file_name, protocol;
    ss >> method >> file_name >> protocol;

    if (method != "GET") {
        send_error(clnt_sock);
        close(clnt_sock);
        return;
    }

    if(file_name.size() > 1 && file_name[0] == '/') {
        file_name = file_name.substr(1); // 去掉开头的 '/'
    } else if (file_name == "/") {
        file_name = "index.html"; // 默认首页
    }

    std::string ct = content_type(file_name);
    send_data(clnt_sock, ct, file_name);

    close(clnt_sock);
    std::cout << "Request Handle End" << std::endl;
}

// ===================================================================================
// 主函数
// ===================================================================================

int main(int argc, char** argv) {
    int serv_sock;          // 服务器的监听套接字
    int clnt_sock;          // 为每个客户端创建的通信套接字

    struct sockaddr_in serv_addr; // 服务器地址结构
    struct sockaddr_in clnt_addr; // 客户端地址结构
    socklen_t clnt_addr_size;     // 客户端地址结构的大小

    // 检查命令行参数，程序需要一个端口号作为参数
    if (argc != 2) {
        error_handling("Usage: <port>");
    }

    // --- 1. 创建监听套接字 ---
    // socket() 函数创建一个套接字
    // PF_INET: 使用 IPv4 协议族
    // SOCK_STREAM: 使用 TCP 协议（面向连接的、可靠的）
    // 0: 协议类型，由系统自动选择
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1) {
        error_handling("socket() error");
    }
    
    int optval = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // SO_REUSEADDR: 允许重用本地地址和端口，即使它们正在使用中

    // --- 2. 绑定地址和端口 ---
    // 初始化服务器地址结构
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;                     // 地址族
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);      // IP地址，INADDR_ANY表示监听服务器上所有网络接口
    serv_addr.sin_port = htons(atoi(argv[1]));          // 端口号，htons()将主机字节序转换为网络字节序

    // bind() 函数将套接字与指定的IP和端口绑定
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        error_handling("bind() error");
    }

    // --- 3. 开始监听 ---
    // listen() 函数将套接字设置为被动监听模式，等待客户端的连接请求
    // 5 是请求队列的最大长度（backlog），表示最多可以有5个连接请求在队列中等待
    if(listen(serv_sock, 5) == -1) {
        error_handling("listen() error");
    }
    
    std::cout << "Server started. Waiting for client connections..." << std::endl;
    
    clnt_addr_size = sizeof(clnt_addr);
    int i = 0; // 用于给连接的客户端编号

    // --- 4. 主循环：接受连接并创建线程 ---
    while(1) {
        // accept() 函数会阻塞，直到有新的客户端连接请求到来
        // 成功时，它会返回一个新的套接字（clnt_sock），专门用于与这个新客户端通信
        // 原始的 serv_sock 继续用于监听新的连接
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if(clnt_sock == -1) {
            error_handling("accept() error");
        }
        std::cout << "Connected client " << ++i << std::endl;
        
        // --- 5. 创建线程处理新客户端 ---
        std::thread t(request_handle, clnt_sock);
        t.detach(); // 分离线程，使其在完成后自动释放资源



        // --- 7. 打印新客户端的详细信息 ---
        char clnt_ip[INET_ADDRSTRLEN]; // INET_ADDRSTRLEN 是IPv4地址字符串的最大长度
        // inet_ntop 将网络字节序的IP地址转换为可读的字符串形式
        inet_ntop(AF_INET, &clnt_addr.sin_addr, clnt_ip, INET_ADDRSTRLEN);
        // ntohs 将网络字节序的端口号转换为主机字节序
        std::cout << "New client connected: IP=" << clnt_ip 
                    << ", Port=" << ntohs(clnt_addr.sin_port) 
                    << ", Socket FD=" << clnt_sock << std::endl;
    }
    
    // 这行代码实际上永远不会被执行，因为上面的 while(1) 是一个无限循环
    // 在真实的服务器程序中，需要有信号处理机制（如处理Ctrl+C）来优雅地关闭服务器
    close(serv_sock);

    return 0;
}
