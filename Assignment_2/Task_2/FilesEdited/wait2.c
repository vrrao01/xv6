#include "types.h"
#include "stat.h"
#include "user.h"

int main()
{
    int i = 0;

    while (i < 2)
    {
        i++;
        int retime, rutime, stime;
        fork();
        int pid = wait2(&retime, &rutime, &stime);
        if (pid == -1)
        {
            printf(1, "No terminated child found for pid = %d\n", getpid());
            continue;
        }
        printf(1, "parent pid=%d, child pid:%d Retime:%d STime:%d Rutime:%d\n", getpid(), pid, retime, stime, rutime);
    }
    exit();
}
