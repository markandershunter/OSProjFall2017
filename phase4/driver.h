#ifndef DRIVER
#define DRIVER

#define INVALID_SLEEP_TIME -1;

typedef struct process process;
typedef struct process* procPtr;

struct process {
    int pid;
    procPtr nextSleeperProc;
    int startTime;
    int sleepEndTime;
    int mbox_id;
};

extern void sleep(USLOSS_Sysargs* args);
extern int sleepReal(int seconds);

extern void setToUserMode();

extern void initializeSysCallTable();
extern void initializeProcessTable();
extern void nullsys3();

extern void addToSleepQ(int pid);

#endif
