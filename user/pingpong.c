#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char const *argv[])
{
    int fds[2];
    pipe(fds);

    int n;
    char buf[10];

    if (fork() == 0) {
        n = read(fds[0], buf, sizeof(buf));
        if (n == -1) {
            printf("Error");
            exit(1);
        }
        printf("%d: received ping\n", getpid());
        write(fds[1], "b", 1);
    } else {
        write(fds[1], "a", 1);
        n = read(fds[0], buf, sizeof(buf));
        if (n == -1) {
            printf("Error");
            exit(1);
        }
        printf("%d: received pong\n", getpid());
    }
    exit(0);
}
