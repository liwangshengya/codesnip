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
    struct ip_mreq join_adr;

    if(argc != 3) {
        error_handling("Usage : <group IP> <port>");
    }

    recv_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if(recv_sock == -1) {
        error_handling("socket() error");
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(argv[2]));

    if(bind(recv_sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        error_handling("bind() error");
    }

    join_adr.imr_multiaddr.s_addr = inet_addr(argv[1]);
    join_adr.imr_interface.s_addr = htonl(INADDR_ANY);

    setsockopt(recv_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                (void*)&join_adr, sizeof(join_adr));

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
