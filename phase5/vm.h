/*
 * vm.h
 */

// status codes for frame fields
#define NOT_USED        0
#define USED            1
#define NOT_RECENTLY    0
#define RECENT          1
#define NO_OWNER        -1
#define CLEAN_ON_DISK   0
#define CLEAN_BLANK     1




/*
 * All processes use the same tag.
 */
#define TAG 0

/*
 * Different states for a page.
 */ 
#define UNUSED          500
#define ON_DISK         501
#define IN_PAGE_TABLE   502
#define BLANK           503

#define NO_FRAME        -1



typedef struct Frame {
    int used;           // is this frame being used
    int ownerPid;       // which process owns this frame
    int pageNumber;     // page that is in this frame
    int dirty;          // is new data in this frame that's not on disk?
} Frame;


/*
 * Page table entry.
 */
typedef struct PTE {
    int  state;      // See above.
    int  frame;      // Frame that stores the page (if any). -1 if none.
    int  diskBlock;  // Disk block that stores the page (if any). -1 if none.
    // Add more stuff here
} PTE;

/*
 * Per-process information.
 */
typedef struct Process {
    int  numPages;   // Size of the page table.
    PTE  *pageTable; // The page table for the process.
    // Add more stuff here */
} Process;

/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
typedef struct FaultMsg {
    int  pid;        // Process with the problem.
    void *addr;      // Address that caused the fault.
    int  replyMbox;  // Mailbox to send reply.
    // Add more stuff here.
} FaultMsg;


extern  int  start5(char *);

void* vmInitReal(int mappings, int pages, int frames, int pagers);
void vmDestroyReal(void);

void PrintStats(void);
int findFrameNumber(void);


#define CheckMode() assert(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)
