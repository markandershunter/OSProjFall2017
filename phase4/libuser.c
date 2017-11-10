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
    USLOSS_Sysargs sysArg;

    CHECKMODE;

    sysArg.number = SYS_DISKREAD;
    sysArg.arg1 = (void *) dbuff;
    sysArg.arg2 = (void *)(long) unit;
    sysArg.arg3 = (void *)(long) track;
    sysArg.arg4 = (void *)(long) first;
    sysArg.arg5 = (void *)(long) sectors;

    USLOSS_Syscall(&sysArg);

    *status = (int)(long) sysArg.arg5;

    return *status;
}


int DiskWrite(void *dbuff, int unit, int track, int first, int sectors, int *status){
    USLOSS_Sysargs sysArg;

    CHECKMODE;

    sysArg.number = SYS_DISKWRITE;
    sysArg.arg1 = (void *) dbuff;
    sysArg.arg2 = (void *)(long) unit;
    sysArg.arg3 = (void *)(long) track;
    sysArg.arg4 = (void *)(long) first;
    sysArg.arg5 = (void *)(long) sectors;
    USLOSS_Syscall(&sysArg);
    

    *status = (int)(long) sysArg.arg5;

    return *status;
}



int DiskSize(int unit, int *sector, int *track, int *disk){
    USLOSS_Sysargs sysArg;

    CHECKMODE;

    sysArg.number = SYS_DISKSIZE;
    sysArg.arg1 = (void *)(long) unit;
    sysArg.arg2 = (void *) sector;
    sysArg.arg3 = (void *) track;
    sysArg.arg4 = (void *) disk;

    USLOSS_Syscall(&sysArg);

    return (int)(long) sysArg.arg5;
}



int TermRead(char *buff, int bsize, int unit_id, int *nread){
    return 0;
}
int TermWrite(char *buff, int bsize, int unit_id, int *nwrite){
    return 0;
}
