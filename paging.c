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
    temp = ((3 << 5 & *pte) >> 5);
    if (temp == 1 || temp == 2)
    {
        return 3 - temp;
    }
    return temp;
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

void swapIn()
{
    acquire(&swapInQueue.lock);
    cprintf("Just entered swapin\n");
    while (1)
    {
        if (isEmpty(&swapInQueue) == 0)
        {
            // Dequeue to process request
            struct proc *requester = getHead(&swapInQueue);
            dequeue(&swapInQueue);
            release(&swapInQueue.lock);
            cprintf("Request for swapIn by %d :%x\n", requester->pid, requester->pageFaultAddress);

            // Allocate a new page and copy from swap file
            char *newPage = kalloc();
            char fName[16];
            getSwapFileName(requester->pid, (requester->pageFaultAddress >> 12), fName);
            int fd = fopen(fName, O_RDONLY);
            fread(fd, newPage, PGSIZE);
            fclose(fd);

            // Update page table entry
            pte_t *pte = getPTE(requester->pgdir, (void *)requester->pageFaultAddress);
            *pte = *pte | PTE_P;
            *pte = *pte & ~PTE_SWP;
            lcr3(V2P(requester->pgdir));
            requester->swapSatisfied = 1;
            acquire(&swapInQueue.lock);
        }
        wakeup(&requestSwapIn);
        sleep(&swapInQueue, &swapInQueue.lock);
    }
    exit();
}

void requestSwapOut()
{
    cprintf("%d requests for swapOut\n", myproc()->pid);
    acquire(&swapOutQueue.lock);
    enqueue(&swapOutQueue);
    wakeup(&swapOutQueue);
    sleep(&requestSwapOut, &swapOutQueue.lock);
    while (myproc()->swapSatisfied == 0)
    {
        sleep(&requestSwapOut, &swapOutQueue.lock);
    }
    myproc()->swapSatisfied = 0;
    release(&swapOutQueue.lock);
}

void requestSwapIn()
{
    cprintf("%d requests for swapIn for VA = %x\n", myproc()->pid, myproc()->pageFaultAddress);
    acquire(&swapInQueue.lock);
    enqueue(&swapInQueue);
    wakeup(&swapInQueue);
    sleep(&requestSwapIn, &swapInQueue.lock);
    while (myproc()->swapSatisfied == 0)
    {
        sleep(&requestSwapIn, &swapInQueue.lock);
    }
    myproc()->swapSatisfied = 0;
    release(&swapInQueue.lock);
}

pde_t *chooseVictim(struct proc **victim, uint *va)
{
    for (int rd = 0; rd <= 3; rd++)
    {
        // cprintf("rd = %d\n", rd);
        for (uint i = 0; i < NPROC; i++)
        {
            // cprintf("i = %d, state = %d, name = %s\n", i, ptable.proc[i].state, ptable.proc[i].name);
            if (!(ptable.proc[i].state == RUNNABLE || ptable.proc[i].state == ZOMBIE) || ptable.proc[i].pid < 4 || ptable.proc[i].pid == myproc()->pid)
                continue;
            // cprintf("Examining %s : %d\n", ptable.proc[i].name, ptable.proc[i].pid);
            for (int pg = 0; pg < ptable.proc[i].sz; pg += PGSIZE)
            {
                pde_t *pte = getPTE(ptable.proc[i].pgdir, (char *)pg);
                if (!((*pte) & PTE_U) || !((*pte) & PTE_P))
                    continue;
                int rdBits = getRDBits(pte);
                if (rdBits == rd)
                {
                    // cprintf("Victim: pid = %d, va = %x, rd = %d\n", ptable.proc[i].pid, pg, rdBits);
                    // cprintf("Victim flags = %x, name = %s,state = %d\n", PTE_FLAGS(*pte), ptable.proc[i].name, ptable.proc[i].state);
                    *victim = &ptable.proc[i];
                    *va = pg;
                    return pte;
                }
            }
        }
    }
    return 0;
}

void swapOut()
{
    acquire(&swapOutQueue.lock);
    cprintf("Just entered swapout\n");
    while (1)
    {
        if (isEmpty(&swapOutQueue) == 0)
        {
            // Dequeue to process request
            struct proc *requester = getHead(&swapOutQueue);
            dequeue(&swapOutQueue);
            release(&swapOutQueue.lock);
            cprintf("Process swapout by %d %s\n", requester->pid, requester->name);

            // Find a victim page
            struct proc *victimProcess;
            pde_t *victimPTE;
            uint victimVA;
            acquire(&ptable.lock);
            victimPTE = chooseVictim(&victimProcess, &victimVA);

            // Unset present bit and set swap bit in PTE
            *victimPTE = *victimPTE & ~PTE_P;
            *victimPTE = *victimPTE | PTE_SWP;
            lcr3(V2P(victimProcess->pgdir));

            // Swap out victim page and add to free list
            int oldState = victimProcess->state;
            victimProcess->state = BUSY;
            release(&ptable.lock);
            writeSwapPage(victimVA, victimProcess->pid, victimPTE);
            acquire(&ptable.lock);
            victimProcess->state = oldState;
            victimProcess->swapSatisfied = 1;
            release(&ptable.lock);
            acquire(&swapOutQueue.lock);
        }
        wakeup(&requestSwapOut);
        sleep(&swapOutQueue, &swapOutQueue.lock);
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

void writeSwapPage(uint va, int pid, pte_t *pte)
{
    char fName[DIRSIZ];
    getSwapFileName(pid, (va >> PTXSHIFT), fName);
    cprintf("fName = %s \n", fName);
    int fd = fopen(fName, O_CREATE | O_WRONLY);
    char *victimPage = (char *)P2V(PTE_ADDR(*pte));
    fwrite(fd, victimPage, PGSIZE);
    fclose(fd);
    kfree(victimPage);
}

void handlePageFault()
{
    uint pageFaultVA = myproc()->pageFaultAddress;
    pte_t *faultPTE = getPTE(myproc()->pgdir, (void *)pageFaultVA);
    if ((*faultPTE & PTE_SWP) != 0)
    {
        cprintf("%d pid's VA = %x was swapped out\n", myproc()->pid, myproc()->pageFaultAddress);
        myproc()->pageFaultAddress = PGROUNDDOWN(pageFaultVA);
        requestSwapIn();
    }
    else
    {
        cprintf("Segmentation Fault: Process accessing outside its address space -- kill proc\n");
        myproc()->killed = 1;
    }
}