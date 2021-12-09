/*
    Include directives
*/
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


/*
    List of Built-in commands 
*/
char *builtin_str[] =
{
  "cd",
  "help",
  "exit"
};

int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);

/*
    List of Built-in commands' functions
*/
int (*builtin_func[]) (char **) =
{
    &lsh_cd,
    &lsh_help,
    &lsh_exit
};


int lsh_num_builtins() 
{
    return sizeof(builtin_str)/sizeof(char *);
}

/*
    Built-in Shell commands function declarations
*/
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);







/*
    Implementation of the cd built-in function
*/
int lsh_cd(char **args)
{
    if(args[1] == NULL)
    {
        // cd command requires an argument; inform user of error
        fprintf(stderr, "LSH: Expected Argument to \"cd\"\n");
    }
    else
    {
        if(chdir(args[1]) != 0) // checks for errors
        {
            perror("LSH");
        }
    }
    return 1;
}

/*
    Implementation of the help built-in function
*/
int lsh_help(char **args)
{
    int i;

    printf("Mohammed Ibrahim's LSH\n");
    printf("Type Program Names and Arguments, and press Enter.\n");
    printf("The following are built in:\n");

    for(i = 0; i < lsh_num_builtins(); i++)
    {
        printf(" %s\n", builtin_str[i]); // print names of built-in commands
    }

    printf("Use the \"man\" command for more info on other programs.\n");
    return 1;
}

/*
    Implementation of the exit built-in function
*/
int lsh_exit(char **args)
{
    return 0; // simply exits
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

int lsh_execute(char **args)
{
    int i;

    if(args[0] == NULL) // checks for empty input
    {
        return 1;
    }

    for(i = 0; i < lsh_num_builtins(); i++)
    {
        if(strcmp(args[0], builtin_str[i]) == 0)
        {
            // returns built-in function with args
            return (*builtin_func[i])(args);
        }
    }
    // call lsh_launch function if command entered is not a built-in and pass args
    return lsh_launch(args);
}

#define LSH_RL_BUFFSIZE 1024
char *lsh_read_line(void)
{
    //simpifying lsh_read_line function with "getline" function
    char *line = NULL;
    ssize_t buffsize = 0; // getline() allocates buffer size
    
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