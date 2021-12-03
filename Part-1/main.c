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

#define LSH_RL_BUFFSIZE 1024
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

#define LSH_TOK_BUFFSIZE 64
#define LSH_TOK_DELIM " \t\r\n\a"
char **lsh_split_line(char *line)
{
    int buffsize = LSH_TOK_BUFFSIZE, position = 0;
    char **tokens = malloc(buffsize * sizeof(char*)), *token;

    if (!tokens)
    {
        fprintf(stderr, "LSH: Allocation Error.\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, LSH_TOK_DELIM); // get pointers to store as an array of character pointers
    while(token != NULL)
    {
        // store character pointer(s) in array
        tokens[position] = token;
        position++;

        if(position >= buffsize) // check for buffer size 
        {
            // reallocate if buffer size is not enough
            buffsize += LSH_TOK_BUFFSIZE;
            tokens = realloc(tokens, buffsize * sizeof(char*));
            if(!tokens)
            {
                fprintf(stderr, "LSH: Allocation Error.\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, LSH_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

int lsh_launch(char **args)
{
    pid_t pid, wpid;
    int status;

    pid = fork();

    if(pid == 0) // child process
    {
        if(execvp(args[0], args) == -1) // check for execvp error
        {
            perror("LSH");
        }
        exit(EXIT_FAILURE);
    }
    else if(pid < 0) // check for fork() error
    {
        perror("LSH");
    }
    else
    { // parent process
        do // wait for process to exit or be terminated
        {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}

int main(int argc, char **argv)
{
    // load config files & run command loop.
    lsh_loop();
    
    return EXIT_SUCCESS;
}