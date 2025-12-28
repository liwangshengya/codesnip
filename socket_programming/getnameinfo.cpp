#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
void error_handling(const std::string& msg) {
    std::cerr << msg << std::endl;
    exit(1);
}

int main(int argc,char* argv[]) {
    if(argc != 2) {
        std::cout << "Usage: " << argv[0] << " <IP>" << std::endl;
        exit(1);
    }

    struct sockaddr_in addr;
    int result;
    memset(&addr,0,sizeof(addr));
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    addr.sin_family = AF_INET6;
    if(inet_pton(AF_INET6, argv[1], &addr.sin_addr) <= 0) {
        error_handling("Invalid IP address");
    }
     // 策略1: 尝试获取主机名，如果失败，返回错误
    result = getnameinfo((struct sockaddr*)&addr, sizeof(addr), 
        hbuf, NI_MAXHOST, sbuf, NI_MAXSERV, NI_NUMERICSERV);
    if (result == 0) {
        std::cout << "Success: Hostname is " << hbuf << std::endl;
    } else {
        std::cout << "Failed to resolve hostname: " << gai_strerror(result) << std::endl;
    }
    std::cout << "\n--- Attempting to resolve with fallback (default behavior) ---" << std::endl;
     // 策略2: 默认行为，尝试获取主机名，如果失败，则返回数字IP字符串
    // 这是 getnameinfo 最常用的方式
    result = getnameinfo((struct sockaddr*)&addr, sizeof(addr), 
        hbuf, NI_MAXHOST, sbuf, NI_MAXSERV, 0);
    if (result == 0) {
        std::cout << "Success: Hostname is " << hbuf << std::endl;
    } else {
        std::cout << "Failed to resolve hostname: " << gai_strerror(result) << std::endl;
    }
    return 0;


}