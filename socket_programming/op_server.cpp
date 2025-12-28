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

int cal_num(int num[], char op, int count) {
    int result = num[0];
    switch(op) {
        case '+':
            for(int i = 1; i < count; i++) {
                result += num[i];
            }
            break;
        case '-':
            for(int i = 1; i < count; i++) {
                result -= num[i];
            }
            break;
        case '*':
            for(int i = 1; i < count; i++) {
                result *= num[i];
            }
            break;
        case '/':
            for(int i = 1; i < count; i++) {
                result /= num[i];
            }
            break;
        default:
            std::cout << "Unknown operator" << std::endl;
            break;
    }
    return result;
}
int main(int argc, char** argv) {

    int serv_sock;
    int clnt_sock;

    struct sockaddr_in serv_addr;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size;
    

    if (argc != 2) {
        error_handling("Usage: <port>");
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1) {
        error_handling("socket() error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        error_handling("bind() error");
    }

    if(listen(serv_sock, 5) == -1) {
        error_handling("listen() error");
    }
    
    clnt_addr_size = sizeof(clnt_addr);
    int i = 0;
    int opmem[100];
    char op;
    int number_count = 0;
    while(1) {
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if(clnt_sock == -1) {
            error_handling("accept() error");
        }
        std::cout << "Connected client " << ++i << std::endl;
        bool exit = false;
        while(!exit) {
            ssize_t read_len = read(clnt_sock, &number_count, sizeof(int));
            if (read_len <= 0) {
                std::cout << "Client disconnected" << std::endl;
                exit = true;
                break;
            }
            std::cout << "Number count: " << (int)number_count << std::endl;
            if(number_count <= 1 || number_count > 100) {
                std::cout << "Invalid number count: " << (int)number_count << std::endl;
                exit = true;
                break;
            }
            bool read_error = false;
            for(int j = 0; j < number_count; j++) {
                read_len = read(clnt_sock, &opmem[j], sizeof(int));
                if (read_len <= 0) {
                    std::cout << "Client disconnected during number reading" << std::endl;
                    read_error = true;
                    break;
                }
                std::cout << "Number " << j << ": " << opmem[j] << std::endl;
            }
            if (read_error) {
                exit = true;
                break;
            }
            
            read_len = read(clnt_sock, &op, 1);
            if (read_len <= 0) {
                std::cout << "Client disconnected during operator reading" << std::endl;
                exit = true;
                break;
            }
            std::cout << "Operator: " << op << std::endl;
            int result = cal_num(opmem, op, number_count);
            write(clnt_sock, &result, sizeof(int));
            std::cout << "Result: " << result << std::endl;
        }
        close(clnt_sock);
    }
    close(serv_sock);


    return 0;
}