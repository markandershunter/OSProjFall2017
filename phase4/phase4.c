#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <providedPrototypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "driver.h"

int semRunning;
int sleepMBoxID;

process processTable[MAXPROC];
procPtr sleepQ;

static int	ClockDriver(char *);
// static int	DiskDriver(char *);

void
start3(void)
{
    // char	name[128];
    // char    buf[10];
    // int		i;
    int		clockPID;
    // int		pid;
    int		status;
    /*
     * Check kernel mode here.
     */



     if (!(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)) {
         USLOSS_Console("start3(): called while in user mode. Halting....\n");
         USLOSS_Halt(1);
     }

     initializeSysCallTable();
     initializeProcessTable();
     sleepQ = NULL;
    

    /*
     * Create clock device driver
     * I am assuming a semaphore here for coordination.  A mailbox can
     * be used instead -- your choice.
     */
    semRunning = semcreateReal(0);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0) {
        USLOSS_Console("start3(): Can't create clock driver\n");
        USLOSS_Halt(1);
    }
    /*
     * Wait for the clock driver to start. The idea is that ClockDriver
     * will V the semaphore "semRunning" once it is running.
     */

    sempReal(semRunning);

    /*
     * Create the disk device drivers here.  You may need to increase
     * the stack size depending on the complexity of your
     * driver, and perhaps do something with the pid returned.
     */

    // for (i = 0; i < USLOSS_DISK_UNITS; i++) {
    //     sprintf(buf, "%d", i);
    //     pid = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);
    //     if (pid < 0) {
    //         USLOSS_Console("start3(): Can't create term driver %d\n", i);
    //         USLOSS_Halt(1);
    //     }
    // }

    // May be other stuff to do here before going on to terminal drivers

    /*
     * Create terminal device drivers.
     */


    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case first letters, as shown in provided_prototypes.h
     */
    spawnReal("start4", start4, NULL, 4 * USLOSS_MIN_STACK, 3);
    waitReal(&status);

    /*
     * Zap the device drivers
     */
    zap(clockPID);  // clock driver

    // eventually, at the end:
    quit(0);

}

static int ClockDriver(char *arg)
{
    int result;
    int status;
    int currTime;
    procPtr ptr;

    sleepMBoxID = MboxCreate(1, 0);

    // Let the parent know we are running and enable interrupts.
    semvReal(semRunning);
    int i = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    i++;

    // Infinite loop until we are zap'd
    while(! isZapped()) {
        result = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
        if (result != 0) {
            return 0;
        }
        /*
        * Compute the current time and wake up any processes
        * whose time has come.
        */
        MboxSend(sleepMBoxID, NULL, 0);

        ptr = sleepQ;
        gettimeofdayReal(&currTime);

        while(ptr != NULL){
            if(ptr->sleepEndTime < currTime){
                removeFromSleepQ(ptr->pid);
                MboxSend(ptr->mbox_id, NULL, 0);
            }
            ptr = ptr->nextSleeperProc;
        }

        result = MboxReceive(sleepMBoxID, NULL, 0);

        if (result < 0) {
            quit(result);
        }
    }

    return 0;
}

// static int
// DiskDriver(char *arg)
// {
//     return 0;
// }

void sleep(USLOSS_Sysargs* args){
    int result = sleepReal((int)(long)args->arg1);

    args->arg4 = (void*) (long) result;
}

int sleepReal(int seconds){
    int pid = 0;
    int time_ = 0;

    // check for valid seconds value
    if(seconds < 0) return INVALID_SLEEP_TIME;


    gettimeofdayReal(&time_);

    pid = getpid();

    processTable[pid % MAXPROC].pid = pid;
    processTable[pid % MAXPROC].startTime = time_;
    processTable[pid % MAXPROC].sleepEndTime = time_ + (1000000 * seconds);

    MboxSend(sleepMBoxID, NULL, 0);
    addToSleepQ(pid);
    MboxReceive(sleepMBoxID, NULL, 0);


    MboxReceive(processTable[pid % MAXPROC].mbox_id, NULL, 0);

    return 0;
}

void setToUserMode() {
    int result = USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE);
    result++;
}

void initializeSysCallTable() {
    systemCallVec[SYS_SLEEP] = sleep;
}

void initializeProcessTable() {
    int i = 0;

    for(i = 0; i < MAXPROC; i++){
        processTable[i].pid = 0;
        processTable[i].nextSleeperProc = NULL;
        processTable[i].startTime = 0;
        processTable[i].sleepEndTime = 0;
        processTable[i].mbox_id = MboxCreate(1,0);
    }
}

void nullsys3() {

}

void addToSleepQ(int pid){
    procPtr ptr = NULL;

    if(sleepQ == NULL){
        sleepQ = &processTable[pid % MAXPROC];
        return;
    }

    ptr = sleepQ;

    while(ptr->nextSleeperProc != NULL){
        ptr = ptr->nextSleeperProc;
    }

    ptr->nextSleeperProc = &processTable[pid % MAXPROC];
}

void removeFromSleepQ(int pid){
    procPtr ptr = NULL;

    if(sleepQ->pid == pid){
        sleepQ = sleepQ->nextSleeperProc;
        return;
    }

    ptr = sleepQ;

    while(ptr->nextSleeperProc != NULL){
        if(ptr->nextSleeperProc->pid == pid){
            ptr->nextSleeperProc = ptr->nextSleeperProc->nextSleeperProc;
        }
        ptr = ptr->nextSleeperProc;
    }
}
