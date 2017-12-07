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
int currentFaultMsgLocation = 0;
int nextFaultMsgLocation = 0;
int faultMBoxID;

VmStats  vmStats;

void *vmRegion;

int numPages = -1;

int numFrames = -1;

int numPagers = -1;
int pagerPids[MAXPAGERS];
int maxPagerPid = -1;

int clockHand = 0;
int nextEmptyDiskBlock = 0;

int framesListMbox;
int frameLockMbox;


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
    numFrames = frames;

    for (i = 0; i < frames; i++) {
        frameTable[i].used = NOT_USED;
        frameTable[i].ownerPid = NO_OWNER;
        frameTable[i].pageNumber = -1;
        frameTable[i].dirty = CLEAN_BLANK;
        frameTable[i].lock = UNLOCKED;
    }

    framesListMbox = MboxCreate(1, 0);
    frameLockMbox = MboxCreate(1, 0);

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
    maxPagerPid = pagerPids[pagers-1];

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
    int myFaultMsgLocation = nextFaultMsgLocation;
    nextFaultMsgLocation++;


    // tell pager there is a fault
    MboxSend(faultMBoxID, NULL, 0);

    // wait for pager's response
    MboxReceive(faults[myFaultMsgLocation % MAXPROC].replyMbox, NULL, 0);

    // release mailbox that process was waiting on
    MboxRelease(faults[myFaultMsgLocation % MAXPROC].replyMbox);

    int pageNumber = (int)(long) arg / USLOSS_MmuPageSize();
    int frameNumber = processes[getpid() % MAXPROC].pageTable[pageNumber].frame;

    // acquire frames list mutex to change lock status
    MboxSend(framesListMbox, NULL, 0);
    frameTable[frameNumber].lock = UNLOCKED;

    // alert any process waiting for a frame to unlock
    MboxCondSend(frameLockMbox, NULL, 0);

    // release frames list mutex now that lock status has
    // been updated
    MboxReceive(framesListMbox, NULL, 0);
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
    int access = 0;
    int track, first;

    dummy = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);

    while(!isZapped()) {
        /* Wait for fault to occur (receive from mailbox) */
        MboxReceive(faultMBoxID, NULL, 0);
        if (isZapped()) break;

        // get fault info
        currentFault = &faults[currentFaultMsgLocation % MAXPROC];
        currentFaultMsgLocation++;
        int pageNumber = (int)(long) (currentFault->addr)/USLOSS_MmuPageSize();

        if (processes[currentFault->pid % MAXPROC].pageTable[pageNumber].state == UNUSED) {
            vmStats.new++;
        }



        /* Look for free frame. If there isn't one then use clock 
         * algorithm to replace a page (perhaps write to disk) */
        MboxSend(framesListMbox, NULL, 0);
        int freeFrameNumber = findFrameNumber(); 
        

        int oldOwner = frameTable[freeFrameNumber].ownerPid;
        int oldPageNumber = frameTable[freeFrameNumber].pageNumber;
        // USLOSS_Console("oldOwner: %d, oldPage: %d, freeFrame: %d\n", oldOwner, oldPageNumber, freeFrameNumber);

        dummy = USLOSS_MmuGetAccess(freeFrameNumber, &access);

        // another page might think this frame belongs to it... undo this
        if (frameTable[freeFrameNumber].ownerPid != NO_OWNER) {
            if (access & USLOSS_MMU_DIRTY) {

                void* memRegion = USLOSS_MmuRegion(&dummy);
                void* buffer = malloc(USLOSS_MmuPageSize());

                dummy = USLOSS_MmuMap(TAG, oldPageNumber, freeFrameNumber, 
                    USLOSS_MMU_PROT_RW);
                memcpy(buffer, memRegion + oldPageNumber * USLOSS_MmuPageSize(),
                    USLOSS_MmuPageSize());


                // write to disk
                if (processes[oldOwner % MAXPROC].pageTable[oldPageNumber].diskBlock == NO_DISK_BLOCK) {
                    track = nextEmptyDiskBlock / 2;
                    first = (nextEmptyDiskBlock % 2) * 8;
                    processes[oldOwner % MAXPROC].pageTable[oldPageNumber].diskBlock = nextEmptyDiskBlock++;
                }
                else {
                    track = processes[oldOwner % MAXPROC].pageTable[oldPageNumber].diskBlock / 2;
                    first = (processes[oldOwner % MAXPROC].pageTable[oldPageNumber].diskBlock % 2) * 8;
                }

                MboxReceive(framesListMbox, NULL, 0);
                diskWriteReal(SWAP_DISK, track, first, NUM_SECTORS, buffer);
                MboxSend(framesListMbox, NULL, 0);

                free(buffer);
                dummy = USLOSS_MmuUnmap(TAG, oldPageNumber);

                vmStats.pageOuts++;

                // old process' old page is now on disk
                processes[oldOwner % MAXPROC].pageTable[oldPageNumber].state = ON_DISK;
            }
            else {
                if (frameTable[freeFrameNumber].dirty == CLEAN_BLANK) {
                    processes[oldOwner % MAXPROC].pageTable[oldPageNumber].state = BLANK;
                }
                else { // CLEAN_ON_DISK
                    processes[oldOwner % MAXPROC].pageTable[oldPageNumber].state = ON_DISK;
                }
            }
            
            processes[oldOwner % MAXPROC].pageTable[oldPageNumber].frame = NO_FRAME;
        }

        

        /* Load page into frame from disk, if necessary */

        // initializing frame by setting all bytes to 0
        if (processes[currentFault->pid % MAXPROC].pageTable[pageNumber].state == UNUSED ||
            processes[currentFault->pid % MAXPROC].pageTable[pageNumber].state == BLANK) {
            
            frameTable[freeFrameNumber].dirty = CLEAN_BLANK;
            
            // zero-ing out frame
            dummy = USLOSS_MmuMap(TAG, pageNumber, freeFrameNumber, 
                USLOSS_MMU_PROT_RW);
            
            memset((char *) (USLOSS_MmuRegion(&dummy) + (int)(long)currentFault->addr), 
                0, USLOSS_MmuPageSize());

            // set frame to clean
            dummy = USLOSS_MmuSetAccess(freeFrameNumber, access & ~(1 << USLOSS_MMU_DIRTY));

            dummy = USLOSS_MmuUnmap(TAG, pageNumber);
        }

        // initializing frame by reading in from disk
        else if (processes[currentFault->pid % MAXPROC].pageTable[pageNumber].state == 
            ON_DISK) {

            vmStats.pageIns++;
            frameTable[freeFrameNumber].dirty = CLEAN_ON_DISK;

            // read from disk
            dummy = USLOSS_MmuMap(TAG, pageNumber, freeFrameNumber, 
                    USLOSS_MMU_PROT_RW);

            void* memRegion = USLOSS_MmuRegion(&dummy);
            void* buffer = malloc(USLOSS_MmuPageSize());
            
            track = processes[currentFault->pid % MAXPROC].pageTable[pageNumber].diskBlock / 2;
            first = (processes[currentFault->pid % MAXPROC].pageTable[pageNumber].diskBlock % 2) * 8;

            MboxReceive(framesListMbox, NULL, 0);
            diskReadReal(SWAP_DISK, track, first, NUM_SECTORS, buffer);
            MboxSend(framesListMbox, NULL, 0);

            // and assign content to frame
            memcpy(memRegion + pageNumber * USLOSS_MmuPageSize(), buffer, USLOSS_MmuPageSize());
            
            free(buffer);
            dummy = USLOSS_MmuUnmap(TAG, oldPageNumber);
        }
        


        // current page gets new frame
        processes[currentFault->pid % MAXPROC].pageTable[pageNumber].frame = freeFrameNumber;
        processes[currentFault->pid % MAXPROC].pageTable[pageNumber].state = IN_PAGE_TABLE;
        
        
        

        // update frame table
        frameTable[freeFrameNumber].used = USED;
        frameTable[freeFrameNumber].ownerPid = currentFault->pid;
        frameTable[freeFrameNumber].pageNumber = pageNumber;

        MboxReceive(framesListMbox, NULL, 0);

        /* Unblock waiting (faulting) process */
        MboxSend(currentFault->replyMbox, NULL, 0);
    }

    dummy++;
    return 0;
} /* Pager */




int findFrameNumber() {
    int i, dummy;

    while (1) {
        // only looks for unused frames
        for (i = 0; i < numFrames; i++) {
            if (frameTable[i].used == NOT_USED) {
                frameTable[i % numFrames].lock = LOCKED;
                return i;
            }
        }

        // looks for frame that hasn't been used recently
        for (i = 0; i < numFrames+1; i++) {
            int access = 0;
            dummy = USLOSS_MmuGetAccess(i % numFrames, &access);
            
            if (~(access & USLOSS_MMU_REF) && frameTable[clockHand % numFrames].lock == UNLOCKED) {
                frameTable[clockHand % numFrames].lock = LOCKED;
                return clockHand++ % numFrames;
            }

            dummy = USLOSS_MmuSetAccess(i, access | USLOSS_MMU_REF);
            dummy++;

            clockHand++;
        }

        // release framesList mutex before blocking on frameLock mutex
        MboxReceive(framesListMbox, NULL, 0);
        MboxReceive(frameLockMbox, NULL, 0);

        // reacquire framesList mutex
        MboxSend(framesListMbox, NULL, 0);
    }

    return -1;
}

