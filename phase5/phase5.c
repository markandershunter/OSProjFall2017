/*
 * skeleton.c
 *
 * This is a skeleton for phase5 of the programming assignment. It
 * doesn't do much -- it is just intended to get you started.
 */


#include <usyscall.h>
#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <libuser.h>
#include <providedPrototypes.h>
#include <vm.h>
#include <string.h>



Process processes[MAXPROC];
Frame*  frameTable;

FaultMsg faults[MAXPROC]; /* Note that a process can have only
                           * one fault at a time, so we can
                           * allocate the messages statically
                           * and index them by pid. */
int nextFaultMsgLocation = 0;
int faultMBoxID;

VmStats  vmStats;

void *vmRegion;

int numPages = -1;

int numPagers = -1;
int pagerPids[MAXPAGERS];


static void FaultHandler(int type, void * offset);
static int Pager(char *buf);

static void vmInit(USLOSS_Sysargs *USLOSS_SysargsPtr);
static void vmDestroy(USLOSS_Sysargs *USLOSS_SysargsPtr);
/*
 *----------------------------------------------------------------------
 *
 * start4 --
 *
 * Initializes the VM system call handlers. 
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int start4(char *arg)
{
    int pid;
    int result;
    int status;


    /* user-process access to VM functions */
    systemCallVec[SYS_VMINIT]    = vmInit;
    systemCallVec[SYS_VMDESTROY] = vmDestroy; 
    
    result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, 2, &pid);
    if (result != 0) {
        USLOSS_Console("start4(): Error spawning start5\n");
        Terminate(1);
    }
    
    result = Wait(&pid, &status);
    if (result != 0) {
        USLOSS_Console("start4(): Error waiting for start5\n");
        Terminate(1);
    }
    
    Terminate(0);
    return 0; // not reached

} /* start4 */

/*
 *----------------------------------------------------------------------
 *
 * VmInit --
 *
 * Stub for the VmInit system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is initialized.
 *
 *----------------------------------------------------------------------
 */
static void vmInit(USLOSS_Sysargs *USLOSS_SysargsPtr)
{
    int dummy = -1;

    CheckMode();

    int mappings = (int)(long) USLOSS_SysargsPtr->arg1;
    int pages = (int)(long) USLOSS_SysargsPtr->arg2;
    int frames = (int)(long) USLOSS_SysargsPtr->arg3;
    int pagers = (int)(long) USLOSS_SysargsPtr->arg4;


    if (mappings != pages || pagers > MAXPAGERS) {
        USLOSS_SysargsPtr->arg4 = (void*)(long) -1;
    }
    else if (USLOSS_MmuRegion(&dummy) != NULL) {
        USLOSS_SysargsPtr->arg4 = (void*)(long) -2;
    }
    else USLOSS_SysargsPtr->arg4 = 0;


    USLOSS_SysargsPtr->arg1 = vmInitReal(mappings, pages, frames, pagers);

    numPages = pages;

    return;
} /* vmInit */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroy --
 *
 * Stub for the VmDestroy system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void vmDestroy(USLOSS_Sysargs *USLOSS_SysargsPtr)
{
   CheckMode();

   vmDestroyReal();
} /* vmDestroy */


/*
 *----------------------------------------------------------------------
 *
 * vmInitReal --
 *
 * Called by vmInit.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables.
 *
 * Results:
 *      Address of the VM region.
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
void* vmInitReal(int mappings, int pages, int frames, int pagers)
{
   int status;
   int dummy;
   int i;

   CheckMode();
   status = USLOSS_MmuInit(mappings, pages, frames, USLOSS_MMU_MODE_TLB);
   if (status != USLOSS_MMU_OK) {
      USLOSS_Console("vmInitReal: couldn't initialize MMU, status %d\n", status);
      abort();
   }
   USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;

   /*
    * Initialize page tables.
    */
    for (i = 0; i < MAXPROC; i++) {
        processes[i].numPages = 0;
        processes[i].pageTable = NULL;
    }

    frameTable = malloc(sizeof(Frame) * frames);

    for (i = 0; i < frames; i++) {
        frameTable[i].used = 0;
        frameTable[i].ownerPid = -1;
        frameTable[i].pageNumber = -1;
    }

   /* 
    * Create the fault mailbox.
    */
    faultMBoxID = MboxCreate(1, 0);

   /*
    * Fork the pagers.
    */
    numPagers = pagers;
    for (i = 0; i < pagers; i++) {
        pagerPids[i] = fork1("pager", Pager, NULL, 4 * USLOSS_MIN_STACK, PAGER_PRIORITY);
    }

    /*
    * Zero out, then initialize, the vmStats structure
    */
    memset((char *) &vmStats, 0, sizeof(VmStats));

    int bytesPerSector, sectorsPerTrack, tracksPerDisk;
    diskSizeReal(1, &bytesPerSector, &sectorsPerTrack, &tracksPerDisk);
    int bytesPerPage = USLOSS_MmuPageSize();
    int pagesPerDisk = bytesPerSector * sectorsPerTrack * 
                    tracksPerDisk / bytesPerPage;

    vmStats.pages = pages;
    vmStats.frames = frames;
    vmStats.diskBlocks = pagesPerDisk;
    vmStats.freeFrames = vmStats.frames;
    vmStats.freeDiskBlocks = vmStats.diskBlocks;
    vmStats.switches = 0;
    vmStats.faults = 0;
    vmStats.new = 0;
    vmStats.pageIns = 0;
    vmStats.pageOuts = 0;
    vmStats.replaced = 0;


    

    return USLOSS_MmuRegion(&dummy);
} /* vmInitReal */





/*
 *----------------------------------------------------------------------
 *
 * vmDestroyReal --
 *
 * Called by vmDestroy.
 * Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
void vmDestroyReal(void)
{

    CheckMode();
   
    if (numPages == -1) return;

    int i = USLOSS_MmuDone();
    i++;
    
    /*
    * Kill the pagers here.
    */
    for (i = 0; i < numPagers; i++) {
        MboxSend(faultMBoxID, NULL, 0);
        zap(pagerPids[i]);
    }


    /* 
    * Print vm statistics.
    */
    PrintStats();

    /* and so on... */
    for(i = 0; i < MAXPROC; i++) {
        free(processes[i].pageTable);
    }

    free(frameTable);

} /* vmDestroyReal */





/*
 *----------------------------------------------------------------------
 *
 * PrintStats --
 *
 *      Print out VM statistics.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Stuff is printed to the USLOSS_Console.
 *
 *----------------------------------------------------------------------
 */
void PrintStats(void)
{
     USLOSS_Console("VmStats\n");
     USLOSS_Console("pages:          %d\n", vmStats.pages);
     USLOSS_Console("frames:         %d\n", vmStats.frames);
     USLOSS_Console("diskBlocks:     %d\n", vmStats.diskBlocks);
     USLOSS_Console("freeFrames:     %d\n", vmStats.freeFrames);
     USLOSS_Console("freeDiskBlocks: %d\n", vmStats.freeDiskBlocks);
     USLOSS_Console("switches:       %d\n", vmStats.switches);
     USLOSS_Console("faults:         %d\n", vmStats.faults);
     USLOSS_Console("new:            %d\n", vmStats.new);
     USLOSS_Console("pageIns:        %d\n", vmStats.pageIns);
     USLOSS_Console("pageOuts:       %d\n", vmStats.pageOuts);
     USLOSS_Console("replaced:       %d\n", vmStats.replaced);
} /* PrintStats */





/*
 *----------------------------------------------------------------------
 *
 * FaultHandler
 *
 * Handles an MMU interrupt. Simply stores information about the
 * fault in a queue, wakes a waiting pager, and blocks until
 * the fault has been handled.
 *
 * Results:
 * None.
 *
 * Side effects:
 * The current process is blocked until the fault is handled.
 *
 *----------------------------------------------------------------------
 */
static void FaultHandler(int type /* MMU_INT */,
             void* arg  /* Offset within VM region */)
{
   int cause;

   assert(type == USLOSS_MMU_INT);
   cause = USLOSS_MmuGetCause();
   assert(cause == USLOSS_MMU_FAULT);
   vmStats.faults++;
   /*
    * Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the
    * reply.
    */
    faults[nextFaultMsgLocation % MAXPROC].pid = getpid();
    faults[nextFaultMsgLocation % MAXPROC].addr = arg;
    faults[nextFaultMsgLocation % MAXPROC].replyMbox = MboxCreate(1,0);
    nextFaultMsgLocation++;


    MboxSend(faultMBoxID, NULL, 0);
    MboxReceive(faults[nextFaultMsgLocation-1 % MAXPROC].replyMbox, NULL, 0);

    MboxRelease(faults[nextFaultMsgLocation-1 % MAXPROC].replyMbox);

} /* FaultHandler */


/*
 *----------------------------------------------------------------------
 *
 * Pager 
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
static int Pager(char *buf)
{
    FaultMsg* currentFault = NULL;
    int dummy = -1;
    int tag = -1;
    int firstTime = 0;

    while(!isZapped()) {
        /* Wait for fault to occur (receive from mailbox) */
        MboxReceive(faultMBoxID, NULL, 0);

        if (isZapped()) break;

        currentFault = &faults[nextFaultMsgLocation-1 % MAXPROC];

        if (processes[currentFault->pid % MAXPROC].pageTable[0].state == UNUSED) {
            firstTime = 1;
            processes[currentFault->pid % MAXPROC].pageTable[0].state = IN_PAGE_TABLE;
            vmStats.new++;
        }

        /* Look for free frame */
        int freeFrameNumber = 0; // cheating
        
        /* If there isn't one then use clock algorithm to
         * replace a page (perhaps write to disk) */

        /* Load page into frame from disk, if necessary */

        processes[currentFault->pid % MAXPROC].pageTable[0].frame = freeFrameNumber; // cheating
        
        // initializing frame
        if (firstTime) {
            // zero-ing out frame
            dummy = USLOSS_MmuGetTag(&tag);
            dummy = USLOSS_MmuMap(tag, (int)(long) currentFault->addr, 
                freeFrameNumber, USLOSS_MMU_PROT_RW);
            dummy++;
            memset((char *) USLOSS_MmuRegion(&dummy), 0, USLOSS_MmuPageSize());
            // dummy = USLOSS_MmuUnmap(tag, (int)(long) currentFault->addr);
        }
        /* Unblock waiting (faulting) process */
        MboxSend(currentFault->replyMbox, NULL, 0);
    }
    return 0;
} /* Pager */

