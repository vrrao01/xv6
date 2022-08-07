#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

int main(int argc, char *argv[])
{
    // Create a char buffer of size 2048 to store ASCII art
    char print_buffer[2048];

    // If draw system call is unsuccessful, print error message
    if (draw(&print_buffer, sizeof(print_buffer)) < 0)
    {
        printf(1, "drawtest: draw system call failed.\nTry increasing buffer size.\n");
        exit();
    }

    // Otherwise, print ASCII art stored in print_buffer
    printf(1, "%s", print_buffer);
    exit();
}