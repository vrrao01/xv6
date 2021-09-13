#include "types.h"
#include "stat.h"
#include "user.h"

int main()
{
    int i=0;
    
    while(i<4){
        i++;
        int retime, rutime,stime;
        fork();

        int pid = wait2(&retime, &rutime, &stime);
        if(pid==-1){
            printf(1,"Process failed\n"); continue;
        } 
        printf(1,"Pid:%d Retime:%d STime:%d Rutime:%d\n",pid ,retime ,stime,rutime);

    }
    exit();
}