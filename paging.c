#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "paging.h"

void enqueue(struct swapRequests *q)
{
    if ((q->tail + 1) % (NPROC + 1) == q->head)
        return;
    q->queue[q->tail] = myproc();
    q->tail = (q->tail + 1) % (NPROC + 1);
}
struct proc *dequeue(struct swapRequests *q)
{
    if (q->head == q->tail)
        return 0;
    struct proc *headProc = q->queue[q->head];
    q->head = (q->head + 1) % (NPROC + 1);
    return headProc;
}

void swapOut()
{
    cprintf("Running swapOut\n");
    acquire(&ptable.lock);
    for (int i = 0; i < NPROC; i++)
    {
        if (ptable.proc[i].state == RUNNING)
        {
            cprintf("%s \n", ptable.proc[i].name);
        }
    }
    release(&ptable.lock);
    exit();
}

void swapIn()
{
    cprintf("Running swapIn\n");
    exit();
}