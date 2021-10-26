struct swapRequests
{
    struct spinlock lock;
    struct proc *queue[NPROC + 1];
    int head;
    int tail;
};

extern struct
{
    struct spinlock lock;
    struct proc proc[NPROC];
} ptable;

void enqueue(struct swapRequests *);
struct proc *dequeue(struct swapRequests *);
void swapOut();
void swapIn();
void requestSwapOut();
void requestSwapIn();