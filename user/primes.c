#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
getIntFromBuf(char buf[])
{
    int value;
    memcpy(&value, buf, sizeof(int));
    return value;
}

void
processor(int reader)
{
    int p[2];
    pipe(p);
    char buf[50];
    int n;

    // get the first int from pipe
    n = read(reader, buf, sizeof(int));
    int firstInt;
    if (n == sizeof(int)) {
        firstInt = getIntFromBuf(buf);
        printf("prime %d\n", firstInt);
    } else {
        return;
    }

    if (fork() > 0) {
        // process integers from left pipe
        while ((n = read(reader, buf, sizeof(int))) == sizeof(int)) {
            int nextInt = getIntFromBuf(buf);
            if (nextInt % firstInt != 0) {
                // send to right pipe
                write(p[1], &nextInt, sizeof(int));
            }
        }
        wait(0);
    } else {
        processor(p[0]);
    }
}

void
generator(int writer)
{
    for (int i = 2; i <= 35; i++) {
        write(writer, &i, sizeof(int));
    }
}

int
main(int argc, char const *argv[])
{
    int p[2];
    pipe(p);

    if (fork() > 0) {
        generator(p[1]);
        wait(0);
    } else {
        processor(p[0]);
    } 
    exit(0);
}
