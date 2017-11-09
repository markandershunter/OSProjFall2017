#ifndef DRIVER
#define DRIVER

#define INVALID_SLEEP_TIME -1
#define DISK_READ       0
#define DISK_WRITE      1

typedef struct process process;
typedef struct process* procPtr;

struct process {
    int pid;
    procPtr nextSleeperProc;
    procPtr nextDiskProc;
    int startTime;
    int sleepEndTime;
    int track;
    int first;
    int sectors;
    void* buffer;
    int operation;
    int mbox_id;
};

extern void sleep(USLOSS_Sysargs* args);
extern int sleepReal(int seconds);

extern void diskRead(USLOSS_Sysargs* args);
extern int diskReadReal(int unit, int track, int first, int sectors, void* buffer);

extern void diskWrite(USLOSS_Sysargs* args);
extern int diskWriteReal(int unit, int track, int first, int sectors, void* buffer);

extern void diskSize(USLOSS_Sysargs* args);
extern int diskSizeReal(int unit, int* sector, int* track, int* disk);

extern void setToUserMode();

extern void initializeSysCallTable();
extern void initializeProcessTable();
extern void nullsys3();

extern void addToSleepQ(int pid);
extern void removeFromSleepQ(int pid);

extern void addToDiskQ(int unit, int pid);

extern void changeTrack(int unit, int track);

#endif
