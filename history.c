#include "types.h"
#include "user.h"

int main(int argc, char *argv[])
{
    char buffer[128];
    int hasHistory;
    for (int i = 0; i < 16; i++)
    {
        memset(buffer, 0, 128);
        if (history(buffer, i) != 0)
        {
            break;
        }
        hasHistory = 1;
        printf(1, "%d : %s\n", i, buffer);
    }
    if (hasHistory == 0)
    {
        printf(1, "No history available \n");
    }
    exit();
}
