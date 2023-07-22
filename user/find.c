#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void
find(char *path, char *name)
{
    int fd;
    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        return;
    }
    if (st.type != T_DIR) {
        fprintf(2, "find: 2nd paramater must be dir\n");
    }

    struct dirent de;
    char buf[512], *p;
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf("find: path too long\n");
        return;
    }

    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) {
            continue;
        }

        // Store full path in buf
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;

        if (strcmp(name, de.name) == 0) {
            printf(buf);
            printf("\n");
        }

        // Check if it's a directory
        if (stat(buf, &st) < 0) {
            printf("find: cannot stat %s\n", buf);
        }
        if (st.type == T_DIR) {
            find(buf, name);
        }
    }
}

int
main(int argc, char const *argv[])
{
    if (argc < 3) {
        fprintf(2, "Usage: find <path> <name>\n");
    }
    return 0;
}
