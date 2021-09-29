#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf(1, "Invalid input format\n");
        exit();
    }
    int n = atoi(argv[1]);
    // Fork and create children
    for (int i = 0; i < 3 * n; i++)
    {
        int pid = fork();
        if (pid == 0)
        {                   // In child process
            pid = getpid(); // Child process's pid
            if (pid % 3 == 0)
            {
                // CPU Bound Process
                for (int i = 0; i < 100; i++)
                {
                    for (int j = 0; j < 1000000; j++)
                    {
                    }
                }
            }
            else if (pid % 3 == 1)
            {
                // Short-task based CPU Bound Process
                for (int i = 0; i < 100; i++)
                {
                    for (int j = 0; j < 1000000; j++)
                    {
                    }
                    yield();
                }
            }
            else
            {
                // IO Bound process
                for (int i = 0; i < 100; i++)
                {
                    sleep(1);
                }
            }
            exit();
        }
    }
    int retime, rutime, stime; // Wait , Running and Sleep (IO Wait) time
    // Statistics printing
    for (int i = 0; i < 3 * n; i++)
    {
        // retime, rutime, stime
        int terminatedPID = wait2(&retime, &rutime, &stime);
        char type[10];
        switch (terminatedPID % 3)
        {
        case 0:
            strcpy(type, "CPU");
            break;
        case 1:
            strcpy(type, "S-CPU");
            break;
        case 2:
            strcpy(type, "IO");
            break;
        }
        printf(1, "pid = %d; Type = %s, Wait = %d, Run = %d, IO = %d\n", terminatedPID, type, retime, rutime, stime);
    }
    exit();
}