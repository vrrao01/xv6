#include "types.h"
#include "user.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf(1, "Invalid input format\n");
        exit();
    }
    int n = atoi(argv[1]);
    for (int i = 0; i < 3 * n; i++) {
		int pid = fork();
		if (pid == 0) {
            pid = getpid();
		
			switch(pid%3) {
				case 0:
                    set_prio(1);
					break;
				case 1:
                    set_prio(2);
					break;
				case 2:
                    set_prio(3);
					break;
			}
			
            for (int k = 0; k < 100; k++){
                for (int j = 0; j < 1000000; j++){
                    printf(1,"");
                }
            }
		    exit(); // children exit here
		}
	}

    int retime, rutime, stime; // Wait , Running and Sleep (IO Wait) time
    int sleepTime[3];
    int readyTime[3];
    int turnTime[3];
    for(int i=0; i<3; ++i){
        sleepTime[i] =0;
        readyTime[i] =0;
        turnTime[i] =0;
    }
    // Statistics printing
    for (int i = 0; i < 3 * n; i++)
    {
        // retime, rutime, stime
        int terminatedPID = wait2(&retime, &rutime, &stime);
        switch (terminatedPID % 3)
        {
        case 0:
            sleepTime[0]+=stime;
            readyTime[0]+=retime;
            turnTime[0]+=(retime+stime+rutime);
            break;
        case 1:
            sleepTime[1]+=stime;
            readyTime[1]+=retime;
            turnTime[1]+=(retime+stime+rutime);
            break;
        case 2:
            sleepTime[2]+=stime;
            readyTime[2]+=retime;
            turnTime[2]+=(retime+stime+rutime);
            break;
        }
        //printf(1, "Priority = %d, Wait = %d, Run = %d, IO = %d\n", i+1, retime, rutime, stime);
    }
    for(int i=0; i<3; ++i){
        if(i==0){
            printf(1,"Priority 1:\n");
        }else if(i==1){
            printf(1,"Priority 2:\n");
        }else{
            printf(1,"Priority 3:\n");
        }
        printf(1,"Avg Sleep Time:%d Avg Ready Time:%d Avg Turn Time:%d\n", (sleepTime[i])/n, readyTime[i]/n, turnTime[i]/n);
    }
    exit();
}