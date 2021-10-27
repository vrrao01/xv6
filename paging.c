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
        for (uint i = 0; i < NPROC; i++)
        {
            if (ptable.proc[i].state != 7 || ptable.proc[i].pid < 4)
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
    // release(&ptable.lock);
    // int fd = fopen("random.swap", O_CREATE | O_RDWR);
    // char text[512];
    // cprintf("fd = %d\n", fd);
    // fwrite(fd, text, strlen(text));
    // fclose(fd);
    // int fd = fopen("README", O_RDONLY);
    // fread(fd, text, 512);
    // cprintf("After reading\n");
    // cprintf("|%s|\n ", text);
    // fclose(fd);
    acquire(&swapOutQueue.lock);
    while (1)
    {
        if (!isEmpty(&swapOutQueue))
        {
            cprintf("Request for swapout by %d %s\n", getHead(&swapOutQueue)->pid, getHead(&swapOutQueue)->name);
            struct proc *victimProcess;
            // pde_t *victimPTE;
            uint victimVA;
            acquire(&ptable.lock);
            // victimPTE = chooseVictim(&victimProcess, &victimVA);
            chooseVictim(&victimProcess, &victimVA);
            // writeSwapPage(victimVA, victimProcess->pid, victimPTE);
            // updateVictimPage()
            dequeue(&swapOutQueue);
            // change satisfied condition
            release(&ptable.lock);
            wakeup(&requestSwapOut);
        }
        sleep(&swapOutQueue, &swapOutQueue.lock);
        cprintf("Swapout sleeping\n");
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
    char *victimPage = P2V(PTE_ADDR(*pte));
    fwrite(fd, victimPage, PGSIZE);
    fclose(fd);
}