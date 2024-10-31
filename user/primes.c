#include "../kernel/types.h"
#include "../user/user.h"

void primeproc(int *fd)
{

    close(fd[1]);
    int n, next_pipefd[2];

    int int_bytes = sizeof(int);
    pipe(next_pipefd);
    // 从 fd 读入父进程的输出
    if (read(fd[0], &n, int_bytes) == int_bytes)
    {
        printf("prime %d\n", n);
        int child_pid = fork();
        if (child_pid != 0)
        {

            primeproc(next_pipefd);
            exit(0);
        }
        // 把筛过的数传给自己的子进程
        else
        {
            // parent process
            close(next_pipefd[0]);
            int temp;
            while (read(fd[0], &temp, int_bytes) == int_bytes)
            {
                if ((temp % n) != 0)
                {
                    write(next_pipefd[1], &temp, int_bytes);
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

    int pipefd[2], err;
    if ((err = pipe(pipefd)) < 0)
    {
        fprintf(2, "cannot create a pipe\n");
        exit(-1);
    }
    int c_pid = fork();
    if (c_pid == 0)
    {
        primeproc(pipefd);
        exit(0);
    }
    close(pipefd[0]);

    // feed all intngers through pipe
    int limit = 35;
    if (argc == 2)
    {
        limit = atoi(argv[1]);
    }
    for (int i = 2; i <= limit; i++)
    {
        int cur_write_bytes = 0;
        if ((cur_write_bytes = write(pipefd[1], &i, 4)) != 4)
        {
            fprintf(2, "cannot write interger %d to pipe, %d bytes", i, cur_write_bytes);
        }
    }
    close(pipefd[1]);
    wait(0);

    exit(0);
}