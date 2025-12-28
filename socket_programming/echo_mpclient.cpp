#include <cstring>
#include <iostream>
#include <sched.h>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

const int BUF_SIZE = 30;

void error_handling(std::string message) {
    std::cout << message << std::endl;
    exit(1);
}
void read_routine(int sock, char *buf) {
    while(1) { 
        int str_len = read(sock, buf, BUF_SIZE);
        if(str_len == -1) {
            return;
        }
        buf[str_len] = 0;
        std::cout << "Message from server: " << buf << std::endl;
    }
}
/*这里showdown() 关闭写操作因为：无法通过1次close函数调用传递EOF，
                         close() 只减少引用计数，只要引用计数不为0，
                    Socket就不会被关闭，也就不会发送EOF（即TCP的FIN包）。*/
/*
正确的流程（有 shutdown）
用户在子进程（write_routine）中输入 'Q'。

子进程调用 shutdown(sock, SHUT_WR)。

内核立即向服务器发送 EOF (FIN 包)，告诉服务器：“我（客户端）这边再也不会写数据了。”

子进程 return，然后调用 close(sock)。连接的共享计数器从 2 变为 1。

服务器端：read() 读到了 EOF (返回 0)，于是服务器知道客户端关闭了（写）连接，服务器也会 close() 它的连接，并给客户端回一个 EOF。

客户端（父进程）：它在 read_routine 中阻塞。当它收到服务器回发的 EOF 时，它的 read() 也返回 0。

父进程 break 循环，执行到 main 函数末尾，调用 close(sock)。

内核操作这个连接，共享计数器从 1 变为 0。

此时，连接才在客户端被彻底清理。
*/                    
void write_routine(int sock, char *buf) {
    while(1) {
        std::cout << "Input message(Q to quit): ";
        std::cin >> buf;
        if(!std::strcmp(buf, "q") || !std::strcmp(buf, "Q")) {
            shutdown(sock, SHUT_WR);
            return;
        }
        write(sock, buf, strlen(buf));
    }
}
int main(int argc, char** argv) {

    int sock;
    struct sockaddr_in serv_addr;
    char message[30];

        
   if(argc != 3) {
        std::cout << "Usage : " << argv[0] << " <IP> <port>" << std::endl;
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        error_handling("socket() error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        error_handling("connect() error");
    } else {
        std::cout << "Connected..........." << std::endl;
    }

    pid_t pid = fork();
    if(pid == 0) {
        write_routine(sock, message);
    } else {
        read_routine(sock, message);
    }
    close(sock);

    return 0;
}