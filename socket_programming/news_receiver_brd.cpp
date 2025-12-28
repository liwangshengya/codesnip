#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

/*
若是比服务端晚启动，会从某个时间点开始接收广播消息，之前的消息收不到。
若是比服务端早启动，则会在服务端开始广播消息后开始接收



UDP 是一种无连接、不可靠的数据报协议。让我们来分解这对你的程序意味着什么：

🚀 为什么会从中间开始？
UDP 是“只管发，不管收” 当你的发送端（Sender）程序启动时，它就开始从 hello.txt 读取文件，
并通过 sendto 将数据包广播出去。它根本不关心网络上是否有任何程序在监听这个端口。它只是尽力将数据包发送出去。

操作系统没有“应用程序缓冲区” 当你的发送端发送“第1行”数据时，这个 UDP 包到达了你的电脑（或网络上的所有电脑）。操作系统内核会查看这个包的目的端口（例如 9000）。

此时，你的接收端（Receiver）程序还没有运行。

操作系统内核会说：“哦，一个发给端口 9000 的 UDP 包。但我检查了一下，没有任何应用程序绑定(bind)在这个端口上。” 于是，内核会立即丢弃这个数据包。它不会为“将来可能运行”的程序缓存这个包。

时间线复现 让我们假设 hello.txt 有 5 行内容，发送间隔为 2 秒：

时间 0s: 你启动了发送端。

时间 0s: 发送端 sendto “第1行”。（此时接收端未启动，数据包被系统丢弃）。

时间 2s: 发送端 sendto “第2行”。（此时接收端未启动，数据包被系统丢弃）。

时间 3s: 你启动了接收端。接收端 bind() 成功，并阻塞在 recvfrom 调用上，等待数据。

时间 4s: 发送端 sendto “第3行”。

时间 4s (几乎同时): 接收端的操作系统收到“第3行”的数据包。内核发现：“啊哈！有一个程序正在监听这个端口！” 于是内核将数据包交给你的接收程序。

时间 4s (几乎同时): 你的接收程序 recvfrom 返回，打印出“第3行”。

时间 6s: 发送端 sendto “第4行”。接收程序接收并打印。

...以此类推。
*/
const int BUF_SIZE = 30;
void error_handling(std::string message) {
    std::cout << message << std::endl;
    exit(1);
}

int main(int argc, char* argv[]) {

    int recv_sock;
    int str_len;
    char buf[BUF_SIZE];
    struct sockaddr_in addr;


    if(argc != 2) {
        error_handling("Usage : <port>");
    }

    recv_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(recv_sock == -1) {
        error_handling("socket() error");
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(argv[1]));

    if(bind(recv_sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        error_handling("bind() error");
    }
    while(1) { 
        str_len = recvfrom(recv_sock, buf, BUF_SIZE, 0, NULL, NULL);
        if(str_len < 0) {
            break;
        }
        buf[str_len] = 0;
        std::cout << buf << std::endl;
    }
    close(recv_sock);
    return 0;



}
