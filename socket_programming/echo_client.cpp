#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

void error_handling(std::string message) {
    std::cout << message << std::endl;
    exit(1);
}
int main(int argc, char** argv) {

    int sock;
    struct sockaddr_in serv_addr;
    char message[30];
    int str_len;

        
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

    while(1) {
        std::cout << "Input message(Q to quit): ";
        memset(message, 0, sizeof(message));
        std::cin >> message;
        if(!std::strcmp(message, "q") || !std::strcmp(message, "Q")) {
            break;
        }
        int str_len = write(sock, message, strlen(message));

        int recv_len = 0;
        while (recv_len < str_len) {
            int len = read(sock, message + recv_len, 29);
            if (len == -1) {
                error_handling("read() error");
            }
            recv_len += len;
        }
        //下面的代码会存在粘包的问题：因为数据可能不是一次性全部发送的，
        // message[recv_len] = 0;
        // write(sock, message, strlen(message));
        // str_len = read(sock, message, sizeof(message)-1);
        // message[str_len] = 0;
        // if(str_len == -1) {
        //     error_handling("read() error");
        // }
        std::cout << "Message from server: " << message << std::endl;
    }

    close(sock);

    return 0;
}