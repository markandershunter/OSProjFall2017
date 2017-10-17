#ifndef SEMS_H
#define SEMS_H


#define UNUSED      0
#define USED        1
#define TERMINATED  2

typedef struct process  process;
typedef struct process* procPtr;

struct process {
    int             pid;
    int             status;
    procPtr         parentPtr;
    procPtr         childPtr;
    procPtr         nextSiblingPtr;
    int             entryMade;      // has the process table entry been made yet
};

int start3(char* arg);




// syscall functions
void    nullsys3();

void    spawn(USLOSS_Sysargs*);
long    spawnReal(char* name, int(*startFunc)(char *), void* arg, int stackSize, int priority);
int     spawnLaunch(char*);

void    wait(USLOSS_Sysargs* args);
long    waitReal(int* status);

void    terminate(USLOSS_Sysargs* args);
void    terminateReal(int status);


// helper functions
void initializeProcessTable();
void checkKernelMode(char* name);
void setToUserMode();
void addToChildList(int parentPid, int childPid);

#endif