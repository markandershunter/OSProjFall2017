/* ------------------------------------------------------------------------
   phase1.c

   University of Arizona
   Computer Science 452
   Fall 2015

   ------------------------------------------------------------------------ */

#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();


/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 0;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static procPtr ReadyList = NULL;

// current process ID
procPtr Current;

// the next pid to be assigned
unsigned int nextPid = SENTINELPID;


/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
             Start up sentinel process and the test process.
   Parameters - argc and argv passed in by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup(int argc, char *argv[])
{
    int result; /* value returned by call to fork1() */
    int i = 0;

    /* initialize the process table */
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
    for (i = 0; i < MAXPROC; i++) {
        ProcTable[i].stack = NULL;
        cleanProcess(&ProcTable[i]);
    }

    // Initialize the Ready list, etc.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing the Ready list\n");

    ReadyList = NULL;

    // Initialize the clock interrupt handler
    // not correct yet
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;
    USLOSS_IntVec[USLOSS_ALARM_INT] = alarmHandler;
    USLOSS_IntVec[USLOSS_DISK_INT] = diskHandler;
    USLOSS_IntVec[USLOSS_TERM_INT] = terminalHandler;
    USLOSS_IntVec[USLOSS_MMU_INT] = mmuHandler;
    USLOSS_IntVec[USLOSS_SYSCALL_INT] = syscallHandler;
    USLOSS_IntVec[USLOSS_ILLEGAL_INT] = illegalHandler;

    // startup a sentinel process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                    SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG && debugflag) {
            USLOSS_Console("startup(): fork1 of sentinel returned error, ");
            USLOSS_Console("halting...\n");
        }
        USLOSS_Halt(1);
    }

    // start the test process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for start1\n");
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error, ");
        USLOSS_Console("halting...\n");
        USLOSS_Halt(1);
    }

    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1\n");

    return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish(int argc, char *argv[])
{
    if (DEBUG && debugflag)
        USLOSS_Console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg,
          int stacksize, int priority)
{
    int procSlot = -1;
    int i = 0;

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process %s\n", name);



    // test if in kernel mode (1); halt if in user mode (0)
    if (!(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)) {
        if (Current != NULL) USLOSS_Console("fork1(): called while in user mode, by process %d. Halting...\n", Current->pid);
        else USLOSS_Console("fork1(): called while in user mode. Halting...\n");

        USLOSS_Halt(1);
    }


    // Return if stack size is too small
    if (stacksize < USLOSS_MIN_STACK) {
        USLOSS_Console("fork1(): Stack size too small.\n");
        return STACK_SIZE_TOO_SMALL;
    }


    // Is there room in the process table? What is the next PID?
    for (i = 0; i < MAXPROC; i++) {
        if (ProcTable[nextPid % MAXPROC].status != UNUSED) nextPid++;
        else break;
    }
    if (i == MAXPROC) {
        USLOSS_Console("fork1(): Processtable full.\n");
        return PROCESS_TABLE_FULL;
    }


    // check priority
    if (priority > MINPRIORITY || priority < MAXPRIORITY) {
        if (priority != SENTINELPRIORITY || strcmp("sentinel", name) != 0) {
            USLOSS_Console("fork1(): Priority out of range.\n");
            return PRIORITY_OUT_OF_RANGE;
        }
    }


    // check startFunc
    if (startFunc == NULL) {
        USLOSS_Console("fork1(): startFunc is NULL.\n");
        return STARTFUNC_NULL;
    }


    // check name
    if (name == NULL) {
        USLOSS_Console("fork1(): name is NULL.\n");
        return NAME_NULL;
    }


    if ( strlen(name) >= (MAXNAME - 1) ) {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }

    ////////////////////////////////////////////////////
    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): done with error checking\n");

    // fill-in entry in process table

    procSlot = nextPid % MAXPROC;

    strcpy(ProcTable[procSlot].name, name);         // set the process name
    ProcTable[procSlot].startFunc = startFunc;      // set the start function

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): 1\n");


    // set the argument to the start function
    if ( arg == NULL ) {
        ProcTable[procSlot].startArg[0] = '\0';
    }
    else if ( strlen(arg) >= (MAXARG - 1) ) {
        USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else {
        strcpy(ProcTable[procSlot].startArg, arg);
    }

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): 2\n");


    ProcTable[procSlot].childProcPtr = NULL;
    ProcTable[procSlot].nextSiblingPtr = NULL;
    ProcTable[procSlot].nextProcPtr = NULL;

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): 3\n");

    // set pointers so that parent knows where its child is
    if (Current != NULL){
        ProcTable[procSlot].parentPtr = Current;
        ProcTable[procSlot].parentPid = Current->pid;
        if(Current->childProcPtr == NULL) {
            Current->childProcPtr = &ProcTable[procSlot];
        }
        else {
            procPtr temp = Current->childProcPtr;

            while (temp->nextSiblingPtr != NULL) {
                temp = temp->nextSiblingPtr;
            }
            temp->nextSiblingPtr = &ProcTable[procSlot];
            ProcTable[procSlot].prevSiblingPtr = temp;
        }
        Current->childCount++;
    }

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): 4\n");



    ProcTable[procSlot].pid = nextPid;              // set pid

    ProcTable[procSlot].priority = priority;        // set priority

    ProcTable[procSlot].totalExecutionTime = 0;
    ProcTable[procSlot].lastReadTime = 0;
    ProcTable[procSlot].startTimeSlice = -1;
    ProcTable[procSlot].exitTimeSlice = -1;

    ProcTable[procSlot].stackSize = stacksize;      // set stackSize

    ProcTable[procSlot].stack = malloc(stacksize);  // set stack
    if (ProcTable[procSlot].stack == NULL) {
        USLOSS_Console("fork1(): Out of memory for stack.\n");
        return OUT_OF_MEMORY;
    }

    ProcTable[procSlot].status = READY;             // set READY status

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): 5\n");

    addToReadyList(&ProcTable[procSlot]);

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): 6\n");


    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)

    enableInterrupts();

    USLOSS_ContextInit(&(ProcTable[procSlot].state),
                       ProcTable[procSlot].stack,
                       ProcTable[procSlot].stackSize,
                       NULL,
                       launch);

    // for future phase(s)
    p1_fork(ProcTable[procSlot].pid);

    // More stuff to do here...

    // call dispatcher for everyone except Sentinel
    if (priority != SENTINELPRIORITY) {
        dispatcher();
    }

    return nextPid++;
} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
    int result;

    if (DEBUG && debugflag)
        USLOSS_Console("launch(): started\n");

    // Enable interrupts
    enableInterrupts();


    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugflag)
        USLOSS_Console("Process %d returned to launch\n", Current->pid);

    quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
             -1 if the process was zapped in the join
             -2 if the process has no children
   Side Effects - If no child process has quit before join is called, the
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *status)
{
    procPtr childPtr = Current->childProcPtr;
    procPtr earliest = NULL;

    int earliestExitTime = 0;
    int i = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &earliestExitTime);


    // test if in kernel mode (1); halt if in user mode (0)
    if (!(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)) {
        USLOSS_Console("join(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }



    if (DEBUG && debugflag) {
        USLOSS_Console("join(): %s joining with children...\n", Current->name);
        USLOSS_Console("join(): Current time: %d\n", earliestExitTime);
    }



    // Check if the current parent process has any children
    if (childPtr == NULL) {
        USLOSS_Console("join(): No child processes exist\n");
        return NO_CHILD_PROCESSES;
    }

    while (childPtr != NULL) {
        if (DEBUG && debugflag) {
            USLOSS_Console("join(): %s exit time is %d\n", childPtr->name, childPtr->exitTimeSlice);
        }

        // Check if the childProc has quit and is also the earliest to quit so far
        if (childPtr->status == QUIT && childPtr->exitTimeSlice < earliestExitTime) {
            if (DEBUG && debugflag) {
                USLOSS_Console("join(): %s found child %s has quit\n", Current->name, childPtr->name);
            }

            earliest = childPtr;
            earliestExitTime = childPtr->exitTimeSlice;
        }

        childPtr = childPtr->nextSiblingPtr;

        if (DEBUG && debugflag) {
            USLOSS_Console("join(): checking next child...\n");
        }
    }


    // remove process from sibling list and set up return values
    if (earliest != NULL) {
        *status = earliest->exitCode;
        i = earliest->pid;

        if (DEBUG && debugflag) {
            USLOSS_Console("join(): reading quit info from %s\n", earliest->name);
        }

        if (earliest->prevSiblingPtr == NULL) {
            if (DEBUG && debugflag) {
                USLOSS_Console("join(): %s was the first child in the list\n", earliest->name);
            }

            Current->childProcPtr = earliest->nextSiblingPtr;
            if (earliest->nextSiblingPtr != NULL) earliest->nextSiblingPtr->prevSiblingPtr = NULL;
        }
        else {
            earliest->prevSiblingPtr->nextSiblingPtr = earliest->nextSiblingPtr;
            earliest->nextSiblingPtr->prevSiblingPtr = earliest->prevSiblingPtr;
        }

        cleanProcess(earliest);

        return i;
    }

    // no children who have quit yet, so block and call dispatcher
    else {
        if (DEBUG && debugflag)
            USLOSS_Console("join(): %s blocking on join...\n", Current->name);

        Current->status = BLOCKED;
        readtime();

        dispatcher();

        if (DEBUG && debugflag)
            USLOSS_Console("join(): %s woken up\n", Current->name);
    }

    // remove child who has just quit from list of children
    *status = Current->childQuitPtr->exitCode;

    if (DEBUG && debugflag)
        USLOSS_Console("join(): exit code saved and status set to UNUSED\n");



    // child who quit was first in the list of children
    if (Current->childQuitPtr->prevSiblingPtr == NULL) {
        if (DEBUG && debugflag)
            USLOSS_Console("join(): quit child had no previous siblings\n");

        Current->childProcPtr = Current->childQuitPtr->nextSiblingPtr;

        if (Current->childProcPtr != NULL) Current->childProcPtr->prevSiblingPtr = NULL;
    }
    else {
        Current->childQuitPtr->prevSiblingPtr->nextSiblingPtr = Current->childQuitPtr->nextSiblingPtr;
        Current->childQuitPtr->nextSiblingPtr->prevSiblingPtr = Current->childQuitPtr->prevSiblingPtr;
    }

    i = Current->childQuitPtr->pid;

    cleanProcess(Current->childQuitPtr);

    return i;
} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int status)
{
    // test if in kernel mode (1); halt if in user mode (0)
    if (!(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)) {
        USLOSS_Console("quit(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }

    if (Current->childProcPtr != NULL) {
        USLOSS_Console("quit(): process %d, '%s', has active children. Halting...\n", Current->pid, Current->name);
        USLOSS_Halt(1);
    }

    if (DEBUG && debugflag)
        USLOSS_Console("quit(): %s quitting...\n", Current->name);

    free(Current->stack);
    Current->status = QUIT;
    Current->exitCode = status;

    int i = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &(Current->exitTimeSlice));
    i++;

    Current->totalExecutionTime = Current->totalExecutionTime + 
        (Current->exitTimeSlice - Current->lastReadTime);

    p1_quit(Current->pid);

    if (Current->parentPtr != NULL) {
        Current->parentPtr->childCount = Current->parentPtr->childCount - 1;

        if (Current->parentPtr->status == BLOCKED) {
            Current->parentPtr->status = READY;
            addToReadyList(Current->parentPtr);

            Current->parentPtr->childQuitPtr = Current;
        }
    }

    dispatcher();
} /* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
    // test if in kernel mode (1); halt if in user mode (0)
    if (!(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)) {
        if (Current != NULL) USLOSS_Console("dispatcher(): called while in user mode, by process %d. Halting...\n", Current->pid);
        else USLOSS_Console("dispatcher(): called while in user mode. Halting...\n");

        USLOSS_Halt(1);
    }

    if (DEBUG && debugflag) {
        USLOSS_Console("dispatcher(): entering dispatcher\n");
        printReadyList();
    }

    procPtr oldProcess = NULL;


    // check if this is the first process that is being run
    if (Current == NULL) {
        Current = ReadyList;
        Current->status = RUNNING;
        ReadyList = ReadyList->nextProcPtr;

        if (DEBUG && debugflag)
            USLOSS_Console("%s now running\n", Current->name);

        p1_switch('\0', Current->pid);
        enableInterrupts();
        USLOSS_ContextSwitch(NULL, &Current->state);
        
        int i = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &(Current->startTimeSlice));
        i++;

        Current->lastReadTime = Current->startTimeSlice;
    }

    // check if a higher priority process has been added to the ReadyList
    else if (ReadyList->priority < Current->priority) {
        if (DEBUG && debugflag)
            USLOSS_Console("%s has a higher priority than %s, %s will now run\n", ReadyList->name, Current->name, ReadyList->name);

        // add Current back to ReadyList if it hasn't yet quit
        if (Current->status != QUIT) {
            Current->status = READY;
            addToReadyList(Current);
        }
        else if (DEBUG && debugflag) printReadyList();

        oldProcess = Current;
        Current = ReadyList;
        Current->status = RUNNING;
        ReadyList = ReadyList->nextProcPtr;

        if (DEBUG && debugflag)
            USLOSS_Console("%s now running\n", Current->name);

        p1_switch(oldProcess->pid, Current->pid);
        enableInterrupts();
        USLOSS_ContextSwitch(&oldProcess->state, &Current->state);
        
        int i = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &(Current->startTimeSlice));
        i++;

        Current->lastReadTime = Current->startTimeSlice;

        if (DEBUG && debugflag)
            USLOSS_Console("dispatcher(): start time is %d", Current->startTimeSlice);
    }

    // is Current higher than all other processes?
    else if (Current->priority < ReadyList->priority) {

        if (Current->status == QUIT || Current->status == BLOCKED) {

            if (DEBUG && debugflag && Current->status == QUIT)
                USLOSS_Console("%s handing off to %s because %s is quitting\n", Current->name,ReadyList->name, Current->name);
            if (DEBUG && debugflag && Current->status == BLOCKED) {
                USLOSS_Console("%s handing off to %s because %s is blocked\n", Current->name,ReadyList->name, Current->name);
            }

            oldProcess = Current;
            Current = ReadyList;
            Current->status = RUNNING;
            ReadyList = ReadyList->nextProcPtr;

            p1_switch(oldProcess->pid, Current->pid);
            enableInterrupts();
            USLOSS_ContextSwitch(&oldProcess->state, &Current->state);
        }
        else {
            // do nothing, let the process continue to run because it is higher than
            // everyone else
        }

    }

    // Current is at the same priority as the next highest process
    else {
        // check if time slice has been reached and Current still needs to run
        if (0 && Current->status != QUIT) {
            if (DEBUG && debugflag)
                USLOSS_Console("%s handing off to %s because %s is time-sliced\n", Current->name,ReadyList->name, Current->name);

            oldProcess = Current;
            Current->status = READY;

            Current = ReadyList;
            Current->status = RUNNING;
            
            ReadyList = ReadyList->nextProcPtr;

            p1_switch(oldProcess->pid, Current->pid);
            enableInterrupts();
            USLOSS_ContextSwitch(&oldProcess->state, &Current->state);
        }
    }

} /* dispatcher */




void  dumpProcesses(void) {
    int i = 0;
    procPtr p = NULL;
    char status[19];

    // test if in kernel mode (1); halt if in user mode (0)
    if (!(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)) {
        USLOSS_Console("dumpProcesses(): called while in user mode, by process %d. Halting...\n", Current->pid);
        USLOSS_Halt(1);
    }

    readtime();


    USLOSS_Console("%5s%11s%10s%15s%13s%10s%12s\n", "PID", "ParentPID", "Priority",  "Status", "Child Count", "CPU Time", "Name");

    for (i = 0; i < MAXPROC; i++) {
        p = &ProcTable[i];
        strcpy(status, statusMatcher(p->status));
        USLOSS_Console("%5d%11d%10d%15s%13d%10d%12s\n", p->pid, p->parentPid, p->priority,
            status, p->childCount, p->totalExecutionTime, p->name);
    }
}





int zap(int pid) {
    return 0;
}





char* statusMatcher(int status) {
    switch(status){
        case 0:
            return "UNUSED";
        case 1:
            return "READY";
        case 2:
            return "BLOCKED";
        case 3:
            return "QUIT";
        case 4:
            return "RUNNING";
        default:
            return "UNKNOWN";
    }
}






int getpid() {
    return Current->pid;
}





/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
             processes are blocked.  The other is to detect and report
             simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
                   and halt.
   ----------------------------------------------------------------------- */

int sentinel (char *dummy)
{
    if (DEBUG && debugflag)
        USLOSS_Console("sentinel(): called\n");
    while (1)
    {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{
    int i = 2;

    if (ProcTable[0].status != UNUSED) {
        USLOSS_Console("All processes completed.\n");
        USLOSS_Halt(0);
    }

    // skip process 1, sentinel

    for (i = 2; i < MAXPROC; i++) {
        if (ProcTable[i].status != UNUSED) {
            USLOSS_Console("All processes completed.\n");
            USLOSS_Halt(0);
        }
    }

    USLOSS_Console("Error: Some processes still running.\n");
    USLOSS_Halt(1);
} /* checkDeadlock */



void enableInterrupts() {
    // compiler gives warning if the return value of USLOSS_... is ignored,
    // hence the int i = ... Also it gives a warning if i is unused, hence
    // the i++;
    int i = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    i++;
}


/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
    // turn the interrupts OFF iff we are in kernel mode
    // if not in kernel mode, print an error message and
    // halt USLOSS

} /* disableInterrupts */



void addToReadyList(procPtr proc) {
    procPtr curr = ReadyList;
    procPtr prev = NULL;

    if (DEBUG && debugflag)
        USLOSS_Console("addToReadyList(): adding %s to ready list\n", proc->name);

    if (ReadyList == NULL) {
        ReadyList = proc;
        return;
    }

    if (DEBUG && debugflag)
        USLOSS_Console("addToReadyList(): 1\n");

    // ReadyList is not empty


    // proc belongs at the head of the ready list because it
    // is the highest priority
    if (ReadyList->priority > proc->priority) {
        proc->nextProcPtr = ReadyList;
        ReadyList = proc;
        return;
    }

    // proc belongs somewhere in the middle/at the end of the ready
    // list
    while (curr != NULL && curr->priority <= proc->priority) {
        prev = curr;
        curr = curr->nextProcPtr;
    }

    // add to end of ReadyList
    if (curr == NULL) {
        prev->nextProcPtr = proc;
    }

    // add to end of its priority
    else {
        proc->nextProcPtr = curr;
        prev->nextProcPtr = proc;
    }


    // print readyList
    if (DEBUG && debugflag) {
        printReadyList();
    }
}



void printReadyList() {
    procPtr curr = ReadyList;
    USLOSS_Console("Printing contents of ready list:\n");
    while (curr != NULL) {
        USLOSS_Console("\t%s in ready list\n", curr->name);
        curr = curr->nextProcPtr;
    }
}





/*
*   Return the CPU time (in ms) used by the current process
*/
int readtime(){
    int temp = Current->lastReadTime;

    int i = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &(Current->lastReadTime));
    i++;

    if (DEBUG && debugflag) {
        USLOSS_Console("readtime(): current total: %d\n", Current->totalExecutionTime);
        USLOSS_Console("readtime(): last read time: %d\n", temp);
        USLOSS_Console("readtime(): time right now: %d\n", Current->lastReadTime);
    }

    Current->totalExecutionTime = Current->totalExecutionTime +
        (Current->lastReadTime - temp);

    // return 6;
    return Current->totalExecutionTime;
}




void cleanProcess(procPtr p) {
    p->nextProcPtr = NULL;
    p->childProcPtr = NULL;
    p->prevSiblingPtr = NULL;
    p->nextSiblingPtr = NULL;
    p->parentPtr = NULL;
    p->pid = -1;               /* process id */
    p->priority = -1;
    p->startFunc = NULL;   /* function where process begins -- launch */
    p->stackSize = 0;
    p->status = UNUSED;
    p->startTimeSlice = -1;
    p->exitTimeSlice = -1;
    p->lastReadTime = 0;
    p->totalExecutionTime = 0;
    p->parentPid = -1;
    p->childCount = 0;
    strcpy(p->name, "");
}



void clockHandler (int interruptType, void* arg) {}
void alarmHandler (int interruptType, void* arg) {}
void diskHandler (int interruptType, void* arg) {}
void terminalHandler (int interruptType, void* arg) {}
void mmuHandler (int interruptType, void* arg) {}
void syscallHandler (int interruptType, void* arg) {}
void illegalHandler (int interruptType, void* arg) {}
