
#include <stdio.h>
#include <stdlib.h>
#include "usloss.h"
#include "usyscall.h"
#include <phase1.h>
#include "phase5.h"
#include "vm.h"


#define DEBUG 0
extern int debugflag;

extern Process processes[MAXPROC];
extern int numPages;
extern VmStats  vmStats;

void
p1_fork(int pid)
{
    int i;

    if (DEBUG && debugflag)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);

    if (numPages != -1) {
        processes[pid % MAXPROC].numPages = numPages;
        processes[pid % MAXPROC].pageTable = malloc(sizeof(PTE) * numPages);

        for (i = 0; i < numPages; i++) {
            processes[pid % MAXPROC].pageTable[i].state = UNUSED;
        }
    }
} /* p1_fork */

void
p1_switch(int old, int new)
{
    int i, tag, dummy;
    USLOSS_Console("%d %d\n", old, new);
    // if (old == 0) USLOSS_Console("hooray\n");

    if (DEBUG && debugflag)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);

    if (old == 10 && new == 11) {
        dummy = USLOSS_MmuGetTag(&tag);

        if (old >= 11) {
            for (i = 0; i < numPages; i++) {
                if (processes[old % MAXPROC].pageTable[i].state != UNUSED) {
                    dummy = USLOSS_MmuUnmap(tag, i);
                }
            }
        }

        if (new >= 11) {
            for (i = 0; i < numPages; i++) {
                if (processes[new % MAXPROC].pageTable[i].state != UNUSED) {
                    dummy = USLOSS_MmuMap(tag, i,
                        processes[new % MAXPROC].pageTable[i].frame, USLOSS_MMU_PROT_RW);
                }
            }
        }


        dummy++;
    }

    vmStats.switches++;
} /* p1_switch */

void
p1_quit(int pid)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
} /* p1_quit */
