
#include <stdio.h>
#include <stdlib.h>
#include "usloss.h"
#include <phase1.h>
#include "vm.h"


#define DEBUG 0
extern int debugflag;

extern Process processes[MAXPROC];
extern int numPages;

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
    USLOSS_Console("old: %d, new: %d\n", old, new);

    if (old == 10 && new == 11) USLOSS_MmuMap(0, 0,0,USLOSS_MMU_PROT_RW);

    if (DEBUG && debugflag)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
} /* p1_switch */

void
p1_quit(int pid)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
} /* p1_quit */
