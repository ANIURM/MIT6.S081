#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"


void
processLine(char *argv[])
{
    // exec
    if (fork() > 0) {
        wait(0);
    } else {
        exec(argv[0], argv);
    }
}

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(2, "xargs: wrong usage\n");
        exit(1);
    }
    char buf[60];
    char *p = buf;
    char **arguments = malloc(sizeof(char *) * MAXARG);
    int length;

    // move argv after "xargs" to arguments
    for (int i = 1; i <= argc; i++) {
        length = strlen(argv[i]) + 1;
        arguments[i - 1] = malloc(sizeof(char) * length);
        strcpy(arguments[i - 1], argv[i]);
    }

    while (read(0, p, 1) != 0) {
        if (*p != '\n') {
            p++;
        } else {
            *p = 0;
            // the content in buf is the last argument
            strcpy(arguments[argc - 1], buf);
            arguments[argc] = 0;

            processLine(arguments);

            // clear
            p = buf;
        }
    }

    for (int i = 0; i < argc; i++) {
        free(arguments[i]);
    }
    free(arguments);
    exit(0);
} 