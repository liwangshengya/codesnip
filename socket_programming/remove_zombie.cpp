#include <csignal>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <unistd.h>


void error_handling(std::string message) {
    std::cout << message << std::endl;
    exit(1);
}

void read_children(int) {
    int status;
    pid_t id = waitpid(-1, &status, WNOHANG);
    if ( WIFEXITED(status)) {
        std::cout << "child process " << id << " terminated" << std::endl;
        std::cout << "exit code: " << WEXITSTATUS(status) << std::endl;
    }
}

int main(int, char**) {

    struct sigaction act;
    act.sa_handler = read_children;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGCHLD,&act,NULL);

    pid_t pid = fork();
    if(pid == 0) {
        std::cout << "child process" << std::endl;
        sleep(10);
        exit(10);
    } else {
        std::cout << "parent process" << std::endl;
        std::cout << "child process id: " << pid << std::endl;
        pid = fork();
        if(pid ==0) {
            std::cout << "child process" << std::endl;
            sleep(5);
            exit(5);
        } else {
            std::cout << "parent process" << std::endl;
            std::cout << "child process id: " << pid << std::endl;
            for(int i = 0; i < 5; i++) {
                std::cout << "parent working..." << std::endl;
                sleep(3);
            }
            std::cout << "parent process terminating..." << std::endl;
        }

    }

    return 0;

}