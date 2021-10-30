#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"
#include "stat.h"
#include "paging.h"

inline struct proc *getHead(struct swapRequests *q)
{
    return q->queue[q->head];
}

inline uint getRDBits(pde_t *pte)
{
    int temp;
    temp = (((3 << 5) & (*pte)) >> 5);
    if (temp == 1 || temp == 2)
    {
        return 3 - temp;
    }
    return temp;
}

static void
wakeup1(void *chan)
{
    struct proc *p;

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if (p->state == SLEEPING && p->chan == chan)
            p->state = RUNNABLE;
}

void enqueue(struct swapRequests *q)
{
    if ((q->tail + 1) % (NPROC + 1) == q->head)
        return;
    q->queue[q->tail] = myproc();
    q->tail = (q->tail + 1) % (NPROC + 1);
    myproc()->swapSatisfied = 0;
}
struct proc *dequeue(struct swapRequests *q)
{
    if (q->head == q->tail)
        return 0;
    struct proc *headProc = q->queue[q->head];
    q->head = (q->head + 1) % (NPROC + 1);
    return headProc;
}

int isEmpty(struct swapRequests *q)
{
    return q->head == q->tail;
}

int fread(int pid, uint addr, char *buf)
{
    char name[14];

    getSwapFileName(pid, addr, name);
    int fd = fopen(name, O_RDONLY); // Open swapout page file
    struct file *f;
    if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
        return -1;
    int noc = fileread(f, buf, 4096); // Read the page into the buffer
    if (noc < 0)
    {
        cprintf("Unable to write. Exiting (paging.c::fread)!!");
    }
    fdelete(name);
    fclose(fd);

    return noc;
}

void swapIn()
{
    sleep(&swapInQueue, &ptable.lock);
    cprintf("Start swap_in process\n");
    for (;;)
    {
        acquire(&swapInQueue.lock);
        while (isEmpty(&swapInQueue) == 0)
        {
            // Dequeue to process request
            struct proc *requester = getHead(&swapInQueue);
            dequeue(&swapInQueue);
            release(&swapInQueue.lock);
            release(&ptable.lock);

            // Allocate a new page and copy from swap file
            char *newPage = kalloc();
            fread(requester->pid, ((requester->pageFaultAddress) >> PTXSHIFT), newPage);

            // Update page table entry
            acquire(&swapInQueue.lock);
            acquire(&ptable.lock);
            mapSwapIn(requester->pgdir, (void *)requester->pageFaultAddress, PGSIZE, V2P(newPage));
            wakeup1(requester->chan);
        }
        release(&swapInQueue.lock);
        sleep(&swapInQueue, &ptable.lock);
    }
    exit();
}

void requestSwapOut()
{
    cprintf("%d requests for swapOut\n", myproc()->pid);
    acquire(&ptable.lock);
    acquire(&swapOutQueue.lock);
    enqueue(&swapOutQueue);
    wakeup1(&swapOutQueue);
    release(&swapOutQueue.lock);
    sleep(&requestSwapOut, &ptable.lock);
    while (myproc()->swapSatisfied == 0)
    {
        // wakeup(&swapOutQueue);
        sleep(&requestSwapOut, &ptable.lock);
    }
    myproc()->swapSatisfied = 0;
    release(&ptable.lock);
}

void requestSwapIn()
{
    cprintf("%d requests for swapIn for VA = %x\n", myproc()->pid, myproc()->pageFaultAddress);
    acquire(&ptable.lock);
    acquire(&swapInQueue.lock);
    enqueue(&swapInQueue);
    wakeup1(&swapInQueue);
    release(&swapInQueue.lock);
    struct proc *p = myproc();
    sleep((char *)p->pid, &ptable.lock);
    release(&ptable.lock);
}

int evictVictimPage(int pid)
{

    struct proc *p;
    struct proc *victimProcess[4] = {0, 0, 0, 0};
    pte_t *victimPTE[4] = {0, 0, 0, 0};
    uint victimVA[4] = {0, 0, 0, 0};
    pde_t *pte;
    for (int process = 0; process < NPROC; process++)
    {
        p = &ptable.proc[process];
        if (p->state == UNUSED || p->state == EMBRYO || p->state == RUNNING || p->pid < 5 || p->pid == pid)
            continue;

        for (uint i = PGSIZE; i < p->sz; i += PGSIZE)
        {
            pte = (pte_t *)getPTE(p->pgdir, (void *)i);
            if (!((*pte) & PTE_U) || !((*pte) & PTE_P))
                continue;
            int idx = getRDBits(pte);
            victimPTE[idx] = pte;
            victimVA[idx] = i;
            victimProcess[idx] = p;
        }
    }
    for (int i = 0; i < 4; i++)
    {
        if (victimPTE[i] != 0)
        {
            pte = victimPTE[i];
            int origstate = victimProcess[i]->state;
            char *origchan = victimProcess[i]->chan;
            victimProcess[i]->state = SLEEPING;
            victimProcess[i]->chan = 0;
            *pte = ((*pte) & (~PTE_P));
            *pte = *pte | ((uint)1 << 7);

            if (victimProcess[i]->state != ZOMBIE)
            {
                release(&swapOutQueue.lock);
                release(&ptable.lock);
                fwrite(victimProcess[i]->pid, (victimVA[i]) >> PTXSHIFT, (void *)P2V(PTE_ADDR(*pte)));
                acquire(&swapOutQueue.lock);
                acquire(&ptable.lock);
            }
            kfree((char *)P2V(PTE_ADDR(*pte)));
            lcr3(V2P(victimProcess[i]->pgdir));
            victimProcess[i]->state = origstate;
            victimProcess[i]->chan = origchan;
            return 1;
        }
    }
    return 0;
}

void swapOut()
{
    sleep(&swapOutQueue, &ptable.lock);
    cprintf("Just entered swapout\n");
    for (;;)
    {
        acquire(&swapOutQueue.lock);
        while (isEmpty(&swapOutQueue) == 0)
        {
            // Dequeue to process request
            struct proc *requester = getHead(&swapOutQueue);
            dequeue(&swapOutQueue);
            cprintf("Process swapout by %d %s\n", requester->pid, requester->name);

            // Select a victim page and evict it
            evictVictimPage(requester->pid);

            // Set the satisfied field of proc structure to notify process to resume
            requester->swapSatisfied = 1;
        }
        wakeup1(&requestSwapOut);
        release(&swapOutQueue.lock);
        sleep(&swapOutQueue, &ptable.lock);
    }
    exit();
}

void getSwapFileName(int pid, uint va, char *name)
{
    int i = 0;
    memset(name, 0, DIRSIZ);
    if (va == 0)
    {
        name[i++] = '0';
    }
    else
    {
        while (va > 0)
        {
            name[i++] = '0' + (va % 10);
            va /= 10;
        }
    }
    name[i++] = '_';
    if (pid == 0)
    {
        name[i++] = '0';
    }
    else
    {
        while (pid > 0)
        {
            name[i++] = '0' + (pid % 10);
            pid /= 10;
        }
    }
    int len = strlen(name);
    for (int k = 0; k < len / 2; k++)
    {
        char temp = name[k];
        name[k] = name[len - k - 1];
        name[len - k - 1] = temp;
    }
    i = (i < (DIRSIZ - 4)) ? i : DIRSIZ - 4;
    strncpy(&name[i], ".swp", 4);
}

void deleteSwapPages()
{
    acquire(&ptable.lock);
    struct proc *p;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
        if (p->state == UNUSED)
            continue;
        if (p->pid == 2 || p->pid == 3)
        {
            for (int fd = 0; fd < NOFILE; fd++)
            {
                if (p->ofile[fd])
                {
                    struct file *f;
                    f = p->ofile[fd];

                    if (f->ref < 1)
                    {
                        p->ofile[fd] = 0;
                        continue;
                    }
                    release(&ptable.lock);
                    if (f->ref == 1)
                        cprintf("Deleting page file: %s\n", f->name);
                    fdelete(p->ofile[fd]->name);
                    fileclose(f);
                    p->ofile[fd] = 0;

                    acquire(&ptable.lock);
                }
            }
        }
    }

    release(&ptable.lock);
}

int fwrite(int pid, uint addr, char *buf)
{
    char name[14];

    getSwapFileName(pid, addr, name);

    int fd = fopen(name, O_CREATE | O_WRONLY); // Open + create file
    struct file *f;
    if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
        return -1;

    int noc = filewrite(f, buf, 4096); // Write the page in the file
    if (noc < 0)
    {
        cprintf("Unable to write page!");
    }
    return noc;
}