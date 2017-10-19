#ifndef SEMS_H
#define SEMS_H


#define UNUSED      0
#define USED        1
#define TERMINATED_FROM_SEMFREE     1
#define TERMINATED  2
#define FREED       3

#define VALID               0
#define INVALID             -1
#define PROCESSES_BLOCKED   1

typedef struct process  process;
typedef struct process* procPtr;
typedef struct semaphore semaphore;

struct process {
    int             pid;
    int             status;
    procPtr         parentPtr;
    procPtr         childPtr;
    procPtr         nextSiblingPtr;
    procPtr         nextSemBlockedSiblingPtr;   // other processes blocked on the same semaphore
    int             entryMade;      // has the process table entry been made yet
    int             mboxID;         // every semaphore has a mailbox so that it can block processes
};


struct semaphore {
    int         status;
    int         value;
    procPtr     blockedProcessPtr; // pointer to the first process blocked on the semaphore
};



int start3(char* arg);




// syscall functions
void    nullsys3();

void    spawn(USLOSS_Sysargs*);
long    spawnReal(char* name, int(*startFunc)(char *), void* arg, long stackSize, long priority);
int     spawnLaunch(char*);

void    wait(USLOSS_Sysargs* args);
long    waitReal(int* status);

void    terminate(USLOSS_Sysargs* args);
void    terminateReal(long status);

void    semCreate(USLOSS_Sysargs* args);
long    semCreateReal(long value, long* status);

void    semP(USLOSS_Sysargs* args);
long    semPReal(long semNumber);

void    semV(USLOSS_Sysargs* args);
long    semVReal(long semNumber);

void    semFree(USLOSS_Sysargs* args);
long    semFreeReal(long semNumber);

void    getTimeOfDay(USLOSS_Sysargs* args);
long    getTimeOfDayReal();

void    cpuTime(USLOSS_Sysargs* args);
long    cpuTimeReal();

void    getPid(USLOSS_Sysargs* args);
long    getPidReal();



// helper functions
void initializeProcessTable();
void initializeSemaphoreTable();
void initializeSysCallTable();
void checkKernelMode(char* name);
void setToUserMode();
void setToKernelMode();
void addToChildList(int parentPid, int childPid);
void printBlockedList(procPtr p);
void addToBlockedList(int semNumber);
int getNextSemIndex();

#endif