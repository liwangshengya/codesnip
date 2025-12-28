#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstring>
#include <cstdlib>
#include <unistd.h>


const int BUF_SIZE = 30;
void error_handling(std::string message) {
    std::cout << message << std::endl;
    exit(1);
}

int main(int argc, char* argv[]) {
    
    int send_sock;
    struct sockaddr_in broad_adr;

    FILE* fp;
    int so_brdcast = 1;
    char message[BUF_SIZE];


    if (argc != 3) {
        std::cout << "Usage : <group_address> <port>" << std::endl;
        exit(1);
    }

    send_sock  = socket(PF_INET, SOCK_DGRAM, 0);
    if (send_sock == -1) {
        error_handling("socket() error");
    }

    memset(&broad_adr, 0, sizeof(broad_adr));
    broad_adr.sin_family = AF_INET;
    broad_adr.sin_addr.s_addr = inet_addr(argv[1]);
    broad_adr.sin_port = htons(atoi(argv[2]));
    
    setsockopt(send_sock, SOL_SOCKET, SO_BROADCAST,
                (void*)&so_brdcast, sizeof(so_brdcast));
    
    if ((fp = fopen("hello.txt", "r")) == NULL) {
        error_handling("fopen() error");
    }
    //UDP 是“只管发，不管收” 当你的发送端（Sender）程序启动时，它就开始从 hello.txt 读取文件，
    // 并通过 sendto 将数据包广播出去。它根本不关心网络上是否有任何程序在监听这个端口。
    // 它只是尽力将数据包发送出去。
    while(!feof(fp)) {
        fgets(message, BUF_SIZE, fp);
        sendto(send_sock, message, strlen(message), 0,
                (struct sockaddr*)&broad_adr, sizeof(broad_adr));
        sleep(2);
    }
    fclose(fp);
    close(send_sock);
    return 0;

}
