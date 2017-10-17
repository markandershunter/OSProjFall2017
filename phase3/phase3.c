#include <stdio.h>
#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>

#include "sems.h"


int (* startFuncGlobal) (char *);
int parentPidGlobal;

process processTable[MAXPROC];


int start2(char *arg)
{
    int pid;
    int status;
    int i;
    /*
     * Check kernel mode here.
     */
     checkKernelMode("start2");

    /*
     * Data structure initialization as needed...
     */
    // process table
    initializeProcessTable();


    // semaphore table

    // sysCall table
    for (i = 0; i < USLOSS_MAX_SYSCALLS; i++) {
        systemCallVec[i] = nullsys3;
    }
    systemCallVec[SYS_SPAWN]        = spawn;
    systemCallVec[SYS_WAIT]         = wait;
    systemCallVec[SYS_TERMINATE]    = terminate;
    // systemCallVec[SYS_SEMCREATE]    = nullsys3;
    // systemCallVec[SYS_SEMP]         = nullsys3;
    // systemCallVec[SYS_SEMV]         = nullsys3;
    // systemCallVec[SYS_SEMFREE]      = nullsys3;
    // systemCallVec[SYS_GETTIMEOFDAY] = nullsys3;
    // systemCallVec[SYS_CPUTIME]      = nullsys3;
    // systemCallVec[SYS_GETPID]       = nullsys3;


    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * Assumes kernel-mode versions of the system calls
     * with lower-case names.  I.e., Spawn is the user-mode function
     * called by the test cases; spawn is the kernel-mode function that
     * is called by the syscallHandler; spawnReal is the function that
     * contains the implementation and is called by spawn.
     *
     * Spawn() is in libuser.c.  It invokes USLOSS_Syscall()
     * The system call handler calls a function named spawn() -- note lower
     * case -- that extracts the arguments from the sysargs pointer, and
     * checks them for possible errors.  This function then calls spawnReal().
     *
     * Here, we only call spawnReal(), since we are already in kernel mode.
     *
     * spawnReal() will create the process by using a call to fork1 to
     * create a process executing the code in spawnLaunch().  spawnReal()
     * and spawnLaunch() then coordinate the completion of the phase 3
     * process table entries needed for the new process.  spawnReal() will
     * return to the original caller of Spawn, while spawnLaunch() will
     * begin executing the function passed to Spawn. spawnLaunch() will
     * need to switch to user-mode before allowing user code to execute.
     * spawnReal() will return to spawn(), which will put the return
     * values back into the sysargs pointer, switch to user-mode, and 
     * return to the user code that called Spawn.
     */
    pid = spawnReal("start3", start3, NULL, 4*USLOSS_MIN_STACK, 3);

    /* Call the waitReal version of your wait code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */
    pid = waitReal(&status);

    return 0;

} /* start2 */





void nullsys3() {

}




void spawn(USLOSS_Sysargs* args) {
    // check for illegal values
    args->arg1 = (void*) spawnReal(args->arg5, args->arg1, args->arg2, (int)args->arg3, (int)args->arg4);
    setToUserMode();
    return;
}

long spawnReal(char* name, int(*startFunc)(char *), void* arg, int stackSize, int priority) {
    int pid = -1;
    
    startFuncGlobal = startFunc;
    parentPidGlobal = getpid();
    
    pid = fork1(name, spawnLaunch, arg, stackSize, priority);

    if (!processTable[pid % MAXPROC].entryMade) {
        processTable[pid % MAXPROC].pid = pid;
        processTable[pid % MAXPROC].entryMade = 1;
        processTable[pid % MAXPROC].status = USED;
        processTable[pid % MAXPROC].parentPtr = &processTable[getpid() % MAXPROC];
        addToChildList(getpid(), pid);

    }

    return pid;
}

int spawnLaunch(char* arg) {
    int returnValue =-1;

    if (!processTable[getpid() % MAXPROC].entryMade) {
        processTable[getpid() % MAXPROC].pid = getpid();
        processTable[getpid() % MAXPROC].entryMade = 1;
        processTable[getpid() % MAXPROC].status = USED;
        processTable[getpid() % MAXPROC].parentPtr = &processTable[parentPidGlobal % MAXPROC];
        addToChildList(parentPidGlobal, getpid());
    }

    setToUserMode();
    returnValue = startFuncGlobal(arg);

    setToKernelMode();

    return returnValue;
}



void wait(USLOSS_Sysargs* args) {
    long status = -1;

    args->arg1 = (void*) waitReal((int*) &status);
    args->arg2 = (void*) status;
    setToUserMode();
    return;
}

long waitReal(int* status) {
    return join(status);
}



void terminate(USLOSS_Sysargs* args) {
    terminateReal((int)args->arg1);
    setToUserMode();
    return;
}

void terminateReal(int status) {
    int pid = -1;
    int runStatus = -1;

    procPtr child = processTable[getpid() % MAXPROC].childPtr;

    while (child != NULL) {
        pid = child->pid;
        runStatus = child->status;
        child = child->nextSiblingPtr;

        if (runStatus != TERMINATED) {
            zap(pid);
        }
    }

    processTable[getpid() % MAXPROC].status = TERMINATED;
    quit(status);

    return;
}








void initializeProcessTable() {
    int i = 0;

    for (i = 0; i < MAXPROC; i++) {
        processTable[i].status = UNUSED;
        processTable[i].parentPtr = NULL;
        processTable[i].childPtr = NULL;
        processTable[i].nextSiblingPtr = NULL;
        processTable[i].entryMade = 0;
    }
}


void checkKernelMode(char* name){
    if (!(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)) {
        USLOSS_Console("%s called while in user mode. Halting...\n", name);
        USLOSS_Halt(1);
    }
}


void setToUserMode() {
    int result = USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE);
    result++;
}


void setToKernelMode() {
    int result = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_MODE);
    result++;
}


void addToChildList(int parentPid, int childPid) {
    procPtr current = processTable[parentPid % MAXPROC].childPtr;

    if (current == NULL) {
        processTable[parentPid % MAXPROC].childPtr = &processTable[childPid % MAXPROC];
        return;
    }

    while (current->nextSiblingPtr != NULL) {
        current = current->nextSiblingPtr;
    }

    current->nextSiblingPtr = &processTable[childPid % MAXPROC];
}

