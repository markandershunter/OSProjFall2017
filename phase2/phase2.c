/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

   ------------------------------------------------------------------------ */

#include <stdio.h>
#include <string.h>
#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <stdint.h>

#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);


/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;
int nextMid = 7;
int clockCounter = 0;

// the mail boxes
mailbox MailBoxTable[MAXMBOX];

// also need array of mail slots, array of function ptrs to system call
// handlers, ...
mailSlot MailSlotTable[MAXSLOTS];
int nextSlot = 0;
int numSlotsUsed = 0;

phase2Proc processTable[MAXPROC];
int nextProcSlot = 0;



/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
    int kid_pid;
    int status;

    arg = NULL;

    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): at beginning\n");

    // check_kernel_mode("start1");

    // Disable interrupts
    // disableInterrupts();

    // Initialize the mail box table, slots, & other data structures.
    // Initialize USLOSS_IntVec and system call handlers,
    // allocate mailboxes for interrupt handlers.  Etc...
    init();

    // enableInterrupts

    // Create a process for start2, then block on a join until start2 quits
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): forking start2 process\n");
    kid_pid = fork1("start2", start2, arg, 4 * USLOSS_MIN_STACK, 1);
    if ( join(&status) != kid_pid ) {
        USLOSS_Console("start2(): join returned something other than ");
        USLOSS_Console("start2's pid\n");
    }

    return 0;
} /* start1 */


/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array.
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
    int i;

    if(isKernel()){
        USLOSS_Console("MboxCreate(): called while in user mode. Halting...\n");
        USLOSS_Halt(1);
    }

    if (slots < 0 || slot_size < 0 || slot_size > MAX_MESSAGE) {
        return INVALID_PARAMETER;
    }

    for(i = 0; i < MAXMBOX; i++){
        if(MailBoxTable[nextMid % MAXMBOX].status == UNUSED){

            MailBoxTable[nextMid % MAXMBOX].mboxID = nextMid;
            MailBoxTable[nextMid % MAXMBOX].status = CREATED;

            MailBoxTable[nextMid % MAXMBOX].numSlots = slots;
            MailBoxTable[nextMid % MAXMBOX].numSlotsUsed = 0;
            MailBoxTable[nextMid % MAXMBOX].slotSize = slot_size;

            MailBoxTable[nextMid % MAXMBOX].headSlot = NULL;

            return MailBoxTable[nextMid++ % MAXMBOX].mboxID;
        }
        else nextMid++;
    }

    return -1;
} /* MboxCreate */






int MboxRelease(int mbox_id) {
    phase2Proc* proc = NULL;

    if (mbox_id < 0 || mbox_id >= MAXMBOX) return INVALID_PARAMETER;

    MailBoxTable[mbox_id].status = UNUSED;

    proc = MailBoxTable[mbox_id].waitingToReceive;
    while (proc != NULL) {
        proc->status = UNUSED;
        unblockProc(proc->pid);
        proc = proc->nextProc;
    }

    proc = MailBoxTable[mbox_id].waitingToSend;
    while (proc != NULL) {
        proc->status = UNUSED;
        unblockProc(proc->pid);
        proc = proc->nextProc;
    }

    return 0;
}





/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
    mailbox* box = NULL;
    phase2Proc* nextInLine = NULL;
    int waitSlot = -1;
    int nextSlotID = -1;
    int i = -1;

    // check for kernel mode
    if(isKernel()){
        USLOSS_Console("MboxCreate(): called while in user mode. Halting...\n");
        USLOSS_Halt(1);
    }


    if (mbox_id < 0 || mbox_id >= MAXMBOX || msg_size < 0 || msg_size > MailBoxTable[mbox_id].slotSize) return INVALID_PARAMETER;

    // check to make sure there is room in slots table
    if (numSlotsUsed == MAXSLOTS) {
        USLOSS_Console("MboxSend(): all slots used. Halting...\n");
        USLOSS_Halt(1);
    }

    box = &MailBoxTable[mbox_id];

    // Check if the mailbox is being used
    if (box->status == CREATED) {

        //zero slot mailbox behaves completely differently
        if (box->numSlots == 0) {
            // first process to reach mailbox
            if (box->waitingToReceive == NULL) {
                waitSlot = getNextProcSlot();

                processTable[waitSlot].pid = getpid();
                processTable[waitSlot].status = BLOCKED;
                addToWaitingListSend(box, &processTable[waitSlot]);

                memcpy(box->zeroSlotSlot, msg_ptr, msg_size);
                box->zeroSlotSize = msg_size;

                blockMe(10 + getpid());

                // just woke up, make sure mailbox was not released
                if (box->status == UNUSED) return MAILBOX_RELEASED;

                return 0; // rendezvous complete
            }

            // different process already waiting to receive
            else {
                i = box->waitingToReceive->pid;             // save pid of process that was blocked

                box->waitingToReceive->status = UNUSED;     // set it's status back to unused

                nextInLine = box->waitingToReceive->nextProc;   // get the next process that is waiting
                box->waitingToReceive->nextProc = NULL;         // remove this process from the chain

                box->waitingToReceive = nextInLine;             // second in line becomes first in line

                memcpy(box->zeroSlotSlot, msg_ptr, msg_size);
                box->zeroSlotSize = msg_size;

                unblockProc(i);     // unblock process that has been waiting

                return 0;           // rendezvous complete
            }
        }


        // if the mailbox has already put in as many messages as it has slots,
        // block the process until a slot opens up
        if (box->numSlotsUsed == box->numSlots) {
            waitSlot = getNextProcSlot();

            processTable[waitSlot].pid = getpid();
            processTable[waitSlot].status = BLOCKED;
            addToWaitingListSend(box, &processTable[waitSlot]);

            blockMe(10 + getpid());
            // just woke up, make sure mailbox was not released
            if (box->status == UNUSED) return MAILBOX_RELEASED;
        }


        // slots table has room, as does this mailbox

        nextSlotID = getNextSlotID();

        MailSlotTable[nextSlotID].mboxID = mbox_id;
        MailSlotTable[nextSlotID].status = CREATED;
        MailSlotTable[nextSlotID].nextSlot = NULL;
        MailSlotTable[nextSlotID].slotSize = msg_size;

        memcpy(&MailSlotTable[nextSlotID].message, msg_ptr, msg_size);

        //point to slot from mailbox
        appendSlotToMailbox(box, nextSlotID);

        // there is a process blocked on a receive
        if (box->waitingToReceive != NULL) {
            i = box->waitingToReceive->pid;             // save pid of process that was blocked

            box->waitingToReceive->status = UNUSED;     // set it's status back to unused

            nextInLine = box->waitingToReceive->nextProc;   // get the next process that is waiting
            box->waitingToReceive->nextProc = NULL;         // remove this process from the chain

            box->waitingToReceive = nextInLine;             // second in line becomes first in line

            unblockProc(i);     // unblock process that has been waiting
        }

        box->numSlotsUsed++;
        numSlotsUsed++;

        return 0;
    }
    else return INVALID_PARAMETER;

} /* MboxSend */


/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
    mailbox* box = NULL;
    slotPtr oldSlot = NULL;
    phase2Proc* nextInLine = NULL;
    int size = -1;
    int waitSlot = -1;
    int i = -1;

    // check for kernel mode
    if(isKernel()){
        USLOSS_Console("MboxCreate(): called while in user mode. Halting...\n");
        USLOSS_Halt(1);
    }

    // invalid mailbox or invalid message size
    if (mbox_id < 0 || mbox_id >= MAXMBOX || msg_size < 0 || msg_size > MAX_MESSAGE) return INVALID_PARAMETER;

    box = &MailBoxTable[mbox_id];


    if (box->status == CREATED) {

        //zero slot mailbox behaves completely differently
        if (box->numSlots == 0) {
            // first process to reach mailbox
            if (box->waitingToSend == NULL) {
                waitSlot = getNextProcSlot();

                processTable[waitSlot].pid = getpid();
                processTable[waitSlot].status = BLOCKED;
                addToWaitingListReceive(box, &processTable[waitSlot]);

                blockMe(10 + getpid());

                // just woke up, make sure mailbox was not released
                if (box->status == UNUSED) return MAILBOX_RELEASED;

                memcpy(msg_ptr, box->zeroSlotSlot, msg_size);
                msg_size = box->zeroSlotSize;

                return msg_size; // rendezvous complete
            }

            // different process already waiting to send
            else {
                i = box->waitingToSend->pid;            // save pid of process that was blocked

                box->waitingToSend->status = UNUSED;    // set it's status back to unused

                nextInLine = box->waitingToSend->nextProc;  // get the next process that is waiting
                box->waitingToSend->nextProc = NULL;        // remove this process from the chain

                box->waitingToSend = nextInLine;            // second in line becomes first in line

                memcpy(msg_ptr, box->zeroSlotSlot, msg_size);
                msg_size = box->zeroSlotSize;

                unblockProc(i);     // unblock process that has been waiting

                return msg_size;           // rendezvous complete
            }
        }


        // no messages yet, so block
        if (box->headSlot == NULL) {
            waitSlot = getNextProcSlot();

            processTable[waitSlot].pid = getpid();
            processTable[waitSlot].status = BLOCKED;
            addToWaitingListReceive(box, &processTable[waitSlot]);

            blockMe(10 + getpid());

            // just woke up, make sure mailbox was not released
            if (box->status == UNUSED) return MAILBOX_RELEASED;
        }


        // message is too big for buffer
        if (box->headSlot->slotSize > msg_size) return BUFFER_TOO_SMALL;


        memcpy(msg_ptr, box->headSlot->message, msg_size);
        size = box->headSlot->slotSize;

        oldSlot = box->headSlot;
        box->headSlot = box->headSlot->nextSlot;

        cleanUpSlot(oldSlot);


        // there is a process blocked on a receive
        if (box->waitingToSend != NULL) {
            i = box->waitingToSend->pid;                    // save pid of process that was blocked

            box->waitingToSend->status = UNUSED;            // set it's status back to unused

            nextInLine = box->waitingToSend->nextProc;      // get the next process that is waiting
            box->waitingToSend->nextProc = NULL;            // remove this process from the chain

            box->waitingToSend = nextInLine;                // second in line becomes first in line

            unblockProc(i);     // unblock process that has been waiting
        }


        box->numSlotsUsed--;
        numSlotsUsed--;

        return size;
    }
    else return INVALID_PARAMETER;

} /* MboxReceive */





int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size) {
    // check for kernel mode
    if(isKernel()){
        USLOSS_Console("MboxCreate(): called while in user mode. Halting...\n");
        USLOSS_Halt(1);
    }

    if (mbox_id < 0 || mbox_id >= MAXMBOX || msg_size < 0 || msg_size > MailBoxTable[mbox_id].slotSize) return INVALID_PARAMETER;

    // check to make sure mailbox has not been released
    if (MailBoxTable[mbox_id].status == UNUSED) return MAILBOX_DNE;

    // check to make sure there is room in slots table
    if (numSlotsUsed == MAXSLOTS) return SYSTEM_FULL;

    // check to make sure there is room in the mailbox
    if (MailBoxTable[mbox_id].numSlotsUsed == MailBoxTable[mbox_id].numSlots) return MAILBOX_FULL;


    return MboxSend(mbox_id, msg_ptr, msg_size);
}

/*
*
*/
int MboxCondReceive(int mbox_id, void *msg_ptr, int msg_size){
    mailbox* box = NULL;

    // check for kernel mode
    if(isKernel()){
        USLOSS_Console("MboxCreate(): called while in user mode. Halting...\n");
        USLOSS_Halt(1);
    }

    // check to make sure mailbox has not been released
    if (MailBoxTable[mbox_id].status == UNUSED) return MAILBOX_DNE;

    // check for valid parameters
    if (mbox_id < 0 || mbox_id >= MAXMBOX || msg_size < 0 || msg_size > MAX_MESSAGE){
        if (DEBUG2 && debugflag2)
            USLOSS_Console("MboxCondReceive(): Invalid parameters");
        return INVALID_PARAMETER;
    }

    box = &MailBoxTable[mbox_id];

    if(box->headSlot == NULL){
        return MAILBOX_EMPTY;
    }

    return MboxReceive(mbox_id, msg_ptr, msg_size);
}


int waitDevice(int type, int unit, int *status){
    if(type == USLOSS_CLOCK_DEV){
        MboxReceive(0, status, 100);
    }else if(type == USLOSS_DISK_DEV){
        MboxReceive(1 + unit, status, 100);
    }else if(type == USLOSS_TERM_DEV){
        MboxReceive(3 + unit, status, 100);
    }

    if(isZapped()) return -1;
    return 0;
}


/*
*   Initializes mailbox table
*/
void init(){
    int i;

    for(i = 0; i < 7; i++){
        MailBoxTable[i].mboxID = i;

        MailBoxTable[i].numSlots = 0;
        MailBoxTable[i].numSlotsUsed = 0;
        MailBoxTable[i].slotSize = 100;
        MailBoxTable[i].status = CREATED;

        MailBoxTable[i].headSlot = NULL;
    }

    for(i = 7; i < MAXMBOX; i++){
        MailBoxTable[i].status = UNUSED;
    }

    for (i = 0; i < MAXSLOTS; i++) {
        MailSlotTable[i].status = UNUSED;
    }

    for (i = 0; i < MAXPROC; i++) {
        processTable[i].status = UNUSED;
        processTable[i].nextProc = NULL;
    }
}

// test if in kernel mode
int isKernel(){
    if (!(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)) {
        return -1;
    }

    return 0;
}




int getNextSlotID() {
    int i = 0;

    for (i = 0; i < MAXSLOTS; i++) {
        if (MailSlotTable[nextSlot % MAXSLOTS].status == UNUSED) return nextSlot;
        else nextSlot++;
    }

    return -1;
}



int getNextProcSlot() {
    int i = 0;

    for (i = 0; i < MAXPROC; i++) {
        if (processTable[nextProcSlot % MAXPROC].status == UNUSED) {
            return nextProcSlot++;
        }
        else nextProcSlot++;
    }

    return -1;
}



void appendSlotToMailbox(mailbox* box, int nextSlotID) {
    slotPtr ptr = NULL;

    if (box == NULL) return;

    if (box->headSlot == NULL) {
        box->headSlot = &MailSlotTable[nextSlotID];
        return;
    }

    ptr = box->headSlot;

    while (ptr->nextSlot != NULL) {
        ptr = ptr->nextSlot;
    }

    ptr->nextSlot = &MailSlotTable[nextSlotID];
}




void cleanUpSlot(slotPtr oldSlot) {
    oldSlot->mboxID = UNUSED;
    oldSlot->status = UNUSED;
    oldSlot->nextSlot = NULL;
    oldSlot->slotSize = 0;
}



void addToWaitingListReceive(mailbox* box, phase2Proc* proc) {
    phase2Proc* ptr = NULL;

    if (box == NULL || proc == NULL) return;

    if (box->waitingToReceive == NULL) {
        box->waitingToReceive = proc;
        return;
    }


    ptr = box->waitingToReceive;

    while (ptr->nextProc != NULL) {
        ptr = ptr->nextProc;
    }

    ptr->nextProc = proc;
}



void addToWaitingListSend(mailbox* box, phase2Proc* proc) {
    phase2Proc* ptr = NULL;

    if (box == NULL || proc == NULL) return;

    if (box->waitingToSend == NULL) {
        box->waitingToSend = proc;
        return;
    }


    ptr = box->waitingToSend;

    while (ptr->nextProc != NULL) {
        ptr = ptr->nextProc;
    }

    ptr->nextProc = proc;
}

void clockHandler (int interruptType, void* arg) {
    void *status = NULL;
    int result = 0;
    if(interruptType == USLOSS_CLOCK_DEV){
        timeSlice();
        result = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, status);
        result++;
        if(clockCounter == 5){
            MboxCondSend(0, status, 100);
            clockCounter = 0;
        }else clockCounter++;
    }
}

void diskHandler (int interruptType, void* arg) {
    void *status = NULL;
    int result = 0;
    if(interruptType == USLOSS_DISK_DEV){
        result = USLOSS_DeviceInput(USLOSS_DISK_DEV, (uintptr_t) arg, status);
        result++;
        MboxCondSend((uintptr_t) arg + 1, status, 100);
    }
}

void terminalHandler (int interruptType, void* arg) {
    USLOSS_Console("it's running\n");
    void *status = NULL;
    int result = 0;
    if(interruptType == USLOSS_TERM_DEV){
        result = USLOSS_DeviceInput(USLOSS_TERM_DEV, (uintptr_t) arg, status);
        result++;
        MboxCondSend((uintptr_t) arg + 3, status, 100);
    }
}

void alarmHandler (int interruptType, void* arg) {}
void mmuHandler (int interruptType, void* arg) {}
void syscallHandler (int interruptType, void* arg) {}
void illegalHandler (int interruptType, void* arg) {}
