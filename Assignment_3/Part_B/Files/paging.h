struct swapRequests
{
    struct spinlock lock;
    struct proc *queue[NPROC + 1];
    int head;
    int tail;
    char *swapChannel;
    char *requestChannel;
};

#ifndef PAGING
extern struct
{
    struct spinlock lock;
    struct proc proc[NPROC];
} ptable;
#endif

extern struct swapRequests swapInQueue, swapOutQueue;

void enqueue(struct swapRequests *);
struct proc *dequeue(struct swapRequests *);
int isEmpty(struct swapRequests *);
void swapOut();
void swapIn();
void requestSwapOut();
void requestSwapIn();
void getSwapFileName(int, uint, char *);
extern int mapSwapIn(pde_t *, void *, uint, uint);
int evictVictimPage(int);
void deleteSwapPages();
int fwrite(int, uint, char *);
int fread(int, uint, char *);
