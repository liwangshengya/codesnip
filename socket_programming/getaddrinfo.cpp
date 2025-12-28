#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {


    struct addrinfo hints;
    struct addrinfo *result;

    if(argc != 2) {
        std::cout << "Usage: " << argv[0] << " <host>" << std::endl;
        return 1;
    }

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    int ret = getaddrinfo(argv[1], "http", &hints, &result);
    if (ret != 0) {
        std::cout << "getaddrinfo: " << gai_strerror(ret) << std::endl;
        return 1;
    }
    struct addrinfo *rp;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        void *addr;
        std::string ipver;
        char ipstr[INET6_ADDRSTRLEN]; // 足够容纳 IPv4 和 IPv6 地址的字符串
        // 获取官方名称 (只在第一个结果中打印，避免重复)
        // if (rp == result && rp->ai_canonname != NULL) {
        //     std::cout << "Official name: " << rp->ai_canonname << std::endl;
        // }
        if(rp->ai_canonname != NULL) {
            std::cout << "Canonical name: " << rp->ai_canonname << std::endl;
        }
        // 打印地址类型
        std::cout << "Address type: " << (rp->ai_family == AF_INET ? "AF_INET" : "AF_INET6") << std::endl;
        std::cout << "Address length: " << rp->ai_addrlen << std::endl;
        // 根据 ai_family 的类型，将通用的 sockaddr 转换为具体的 sockaddr_in 或 sockaddr_in6
        if (rp->ai_family == AF_INET) { // IPv4
            struct sockaddr_in* ipv4 = (struct sockaddr_in*)rp->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        } else { // IPv6
            struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)rp->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }
        // 将二进制IP地址转换为可读的字符串
        // inet_ntop 是现代、线程安全的函数，替代了 inet_ntoa
        inet_ntop(rp->ai_family, addr, ipstr, sizeof(ipstr));
        std::cout << "IP address (" << ipver << "): " << ipstr << std::endl;
        
    }
    freeaddrinfo(result);
    return 0;

}