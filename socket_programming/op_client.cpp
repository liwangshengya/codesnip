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
        std::cout << "Operand count: ";
        int operand_count;
        std::cin >> operand_count;
        if(operand_count < 2 || operand_count > 100) {
            std::cout << "Invalid operand count. Please enter a number between 2 and 100." << std::endl;
            continue;
        }
        write(sock, &operand_count, sizeof(int));

        for(int i = 0; i < operand_count; i++) {
            int operand;
            std::cout << "Operand " << i + 1 << ": ";
            std::cin >> operand;
            write(sock, &operand, sizeof(int));
        }

        char op;
        std::cout << "Operator (+, -, *, /): ";
        std::cin >> op;
        write(sock, &op, sizeof(char));

        int result;
        read(sock, &result, sizeof(int));
        std::cout << "Result from server: " << result << std::endl;
    }



    

    close(sock);

    return 0;
}