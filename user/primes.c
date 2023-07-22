#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char const *argv[])
{
    int p[2];
    pipe(p);
    char buf[100];

    if (fork() > 0) {
        close(p[0]);
        for (int i = 2; i <= 35; i++) {
            write(p[1], i, sizeof(int));
        }
        wait(0);

    } else {
        read(p[0], buf, sizeof(int));
    } 
    exit(0);
}
