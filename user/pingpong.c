#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char const *argv[])
{
    int fds[2];
    pipe(fds);

    char buf[10];

    if (fork() == 0) {
        // The reading side is supposed to learn that 
        // the writer has finished if it notices an EOF condition. 
        // This can only happen if all writing sides are closed. 
        // So it is best if it closes its writing FD ASAP.
        close(fds[1]);
        read(fds[0], buf, sizeof(buf));
        printf("%d: received ping\n", getpid());
        write(fds[1], "b", 1);
        close(fds[1]);
    } else {
        // The writer should close its reading FD just in order not to have too many FDs open 
        // and thus reaching a maybe existing limit of open FDs. 
        // Besides, if the then only reader dies, the writer gets notified about this 
        // by getting a SIGPIPE or at least an EPIPE error. 
        // If there are several readers, the writer cannot detect that "the real one" went away, 
        // goes on writing and gets stuck as the writing FD blocks in the hope, the "unused" reader will read something.
        close(fds[0]);
        write(fds[1], "a", 1);
        wait(0);
        read(fds[0], buf, sizeof(buf));
        close(fds[0]);
        printf("%d: received pong\n", getpid());
    }
    exit(0);
}
