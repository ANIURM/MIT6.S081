#include "sysinfo.h"
#include "defs.h"
#include "types.h"
#include "proc.h"

uint64
sys_sysinfo(void)
{
    uint64 addr;
    if (argaddr(0, &addr) < 0) {
        return -1;
    }

    struct proc *p = myproc();
    struct sysinfo *info;
    info->freemem = freememsize();
    info->nproc = procnum();
    if (copyout(p->pagetable, addr, (char *)info, sizeof(*info)) < 0) {
        return - 1;
    }
    return 0;
}
