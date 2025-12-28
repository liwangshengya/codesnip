#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <wait.h>
#include <signal.h>
#include <errno.h>
const int BUF_SIZE = 1024;
void error_handling(std::string message) {
    std::cout << message << std::endl;
    exit(1);
}
void read_childproc(int) {
    int status;
    pid_t id = waitpid(-1, &status, WNOHANG);
    if ( WIFEXITED(status)) {
        std::cout << "child process " << id << " terminated" << std::endl;
        std::cout << "exit code: " << WEXITSTATUS(status) << std::endl;
    }
}
int main(int argc, char** argv) {

    int serv_sock;
    int clnt_sock;

    struct sockaddr_in serv_addr;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size;
    char message[BUF_SIZE];
    
    struct sigaction act;
    act.sa_handler = read_childproc;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGCHLD, &act, NULL);

    if (argc != 2) {
        error_handling("Usage: <port>");
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    int optval = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, 
                (void*)&optval, sizeof(optval));
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
    /* 在客户端断开时 accept() 会返回 -1,原因是子线程退出，被信号中断 */
    /*信号中断的过程：
        父进程在while(1)循环中，阻塞在accept()调用上，等待新客户端连接。

        与此同时，一个已经连接的客户端断开了。

        处理这个客户端的子进程完成了它的工作（read返回0，break循环），然后子进程退出。

        子进程退出时，操作系统会给父进程发送一个SIGCHLD信号。

        父进程收到SIGCHLD信号，会中断它当前正在做的事情（即阻塞的accept()调用），
        转而去执行你注册的信号处理函数read_childproc（这个函数通常用来wait()或waitpid()来回收子进程，防止僵尸进程）。

        当信号处理函数执行完毕后，控制权返回到主循环。

        关键点：由于accept()系统调用被信号中断了，它会立即返回-1，
                并且系统会设置一个全局变量errno为EINTR（Interrupted system call，即“系统调用被中断”）。*/
    while(1) {
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if(clnt_sock == -1) {
            if(errno == EINTR) {
                std::cout << "accept() interrupted by signal. Continuing..." << std::endl;
                continue;
            } else  {
                error_handling("accept() error");
            }
        } else {
            std::cout << "Connected client " << ++i << std::endl;
        }

        pid_t pid = fork();
        if(pid == 0) {   /* child process */
            close(serv_sock);
            while (1) {
                memset(message, 0, sizeof(message));
                int str_len = read(clnt_sock, message, sizeof(BUF_SIZE));
                if(str_len == 0) {
                    break;
                }
                std::cout << "Message from client " << i << ": " << message << std::endl;
                write(clnt_sock, message, str_len);
            }
        } else {
            close(clnt_sock);
        }
    }

    close(serv_sock);
    return 0;
}