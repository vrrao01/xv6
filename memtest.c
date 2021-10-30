#include "types.h"
#include "user.h"

#define PGSIZE 4096
#define ITERATIONS 20
#define CHILDREN 20

void child_process(int i)
{
    char *ptr[ITERATIONS];

    // Set all values
    for (int j = 0; j < ITERATIONS; j++)
    {
        ptr[j] = (char *)malloc(PGSIZE);
        for (int k = 0; k < PGSIZE; k++)
            ptr[j][k] = (i + j * k) % 128;
    }
    // Check all values
    for (int j = 0; j < ITERATIONS; j++)
        for (int k = 0; k < PGSIZE; k++)
            if (ptr[j][k] != (i + j * k) % 128)
                printf(1, "Error@ %d %d %d %c\n", i, j, k, ptr[j][k]);

    // printf(1, "CHILD: %d\n", i);
    exit();
}

int main(int argc, char *argv[])
{
    for (int i = 1; i <= CHILDREN; i++)
        if (!fork())
            child_process(i);

    for (int i = 1; i <= CHILDREN; i++)
    {
        int p = wait();
        printf(1, "%d ended\n", p);
    }
    exit();
}