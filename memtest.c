#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

int main(int argc, char *argv[])
{
    for (int i = 0; i < 20; i++)
    {
        int pid = fork();
        if (pid == 0)
        {
            char *ptr = (char *)malloc(1048576);
            if (ptr == 0)
            {
                printf(1, "NO MEMORY\n");
                exit();
            }
            strcpy(ptr, "Hello World!\n");
            // printf(1, "%s", ptr);
        }
    }
    for (int i = 0; i < 1024; i++)
    {
        wait();
    }
    // char *ptr = (char *)malloc(1048576 * 1024);
    // if (ptr == 0)
    // {
    //     printf(1, "NO MEMORY\n");
    //     exit();
    // }
    // strcpy(ptr, "Hello World!\n");

    exit();
}