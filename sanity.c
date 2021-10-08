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
                    double temp = 0;
                    for (int j = 0; j < 1000000; j++)
                    {
                        temp += 6.42324 * 4.3121;
                        temp -= 6.00001 * 4.812;
                        printf(1, "");
                    }
                }
            }
            else if (pid % 3 == 1)
            {
                // Short-task based CPU Bound Process
                for (int i = 0; i < 100; i++)
                {
                    double temp = 0;
                    for (int j = 0; j < 1000000; j++)
                    {
                        temp += 6.42324 * 4.3121;
                        temp -= 6.00001 * 4.812;
                        printf(1, "");
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
    int sleepTime[3];
    int readyTime[3];
    int turnTime[3];
    for (int i = 0; i < 3; ++i)
    {
        sleepTime[i] = 0;
        readyTime[i] = 0;
        turnTime[i] = 0;
    }
    // Statistics printing
    for (int i = 0; i < 3 * n; i++)
    {
        // retime, rutime, stime
        int terminatedPID = wait2(&retime, &rutime, &stime);
        char type[10];
        switch (terminatedPID % 3)
        {
        case 0:
            sleepTime[0] += stime;
            readyTime[0] += retime;
            turnTime[0] += (retime + stime + rutime);
            strcpy(type, "CPU");
            break;
        case 1:
            sleepTime[1] += stime;
            readyTime[1] += retime;
            turnTime[1] += (retime + stime + rutime);
            strcpy(type, "S-CPU");
            break;
        case 2:
            sleepTime[2] += stime;
            readyTime[2] += retime;
            turnTime[2] += (retime + stime + rutime);
            strcpy(type, "IO");
            break;
        }
        printf(1, "pid = %d; Type = %s, Wait = %d, Run = %d, IO = %d\n", terminatedPID, type, retime, rutime, stime);
    }
    for (int i = 0; i < 3; ++i)
    {
        if (i == 0)
        {
            printf(1, "CPU:\n");
        }
        else if (i == 1)
        {
            printf(1, "S-CPU:\n");
        }
        else
        {
            printf(1, "IO:\n");
        }
        printf(1, "Avg Sleep Time:%d Avg Ready Time:%d Avg Turn Time:%d\n", (sleepTime[i]) / n, readyTime[i] / n, turnTime[i] / n);
    }
    exit();
}
