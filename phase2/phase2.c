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

#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);


/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;
int nextMid = 7;

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
        USLOSS_Console("start1(): fork'ing start2 process\n");
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
    int nextSlotID = -1;

    // check for kernel mode
    if(isKernel()){
        USLOSS_Console("MboxCreate(): called while in user mode. Halting...\n");
        USLOSS_Halt(1);
    }

    if (mbox_id < 0 || mbox_id >= MAXMBOX || msg_size < 0 || msg_size > MAX_MESSAGE) return INVALID_PARAMETER;

    // check to make sure there is room in slots table
    if (numSlotsUsed == MAXSLOTS) {
        USLOSS_Console("MboxSend(): all slots used. Halting...\n");
        USLOSS_Halt(1);
    }

    box = &MailBoxTable[mbox_id];

    if (box->numSlotsUsed == box->numSlots) return MAILBOX_FULL;

    // slots table has room, as does this mailbox
    if (box->status == CREATED) {
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
            unblockProc(box->waitingToReceive->pid);    // unblock process that has been waiting
            box->waitingToReceive->status = UNUSED;     // set it's status back to unused
            
            nextInLine = box->waitingToReceive->nextProc;   // get the next process that is waiting
            box->waitingToReceive->nextProc = NULL;         // remove this process from the chain
            
            box->waitingToReceive = nextInLine;             // second in line becomes first in line
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
    int size = -1;
    int waitSlot = -1;

    // check for kernel mode
    if(isKernel()){
        USLOSS_Console("MboxCreate(): called while in user mode. Halting...\n");
        USLOSS_Halt(1);
    }
    
    // invalid mailbox or invalid message size
    if (mbox_id < 0 || mbox_id >= MAXMBOX || msg_size < 0 || msg_size > MAX_MESSAGE) return INVALID_PARAMETER;

    box = &MailBoxTable[mbox_id];
    

    // no messages yet, so block
    if (box->headSlot == NULL) {
        waitSlot = getNextProcSlot();
        
        processTable[waitSlot].pid = getpid();
        processTable[waitSlot].status = BLOCKED;
        addToWaitingList(box, &processTable[waitSlot]);

        blockMe(10 + getpid());
    }


    // message is too big for buffer
    if (box->headSlot->slotSize > msg_size) return BUFFER_TOO_SMALL;


    memcpy(msg_ptr, box->headSlot->message, msg_size);
    size = box->headSlot->slotSize;

    oldSlot = box->headSlot;
    box->headSlot = box->headSlot->nextSlot;

    cleanUpSlot(oldSlot);

    box->numSlotsUsed--;
    numSlotsUsed--;

    return size;

    // return 0;
} /* MboxReceive */


/*  TODO
*   Initializes mailbox table (more to come)
*/
void init(){
    int i;

    for(i = 0; i < 7; i++){
        MailBoxTable[i].status = CREATED;
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

int isKernel(){
    // test if in kernel mode (1); halt if in user mode (0)
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



void addToWaitingList(mailbox* box, phase2Proc* proc) {
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



