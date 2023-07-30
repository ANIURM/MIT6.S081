#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void 
primes(int *p)
{
    // close the write end of the parent's pipe
    close(p[1]);
    // The first int received from pipe must be prime
    int prime;
    if (read(p[0], &prime, sizeof(prime)) != sizeof(prime)) {
        // 已经从管道里读不出东西了，直接exit.
        exit(0);
    }
    printf("prime %d\n", prime);

    int next;
    // 作为父进程，新建一个通往子进程的管道。
    int newPipe[2];
    pipe(newPipe);
    if (fork() == 0) {
        // 子进程直接进入递归。
        primes(newPipe);
    } else {
        // 作为父进程，不需要在newPipe中读东西，而是写
        close(newPipe[0]);
        // 读取本进程的父进程传来的数据
        // 有点绕hh，可以试着把流水线从左到右画
        // 这里就相当于从左边的管道读东西，再视情况传递给右边的管道
        while(read(p[0], &next, sizeof(next)) == sizeof(next)) {
            if (next % prime != 0) {
                write(newPipe[1], &next, sizeof(next));
            }
        }
        // 写完了，赶紧关掉右侧管道的写入端
        // 这样右侧的子进程才能知道EOF（文件结束）
        // 不关闭的话会导致右侧的子进程阻塞，视觉上看就是整个程序卡死了
        close(newPipe[1]);
        // 不需要从左侧读数据了，关掉左侧的读取端
        close(p[0]);
        wait(0);
    }
}

int
main(int argc, char const *argv[])
{
    int p[2];
    pipe(p);
    int start = 2, end = 35;

    if (fork() > 0) {
        close(p[0]);
        // 作为流水线的开端，向右输送数据
        for (int i = start; i <= end; i++) {
            write(p[1], &i, sizeof(i));
        }
        close(p[1]);
        wait(0);
    } else {
        // 递归创造流水线
        primes(p);
    } 
    exit(0);
}
