#include "../kernel/types.h"
#include "../user/user.h"

int main(int argc,char **argv){
    char buf;
    // 0 for read , 1 for write
    int pipefd[2];

    if(pipe(pipefd) == -1){
        fprintf(2, "can not create a pipe\n");
        exit(1);
    }
    int child_pid = fork();
    if(child_pid == -1){
        fprintf(2, "can not fork\n");
    }
    if(child_pid > 0){
        char p_send = 'a';
        write(pipefd[1], (void*)(&p_send), sizeof(p_send));
        close(pipefd[1]);
        // wait child
        wait(0);
        read(pipefd[0],(void*)(&buf),sizeof(buf));
        printf("%d: received pong\n",getpid());
        close(pipefd[0]);
    }
    else if(child_pid == 0){
        read(pipefd[0], (void *)(&buf), sizeof(buf));
        printf("%d: received ping\n",getpid());
        char c_send = 'x';
        write(pipefd[1], (void*)(&c_send), sizeof(c_send));
        close(pipefd[0]);
        close(pipefd[1]);
        exit(0);
    }
    exit(0);
}