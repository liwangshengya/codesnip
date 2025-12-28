#include <cstring>
#include <iostream>
#include <sched.h>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <atomic>

const int BUF_SIZE = 1024;
const int NAME_SIZE = 30;

char name[NAME_SIZE] = "[DEFAULT]";
// 使用原子变量，用于控制线程退出
std::atomic<bool> shutdown_flag(false);

void error_handling(std::string message) {
    std::cout << message << std::endl;
    exit(1);
}
void read_routine(int sock, char *buf) {
    while(!shutdown_flag.load()) { 
        int str_len = read(sock, buf, BUF_SIZE);
        if(str_len == -1) {
            std::cout << "read() error!" << std::endl;
            shutdown_flag.store(true); // 发生错误，也设置标志退出
            return;
        }

        if(str_len == 0) {
            std::cout << "\nServer closed the connection." << std::endl;
            shutdown_flag.store(true);
            return;
        }
        std::cout << "str_len: " << str_len << std::endl;
        buf[str_len] = 0;
        std::cout << "\nMessage from server: " << buf << std::endl;
        std::cout << "Input message(Q to quit): "; // 重新打印提示符，因为上面的cout换行了
    }
}
               
void write_routine(int sock, char *buf) {
    while(!shutdown_flag.load()) {
        std::cout << "Input message(Q to quit): ";
        std::cout.flush();  // 立即刷新缓冲区，确保提示立即显示
        std::cin >> buf;
        if(!std::strcmp(buf, "q") || !std::strcmp(buf, "Q")) {
            shutdown_flag.store(true);
            close(sock);
            error_handling("close");
            break;
        }
        char name_buf[NAME_SIZE + BUF_SIZE];
        sprintf(name_buf, "%s %s", name, buf);
        ssize_t written_bytes = write(sock, name_buf, strlen(name_buf));

        if(written_bytes == -1) {
            std::cout << "Failed to send message to server." << std::endl;
            shutdown_flag.store(true);
            close(sock);
            error_handling("write");
            break;
        } 

    }
}
int main(int argc, char** argv) {

    int sock;
    struct sockaddr_in serv_addr;
    char message[BUF_SIZE];


   if(argc != 4) {
        std::cout << "Usage : " << argv[0] << " <IP> <port> <name>" << std::endl;
        exit(1);
    }
    sprintf(name, "[%s]", argv[3]);

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


   std::thread read_thread(read_routine, sock, message);
   std::thread write_thread(write_routine, sock, message);
   read_thread.join();
   write_thread.join();

    close(sock);

    return 0;
}