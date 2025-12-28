// ===================================================================================
// 头文件部分
// ===================================================================================

#include <cstring>      // C风格字符串操作，如 memset
#include <iostream>     // 标准输入输出流，如 std::cout
#include <string>       // C++ string 类
#include <sys/socket.h> // Linux/Unix 的套接字编程核心函数，如 socket, bind, listen, accept
#include <netinet/in.h> // 包含互联网协议地址结构，如 sockaddr_in
#include <unistd.h>     // Unix 标准函数定义，如 read, write, close
#include <arpa/inet.h>  // IP地址转换函数，如 inet_ntop, htons, htonl
#include <thread>       // C++11 多线程库
#include <mutex>        // C++11 互斥锁

// ===================================================================================
// 全局变量定义
// ===================================================================================

const int BUF_SIZE = 1024; // 消息缓冲区的大小，用于存储从客户端读取的数据
const int MAX_CLNT = 100;  // 服务器允许的最大并发客户端数量（硬编码限制）

// 以下是所有线程共享的全局数据，因此必须通过互斥锁来保护，防止竞态条件
int clnt_count = 0;         // 当前已连接的客户端数量
int clnt_socks[MAX_CLNT];   // 用于存储所有已连接客户端的套接字文件描述符的数组
std::mutex mtx;             // 互斥锁，用于保护对 clnt_count 和 clnt_socks 的访问

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

/**
 * @brief 将消息广播给所有已连接的客户端
 * @param msg 指向要发送的消息内容的指针
 * @param len 消息的长度
 * 这是一个线程安全的函数，它遍历客户端列表，并向每个客户端发送相同的消息。
 */
void send_msg_to_all(char* msg, int len) {
    // 在访问或修改共享数据 clnt_socks 和 clnt_count 之前，必须加锁
    // 这是为了防止其他线程（例如，正在处理客户端断开连接的线程）同时修改这个列表
    mtx.lock();
    for (int i = 0; i < clnt_count; i++) {
        // 将消息写入每个客户端的套接字
        write(clnt_socks[i], msg, len);
    }
    // 操作完成后，必须立即解锁，以便其他线程可以访问共享数据
    mtx.unlock();
}

/**
 * @brief 处理单个客户端通信的线程函数
 * @param clnt_sock 该线程负责的客户端的套接字文件描述符
 * 每个客户端连接后，都会有一个独立的线程来执行这个函数。
 * 该函数负责接收该客户端的消息，并在客户端断开后进行清理工作。
 */
void handle_client(int clnt_sock) {
    char message[BUF_SIZE]; // 用于接收客户端消息的本地缓冲区
    int str_len;            // read函数返回的实际读取的字节数

    // 循环从客户端套接字读取数据
    // read函数会阻塞，直到客户端发送数据或关闭连接
    while ((str_len = read(clnt_sock, message, sizeof(message))) > 0) {
        // 如果成功读取到数据（str_len > 0），则将该消息广播给所有客户端
        send_msg_to_all(message, str_len);   
    }

    // 如果while循环结束，说明 read() 返回值 <= 0，表示客户端已断开连接
    std::cout << "Client handling thread: Client disconnected, cleaning up..." << std::endl;

    // --- 开始清理工作 ---
    // 同样，修改共享数据 clnt_socks 和 clnt_count 需要加锁
    mtx.lock();
    // 在 clnt_socks 数组中找到当前要断开的客户端的套接字
    for (int i = 0; i < clnt_count; i++) {
        if (clnt_socks[i] == clnt_sock) {
            // 为了高效地从数组中移除元素，用最后一个元素覆盖当前元素
            // 这比将后面所有元素前移一位要快，但会打乱数组顺序（在此场景下无所谓）
            clnt_socks[i] = clnt_socks[clnt_count - 1];
            break; // 找到并处理后即可跳出循环
        }
    }
    clnt_count--; // 客户端总数减一
    mtx.unlock();

    // 关闭与该客户端通信的套接字，释放系统资源
    close(clnt_sock);
    // 注意：这个线程在函数执行完毕后会自动结束
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
        
        // --- 5. 将新客户端信息注册到共享列表 ---
        // 因为要修改全局的 clnt_socks 和 clnt_count，必须加锁
        mtx.lock();
        clnt_socks[clnt_count++] = clnt_sock;
        mtx.unlock();

        // --- 6. 创建工作线程 ---
        // 创建一个新的 std::thread 对象，该线程将执行 handle_client 函数
        // clnt_sock 作为参数传递给新线程
        std::thread t(handle_client, clnt_sock);
        // t.detach() 将子线程与主线程分离。主线程不再等待子线程结束（join()）
        // 子线程在后台独立运行。这使得主循环可以立即返回 accept()，继续等待下一个客户端
        t.detach();
        
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
