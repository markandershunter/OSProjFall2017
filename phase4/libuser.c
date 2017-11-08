/*
 *  File:  libuser.c
 *
 *  Description:  This file contains the interface declarations
 *                to the OS kernel support package.
 *
 */

#include <usyscall.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <libuser.h>
#include <stdint.h>

#define CHECKMODE {    \
    if (USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) { \
        USLOSS_Console("Trying to invoke syscall from kernel\n"); \
        USLOSS_Halt(1);  \
    }  \
}

int Sleep(int seconds){
    USLOSS_Sysargs sysArg;

    CHECKMODE;

    sysArg.number = SYS_SLEEP;
    sysArg.arg1 = (void *) (long) seconds;

    USLOSS_Syscall(&sysArg);

    return (int)(long) sysArg.arg4;
}



int DiskRead(void *dbuff, int unit, int track, int first, int sectors, int *status){
    return 0;
}
int DiskWrite(void *dbuff, int unit, int track, int first, int sectors, int *status){
    return 0;
}
int DiskSize(int unit, int *sector, int *track, int *disk){
    return 0;
}
int TermRead(char *buff, int bsize, int unit_id, int *nread){
    return 0;
}
int TermWrite(char *buff, int bsize, int unit_id, int *nwrite){
    return 0;
}
