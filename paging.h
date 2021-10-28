struct swapRequests
{
    struct spinlock lock;
    struct proc *queue[NPROC + 1];
    int head;
    int tail;
};

#ifndef PAGING
extern struct
{
    struct spinlock lock;
    struct proc proc[NPROC];
} ptable;
#endif

struct swapRequests swapInQueue, swapOutQueue;

void enqueue(struct swapRequests *);
struct proc *dequeue(struct swapRequests *);
int isEmpty(struct swapRequests *);
void swapOut();
void swapIn();
void requestSwapOut();
void requestSwapIn();
pde_t *chooseVictim(struct proc **, uint *);
void getSwapFileName(int, uint, char *);
void writeSwapPage(uint, int, pte_t *);
void handlePageFault();

#define SWAP_OUT_CHAN 50
#define SWAP_IN_CHAN 115
