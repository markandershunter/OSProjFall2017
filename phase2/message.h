
#define DEBUG2 1

#define INVALID_PARAMETER   -1
#define BUFFER_TOO_SMALL    -1
#define MESSSAGE_TOO_BIG    -1
#define MAILBOX_DNE         -1
#define SYSTEM_FULL         -2
#define MAILBOX_FULL        -2
#define MAILBOX_RELEASED    -3
#define MAILBOX_EMPTY       -2


#define BLOCKED             1



typedef struct mailSlot     mailSlot;
typedef struct mailSlot     *slotPtr;
typedef struct mailbox      mailbox;
typedef struct mboxProc     *mboxProcPtr;
typedef struct phase2Proc   phase2Proc;


struct mailbox {
    int         mboxID;
    int         status;

    // other items as needed...
    int         numSlots;           // how many slots does the mailbox hold
    int         numSlotsUsed;
    int         slotSize;           // maximum size of a message in the mail slot
    slotPtr     headSlot;
    phase2Proc* waitingToReceive;   // first process in line that is blocked on a receive
    phase2Proc* waitingToSend;      // first process in line that is blocked on a send
    char        zeroSlotSlot[MAX_MESSAGE];
    int         zeroSlotSize;
};


struct mailSlot {
    int         mboxID;
    int         status;

    // other items as needed...
    slotPtr     nextSlot;
    char        message[MAX_MESSAGE];   // array to hold message
    int         slotSize;               // can be no bigger than MAX_MESSAGE
};



struct phase2Proc {
    int             status;
    int             pid;
    phase2Proc*     nextProc;           // the next process waiting on a Receive from the same mailbox
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


int getNextSlotID();
int getNextProcSlot();
void appendSlotToMailbox(mailbox* box, int nextSlotID);
void cleanUpSlot(slotPtr);
void addToWaitingListReceive(mailbox* box, phase2Proc* proc);
void addToWaitingListSend(mailbox* box, phase2Proc* proc);
