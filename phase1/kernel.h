/* Patrick's DEBUG printing constant... */
#define DEBUG 1

typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
   procPtr         nextProcPtr;
   procPtr         childProcPtr;
   procPtr         nextSiblingPtr;
   char            name[MAXNAME];     /* process's name */
   char            startArg[MAXARG];  /* args passed to process */
   USLOSS_Context  state;             /* current context for process */
   short           pid;               /* process id */
   int             priority;
   int (* startFunc) (char *);   /* function where process begins -- launch */
   char           *stack;
   unsigned int    stackSize;
   int             status;        /* READY, BLOCKED, QUIT, etc. */

   /* other fields as needed... */
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




void  addToReadyList(procPtr proc);

void clockHandler (int interruptType, void* arg);
void alarmHandler (int interruptType, void* arg);
void diskHandler (int interruptType, void* arg);
void terminalHandler (int interruptType, void* arg);
void mmuHandler (int interruptType, void* arg);
void syscallHandler (int interruptType, void* arg);
void illegalHandler (int interruptType, void* arg);







/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY (MINPRIORITY + 1)

#define STACK_SIZE_TOO_SMALL -2
#define PROCESS_TABLE_FULL -1
#define PRIORITY_OUT_OF_RANGE -1
#define STARTFUNC_NULL -1
#define NAME_NULL -1
#define OUT_OF_MEMORY -1

#define UNUSED 0
#define READY 1
#define BLOCKED 2
#define QUIT 3

