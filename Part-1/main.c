/*
    Include directives
*/
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    // load config files & run command loop.
    lsh_loop();
    
    return EXIT_SUCCESS;
}