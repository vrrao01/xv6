#include "sysdraw.h"
#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// Kernel code for draw system call.
// Uses the ASCII art defined in sysdraw.h
int sys_draw(void)
{
    // System call arguments
    char *buffer;
    uint buffer_size;

    // Fetch the second argument from stack
    // If the argument is invalid return -1
    if (argint(1, (int *)&buffer_size) < 0)
    {
        return -1;
    }

    // If buffer is of insufficient size, return -1
    if (buffer_size < strlen(wolf))
    {
        return -1;
    }

    // Fetch user supplied buffer pointer (first argument) from stack
    // If argument is invalid return -1
    if (argptr(0, &buffer, buffer_size) < 0)
    {
        return -1;
    }

    // Copy ASCII art into user provided buffer
    memmove(buffer, wolf, strlen(wolf));

    return strlen(wolf);
}