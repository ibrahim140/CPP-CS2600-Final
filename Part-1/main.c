/*
    Include directives
*/
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


void lsh_loop()
{
    char *line, **args;
    int status;

    do
    {
        printf("> "); // print prompt
        line = lsh_read_line(); // function to read line
        args = lsh_split_line(line); // split the previous line into arguments
        status = lsh_execute(args); // execute the arguments from the line

        free(line); // free line for next line
        free(args); // free args for next line arguments
    } while(status); // exit condition
}

int main(int argc, char **argv)
{
    // load config files & run command loop.
    lsh_loop();
    
    return EXIT_SUCCESS;
}