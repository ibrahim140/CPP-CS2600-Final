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

char *lsh_read_line(void)
{
    //variable declarations
    int buffsize = LSH_RL_BUFFSIZE, position = 0, c;
    char *buffer = malloc(sizeof(char) *buffsize);

    // check for buffer allocation and print error if necessary
    if(!buffer)
    {
        fprintf(stderr, "LSH: Allocation Error.\n");
        exit(EXIT_FAILURE);
    }

    while(1)
    {
        c = getchar();
        // check 'c' for end of file or newline 
        if(c == EOF || c == '\n');
        {
            buffer[position] = NULL; // null terminate current string
            return buffer; // return current string
        }
        else
        {
            buffer[position] = c; // add c to current string
        }
        position++; // increment position of c in line

        // check if buffer size is enough
        if(position >= buffsize) 
        {
            // reallocate buffer
            buffsize += LSH_RL_BUFFSIZE;
            buffer = realloc(buffer, buffsize);
            // check for buffer allocation and print error if necessary
            if(!buffer)
            {
                fprintf(stderr, "LSH: Allocation Error.\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    /*
    simpifying lsh_read_line function with "getline" function
    char *line = NULL;
    ssize_t buffsize = 0;
    if(getline(&line, &buffsize, stdin) == -1)
    {
        if(feof(stdin))
        {
            exit(EXIT_SUCCESS);
        }
        else
        {
            perror("Read Line");
            exit(EXIT_FAILURE);
        }
    }
    return line;
    */
}

int main(int argc, char **argv)
{
    // load config files & run command loop.
    lsh_loop();
    
    return EXIT_SUCCESS;
}