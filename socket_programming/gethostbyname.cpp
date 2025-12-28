#include <netdb.h>
#include <iostream>
#include  <arpa/inet.h>
 void error_handling(std::string message) {
    std::cout << message << std::endl;
    exit(1);
}

int main(int argc, char *argv[]) {
    int i = 0;
    struct hostent *host;

    if(argc != 2) {
        std::cout << "Usage : " << argv[0] << " <hostname>" << std::endl;
        exit(1);
    }

    host = gethostbyname(argv[1]);
    if(!host) {
        error_handling("gethostbyname() error");
    }

    std::cout << "Official name: " << host->h_name << std::endl;
    for(int i = 0; host->h_aliases[i]; i++) {
        std::cout << "Aliases: " << host->h_aliases[i] << std::endl;
    }
    std::cout << "Address type: " << (host->h_addrtype == AF_INET ? "AF_INET" : "AF_INET6") << std::endl;
    std::cout << "Address length: " << host->h_length << std::endl;
    for(int i = 0; host->h_addr_list[i]; i++) {
        std::cout << "IP address: " << inet_ntoa(*(struct in_addr*)host->h_addr_list[i]) << std::endl;
    }

    return 0;

}