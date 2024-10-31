#include "../kernel/types.h"
#include "../user/user.h"


int main(int argc,char **argv){
    char buf;
    // 0 for read , 1 for write
    /* father write*/
    int pipefd_parent[2];
    /* child wirte */
    int pipefd_child[2];
    if(pipe(pipefd_parent) == -1 || pipe(pipefd_child)){
        fprintf(2, "can not create a pipe\n");
        exit(1);
    }
    int child_pid = fork();
    if(child_pid == -1){
        fprintf(2, "can not fork\n");
    }
    if(child_pid > 0){
        close(pipefd_child[1]);
        close(pipefd_parent[0]);
        char p_send = 'a';
        write(pipefd_parent[1], (void*)(&p_send), sizeof(p_send));
        close(pipefd_parent[1]);
        read(pipefd_child[0],(void*)(&buf),sizeof(buf));
        printf("%d: received pong\n",getpid());
        close(pipefd_child[0]);
       
    }
    else if(child_pid == 0){
        close(pipefd_parent[1]);
        read(pipefd_parent[0], (void *)(&buf), sizeof(buf));
        close(pipefd_parent[0]);
        printf("%d: received ping\n",getpid());
        char c_send = 'x';
        close(pipefd_child[0]);
        write(pipefd_child[1], (void*)(&c_send), sizeof(c_send));
        close(pipefd_child[1]);
        exit(0);
    }
    exit(0);
}