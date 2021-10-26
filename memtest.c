#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

int main(int argc, char *argv[])
{
    char *ptr = (char *)malloc(1048576 * 1024);
    strcpy(ptr, "Hello World!\n");
    printf(1, "%s", ptr);
    exit();
}