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
int disk0_Q_MBoxID;
int disk1_Q_MBoxID;
int disk0_MBoxID;
int disk1_MBoxID;

process processTable[MAXPROC];
procPtr sleepQ;
procPtr disk0Q;
procPtr disk1Q;

static int	ClockDriver(char *);
static int	DiskDriver(char *);

void
start3(void)
{
    char	name[128];
    char    buf[10];
    int		i;
    int		clockPID;
    int     pid;
    int		diskPID0;
    int     diskPID1;
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

    sleepMBoxID = MboxCreate(1, 0);
    disk0_Q_MBoxID = MboxCreate(1, 0);
    disk1_Q_MBoxID = MboxCreate(1, 0);
    disk0_MBoxID = MboxCreate(1, 0);
    disk1_MBoxID = MboxCreate(1, 0);
    
    sleepQ = NULL;
    disk0Q = NULL;
    disk1Q = NULL;
    

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

    for (i = 0; i < USLOSS_DISK_UNITS; i++) {
        sprintf(buf, "%d", i);

        pid = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);

        if (i == 0) diskPID0 = pid;
        else diskPID1 = pid;

        if (pid < 0) {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }
    }

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
    
    MboxSend(disk0_MBoxID, NULL, 0);
    zap(diskPID0);  // disk driver

    MboxSend(disk1_MBoxID, NULL, 0);
    zap(diskPID1);  // disk driver

    // eventually, at the end:
    quit(0);

}





static int ClockDriver(char *arg)
{
    int result;
    int status;
    int currTime;
    procPtr ptr;

    // Let the parent know we are running and enable interrupts.
    semvReal(semRunning);
    int i = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    i++;

    // Infinite loop until we are zap'd
    while(!isZapped()) {
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




static int DiskDriver(char *arg)
{
    // initialization...
    int mboxID = -1;
    int mbox_Q_ID = -1;
    procPtr* diskQ_ptr = NULL;
    procPtr diskQ = NULL;
    int unit = -1;
    int i = 0;
    int result = -1;
    int status = -1;
    USLOSS_DeviceRequest request;
    


    if (*arg == '0') {
        unit = 0;
        mboxID = disk0_MBoxID;
        mbox_Q_ID = disk0_Q_MBoxID;
        diskQ_ptr = &disk0Q;
    }
    else {
        unit = 1;
        mboxID = disk1_MBoxID;
        mbox_Q_ID = disk1_Q_MBoxID;
        diskQ_ptr = &disk1Q;
    }




    while (!isZapped()) {
        MboxReceive(mboxID, NULL, 0);

        if (isZapped()) break;

        while (*diskQ_ptr != NULL) {
            diskQ = *diskQ_ptr;

            int mbox_id = diskQ->mbox_id;
            int track = diskQ->track;
            int firstSector = diskQ->first;
            int numSectors = diskQ->sectors;
            void* buffer = diskQ->buffer;
            int operation = diskQ->operation;

            // set disk's read/write head to correct track
            changeTrack(unit, track);


            if (operation == DISK_READ) request.opr = USLOSS_DISK_READ;
            else request.opr = USLOSS_DISK_WRITE;

            request.reg1 = (void*)(long) firstSector;
            request.reg2 = buffer;


            for (i = 0; i < numSectors; i++, request.reg1++, request.reg2 += USLOSS_DISK_SECTOR_SIZE) {

                if ((int)(long) request.reg1 == USLOSS_DISK_TRACK_SIZE) {
                    request.reg1 = 0;
                    track++;
                    changeTrack(unit, track);
                }


                result = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &request);
                result = waitDevice(USLOSS_DISK_DEV, unit, &status);
            }

            MboxSend(mbox_Q_ID, NULL, 0);
            *diskQ_ptr = (*diskQ_ptr)->nextDiskProc;
            MboxReceive(mbox_Q_ID, NULL, 0);

            MboxSend(mbox_id, NULL, 0);
        }
    }

    return 0;
}




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






void diskRead(USLOSS_Sysargs* args) {
    void* buffer = args->arg1;
    int unit = (int)(long) args->arg2;
    int track = (int)(long) args->arg3;
    int first = (int)(long) args->arg4;
    int sectors = (int)(long) args->arg5;

    args->arg5 = (void*)(long) diskReadReal(unit, track, first, sectors, buffer);
}


int diskReadReal(int unit, int track, int first, int sectors, void* buffer) {
    int pid = getpid();

    int mbox_Q_ID = -1;
    int mboxID = -1;

    if (unit == 0) {
        mbox_Q_ID = disk0_Q_MBoxID;
        mboxID = disk0_MBoxID;
    }
    else {
        mbox_Q_ID = disk1_Q_MBoxID;
        mboxID = disk1_MBoxID;
    }

    processTable[pid % MAXPROC].pid = pid;
    processTable[pid % MAXPROC].track = track;
    processTable[pid % MAXPROC].first = first;
    processTable[pid % MAXPROC].sectors = sectors;
    processTable[pid % MAXPROC].buffer = buffer;
    processTable[pid % MAXPROC].operation = DISK_READ;


    MboxSend(mbox_Q_ID, NULL, 0);
    addToDiskQ(unit, pid);
    MboxReceive(mbox_Q_ID, NULL, 0);

    MboxCondSend(mboxID, NULL, 0);


    MboxReceive(processTable[pid % MAXPROC].mbox_id, NULL, 0);

    return 0;
}






void diskWrite(USLOSS_Sysargs* args) {
    void* buffer = args->arg1;
    int unit = (int)(long) args->arg2;
    int track = (int)(long) args->arg3;
    int first = (int)(long) args->arg4;
    int sectors = (int)(long) args->arg5;

    args->arg5 = (void*)(long) diskWriteReal(unit, track, first, sectors, buffer);
}


int diskWriteReal(int unit, int track, int first, int sectors, void* buffer) {
    int pid = getpid();

    int mbox_Q_ID = -1;
    int mboxID = -1;

    if (unit == 0) {
        mbox_Q_ID = disk0_Q_MBoxID;
        mboxID = disk0_MBoxID;
    }
    else {
        mbox_Q_ID = disk1_Q_MBoxID;
        mboxID = disk1_MBoxID;
    }

    processTable[pid % MAXPROC].pid = pid;
    processTable[pid % MAXPROC].track = track;
    processTable[pid % MAXPROC].first = first;
    processTable[pid % MAXPROC].sectors = sectors;
    processTable[pid % MAXPROC].buffer = buffer;
    processTable[pid % MAXPROC].operation = DISK_WRITE;


    MboxSend(mbox_Q_ID, NULL, 0);
    addToDiskQ(unit, pid);
    MboxReceive(mbox_Q_ID, NULL, 0);

    MboxCondSend(mboxID, NULL, 0);


    MboxReceive(processTable[pid % MAXPROC].mbox_id, NULL, 0);

    return 0;
}





void diskSize(USLOSS_Sysargs* args) {
    int units = (int)(long) args->arg1;
    int* sector = (int*) args->arg2;
    int* track = (int*) args->arg3;
    int* disk = (int*) args->arg4;

    int result = diskSizeReal(units, sector, track, disk);

    args->arg5 = (void*)(long) result;
}


int diskSizeReal(int unit, int* sector, int* track, int* disk) {
    int status;

    *sector = USLOSS_DISK_SECTOR_SIZE;
    *track = USLOSS_DISK_TRACK_SIZE;

    USLOSS_DeviceRequest request;
    request.opr = USLOSS_DISK_TRACKS;
    request.reg1 = disk;

    int result = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &request);
    result = waitDevice(USLOSS_DISK_DEV, unit, &status);

    return result;
}











void setToUserMode() {
    int result = USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE);
    result++;
}

void initializeSysCallTable() {
    systemCallVec[SYS_SLEEP] = sleep;
    systemCallVec[SYS_DISKREAD] = diskRead;
    systemCallVec[SYS_DISKWRITE] = diskWrite;
    systemCallVec[SYS_DISKSIZE] = diskSize;
}

void initializeProcessTable() {
    int i = 0;

    for(i = 0; i < MAXPROC; i++){
        processTable[i].pid = 0;
        processTable[i].nextSleeperProc = NULL;
        processTable[i].nextDiskProc = NULL;
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

    if (sleepQ == NULL) return;

    if(sleepQ->pid == pid){
        sleepQ = sleepQ->nextSleeperProc;
        return;
    }

    ptr = sleepQ;

    while(ptr->nextSleeperProc != NULL){
        if(ptr->nextSleeperProc->pid == pid){
            ptr->nextSleeperProc = ptr->nextSleeperProc->nextSleeperProc;
            break;
        }
        ptr = ptr->nextSleeperProc;
    }
}



void addToDiskQ(int unit, int pid){
    procPtr ptr = NULL;

    if (unit == 0) {
        if (disk0Q == NULL) {
            disk0Q = &processTable[pid % MAXPROC];
            return;
        }

        ptr = disk0Q;
    }
    else {
        if (disk1Q == NULL) {
            disk1Q = &processTable[pid % MAXPROC];
            return;
        }

        ptr = disk1Q;
    }

    // still need to make it do a circular scan
    while(ptr->nextDiskProc != NULL){
        ptr = ptr->nextDiskProc;
    }

    ptr->nextDiskProc = &processTable[pid % MAXPROC];
}




void changeTrack(int unit, int track) {
    int result, status;
    USLOSS_DeviceRequest request;

    request.opr = USLOSS_DISK_SEEK;
    request.reg1 = (void*)(long) track;

    result = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &request);
    result = waitDevice(USLOSS_DISK_DEV, unit, &status);
}
