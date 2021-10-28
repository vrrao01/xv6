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
    // release(&ptable.lock);
    cprintf("Running swapIn\n");
    char text[14];
    getSwapFileName(10, 1234567, text);
    cprintf("GET NAME : %s\n", text);
    exit();
}

void requestSwapOut()
{
    acquire(&swapOutQueue.lock);
    enqueue(&swapOutQueue);
    wakeup(&swapOutQueue);
    sleep(&requestSwapOut, &swapOutQueue.lock);
    release(&swapOutQueue.lock);
}

pde_t *chooseVictim(struct proc **victim, uint *va)
{
    for (int rd = 0; rd <= 3; rd++)
    {
        // cprintf("rd = %d\n", rd);
        for (uint i = 0; i < NPROC; i++)
        {
            // cprintf("i = %d, state = %d, name = %s\n", i, ptable.proc[i].state, ptable.proc[i].name);
            if (!(ptable.proc[i].state == RUNNABLE || ptable.proc[i].state == ZOMBIE) || ptable.proc[i].pid < 4)
                continue;
            cprintf("Examining %s : %d\n", ptable.proc[i].name, ptable.proc[i].pid);
            for (int pg = 0; pg < ptable.proc[i].sz; pg += PGSIZE)
            {
                pde_t *pte = getPTE(ptable.proc[i].pgdir, (char *)pg);
                if (!((*pte) & PTE_U) || !((*pte) & PTE_P))
                    continue;
                int rdBits = getRDBits(pte);
                if (rdBits == rd)
                {
                    cprintf("Victim: pid = %d, va = %x, rd = %d\n", ptable.proc[i].pid, pg, rdBits);
                    cprintf("Victim flags = %x, name = %s,state = %d\n", PTE_FLAGS(*pte), ptable.proc[i].name, ptable.proc[i].state);
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
        if (!isEmpty(&swapOutQueue))
        {
            // Dequeue to process request
            struct proc *requester = getHead(&swapOutQueue);
            dequeue(&swapOutQueue);
            release(&swapOutQueue.lock);
            cprintf("Request for swapout by %d %s\n", requester->pid, requester->name);

            // Find a victim page
            struct proc *victimProcess;
            pde_t *victimPTE;
            uint victimVA;
            acquire(&ptable.lock);
            victimPTE = chooseVictim(&victimProcess, &victimVA);

            // Unset present bit and set swap bit in PTE
            *victimPTE = *victimPTE & ~PTE_P;
            *victimPTE = *victimPTE & ~PTE_SWP;
            lcr3(V2P(victimProcess->pgdir));

            // Swap out victim page and add to free list
            int oldState = victimProcess->state;
            victimProcess->state = BUSY;
            release(&ptable.lock);
            writeSwapPage(victimVA, victimProcess->pid, victimPTE);
            acquire(&ptable.lock);
            victimProcess->state = oldState;
            release(&ptable.lock);
            acquire(&swapOutQueue.lock);
            wakeup(&requestSwapOut);
        }
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
    getSwapFileName(pid, va, fName);
    cprintf("fName = %s \n", fName);
    int fd = fopen(fName, O_CREATE | O_WRONLY);
    char *victimPage = (char *)P2V(PTE_ADDR(*pte));
    fwrite(fd, victimPage, PGSIZE);
    fclose(fd);
    kfree(victimPage);
}