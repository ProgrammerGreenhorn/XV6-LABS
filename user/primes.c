#include "../kernel/types.h"
#include "../user/user.h"

void primeproc(int *fd)
{

    close(fd[1]);
    int n, next_pipefd[2];
    pipe(next_pipefd);
    // read from parent process, the first in pipe is 
    // certainly prime
    if (read(fd[0], &n, sizeof(n)) == sizeof(n)){
        printf("prime %d\n", n);
        int child_pid = fork();
        if (child_pid != 0){
            primeproc(next_pipefd);
            exit(0);
        }
        // child filter the num
        else{
            close(next_pipefd[0]);
            int temp;
            while (read(fd[0], &temp, sizeof(temp)) == sizeof(temp)){
                if ((temp % n) != 0){
                    write(next_pipefd[1], &temp, sizeof(temp));
                }
            }
            close(next_pipefd[1]);
            wait(0);
        }
    }
}
// pipefd[0] refers to the read end
// pipefd[1] refers to the write end
int main(int argc, char **argv)
{

    int pipefd[2];
    if (pipe(pipefd) < 0){
        fprintf(2, "cannot create a pipe\n");
        exit(-1);
    }
    int c_pid = fork();
    /* child */
    if (c_pid == 0){
        primeproc(pipefd);
        exit(0);
    }else if( c_pid > 0) {    /* parent, feed all intergers through pipe */
        close(pipefd[0]);  
        int limit = 35;
        if (argc == 2){
           limit = atoi(argv[1]);
        }
        for (int i = 2; i <= limit; ++i){
           if (write(pipefd[1], &i, sizeof(i)) != sizeof(i)){
              fprintf(2, "cannot write interger %d to pipe", i);
           }
       }
       close(pipefd[1]);
       wait(0);
    }
    exit(0);
}