/* Patrick's DEBUG printing constant... */
#define DEBUG 1

typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
   procPtr         nextProcPtr;
   procPtr         childProcPtr;
   procPtr         prevSiblingPtr;
   procPtr         nextSiblingPtr;
   procPtr         parentPtr;
   procPtr         zappedPtr;
   procPtr         zappedSiblingPtr;
   procPtr         childQuitPtr;      /* the child who quit after join */
   char            name[MAXNAME];     /* process's name */
   char            startArg[MAXARG];  /* args passed to process */
   USLOSS_Context  state;             /* current context for process */
   short           pid;               /* process id */
   int             priority;
   int (* startFunc) (char *);   /* function where process begins -- launch */
   char           *stack;
   unsigned int    stackSize;
   int             status;        /* READY, BLOCKED, QUIT, etc. */
   int             exitCode;

   /* other fields as needed... */
   int             startTimeSlice;       /* slice start time */
   int             exitTimeSlice;        /* time that a process quit */
   int             lastReadTime;         /* the last time that the time was read for a process */
   int             totalExecutionTime;
   int             parentPid;
   int             childCount;
   int             zap;                  /* has the process been zapped yet */
};

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
   struct psrBits bits;
   unsigned int integerPart;
};




void addToReadyList(procPtr);
void cleanProcess(procPtr);
void addToZappedList(procPtr);
void printZappedList(procPtr);

void clockHandler (int interruptType, void* arg);
void alarmHandler (int interruptType, void* arg);
void diskHandler (int interruptType, void* arg);
void terminalHandler (int interruptType, void* arg);
void mmuHandler (int interruptType, void* arg);
void syscallHandler (int interruptType, void* arg);
void illegalHandler (int interruptType, void* arg);







/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS  NULL
#define MINPRIORITY         5
#define MAXPRIORITY         1
#define SENTINELPID         1
#define SENTINELPRIORITY    (MINPRIORITY + 1)

#define STACK_SIZE_TOO_SMALL    -2
#define PROCESS_TABLE_FULL      -1
#define PRIORITY_OUT_OF_RANGE   -1
#define STARTFUNC_NULL          -1
#define NAME_NULL               -1
#define OUT_OF_MEMORY           -1
#define NO_CHILD_PROCESSES      -2

#define UNUSED      0
#define READY       1
#define QUIT        3
#define RUNNING     4
#define JOIN_BLOCK    5
#define ZAP_BLOCK     6

#define ZAPPED_WHILE_JOINING        -1
#define ZAPPED_WHILE_ZAPPING        -1
#define ZAPPED_WHILE_BLOCKME        -1
#define ZAPPED_WHILE_UNBLOCKING     -1

#define ZAP_PROC_QUIT       -1
#define ZAP_OK              0
#define BLOCKME_OK          0
#define UNBLOCK_PROC_OK     0

#define UNBLOCK_PROC_ERROR      -2
