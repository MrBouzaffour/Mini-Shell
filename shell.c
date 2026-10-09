/*
* Ahmed Bouzaffour
*/

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

/*Constants*/
#define DELIMITERS " \t\r\n"
#define INITIAL_TOKENS_CAPACITY 8
#define MAX_TOKENS 64

/*Global variables*/
int shell_status = 0;

/*Types*/
typedef struct builtin_command
{
    const char *name;
    int (*function)(char **args);
}BUILTIN_COMMAND;


/* Function Prototypes*/
char * read_line(void);
char **split_line(char *line);

int is_delimiter(char c);
int count_pipes(char **args);
int find_executable(const char *command,
        char *path, size_t path_size);
int find_pipe(char **args);
int close_pipe(int pipe_fd[2]);
void execute_pipe(char **args);
void execute_command(char **args);
void launch_command(char **args);

int builtin_exit(char **args);
int builtin_cd(char **args);


/* built in command table*/
static BUILTIN_COMMAND builtin_commands[] =
{
    {"exit", builtin_exit},
    {"cd", builtin_cd},
    {NULL, NULL}
};

static const char *search_paths[] =
{
    "/bin",
    "/usr/bin",
    "/usr/local/bin",
    NULL
};

/*
 * Prints the shell prompt and 
 * reads one complete line from stdin
 *
 * Returns:
 *     Pointer to the allocated input line on success
 *     NULL on EOF or failure
 */
char *
read_line(void)
{
    char *line;
    size_t line_capacity;
    char cwd[BUFSIZ];

    line = NULL;
    line_capacity = 0;

    if (getcwd(cwd,sizeof(cwd)) == NULL)
    {
        perror("getcwd");
        return NULL;
    }

    printf("%s $> ",cwd);
    fflush(stdout);

    if (getline(&line, &line_capacity, stdin) == -1)
    {
        if (feof(stdin))
            printf("[EOF]\n");
        else
            perror("getline");
        free(line);
        return NULL;
    }
    return line;
}

/*
 * Checks whether a character is one of the characters in DEL
 *
 * Returns:
 *     1 if c is a delimiter
 *     0 otherwise
 */
int
is_delimiter(char c)
{
    int index;

    index =0;
    while (DELIMITERS[index] != '\0')
    {
        if (c == DELIMITERS[index])
            return 1;
        index++;
    }
    return 0;
}

/*return the number of pipes in the argument array*/
int
count_pipes (char **args)
{
    int index;
    int pipe_count;
    
    if (args == NULL)
        return 0;

    index = 0;
    pipe_count = 0;

    while(args[index] != NULL)
    {
        if(strcmp(args[index], "|") == 0)
            pipe_count++;
        index++;
    }
    return pipe_count;
}

int
find_pipe(char **args)
{
    int index;

    if (args == NULL)
        return -1;

    index = 0;

    while (args[index] != NULL)
    {
        if (strcmp(args[index], "|") == 0)
            return index;

        index++;
    }

    return -1;
}

/*
 * Tokenizes line into an array of strings
 * This function implements its own tokenizer instead of using strtok()
 * Delimiter characters are replaced with '\0'
 * The returned array always ends with a NULL pointer.
 */
char **
split_line(char *line)
{
    char **tokens;
    char **resized_tokens;
    char *cursor;
    unsigned int token_count;
    size_t capacity;

    capacity = INITIAL_TOKENS_CAPACITY;
    token_count = 0;

    tokens = malloc(capacity * sizeof(*tokens));
    if (tokens == NULL)
    {
        perror("Malloc");
        exit(EXIT_FAILURE);
    }

    cursor = line;

    while (*cursor != '\0')
    {
        /*skip dilimiter characters*/
        while(*cursor != '\0' && is_delimiter(*cursor))
            cursor++;

        /*EOL*/
        if (*cursor == '\0')
            break;

        /* reject commands containing too many tokens*/
        if (token_count >= MAX_TOKENS)
        {
            fprintf(stderr, "Error: too many arguments\n");
            free(tokens);
            return NULL;
        }

        /* an extra position for the final NULL*/
        if (token_count +1 >= capacity)
        {
            capacity *=2;
            if (capacity > MAX_TOKENS +1)
                capacity = MAX_TOKENS +1;

            resized_tokens = realloc(tokens, capacity * sizeof *tokens);
            if (resized_tokens == NULL)
            {
                free(tokens);
                perror("Realloc");
                exit(EXIT_FAILURE);
            }
            tokens = resized_tokens;
        }

        /*cuurent points to the beginning of the token*/
        tokens[token_count] = cursor;
        token_count++;

        /*move until the next delimiter*/
        while (*cursor != '\0' && !is_delimiter(*cursor))
            cursor++;

        if (*cursor != '\0')
        {
            *cursor = '\0';
            cursor++;
        }
    }
    tokens[token_count] = NULL;

    return tokens;
}

/*
 * shell's exit built-in command
 */
int
builtin_exit(char **args)
{
    (void)args;

    printf("Goodbye.\n");

    exit(EXIT_SUCCESS);

    return 0;
}

/*
 * shell's cd built-in commad 
 */
int
builtin_cd(char **args)
{
    if(args[1] == NULL)
    {
        fprintf(stderr,"cd: missing directory\n");
        return -1;
    }
    if (args[2] != NULL)
    {
        fprintf(stderr,"cd: too many arguments\n");
        return -1;
    }

    if (chdir(args[1]) == -1)
    {
        perror("cd");
        return -1;
    }
    return 0;
}

int
close_pipe(int pipe_fd[2])
{
    int failed;
    failed = 0;

    if (close(pipe_fd[0]) == -1)
    {
        perror("close");
        failed = 1;
    }

    if (close(pipe_fd[1]) == -1)
    {
        perror("close");
        failed = 1;
    }
    return failed;
}

/*
 * Find the executable corresponding to command
 * If command contains '/' it is treated as a user-supplied path
 * Otherwise the coded executable directories are searched
 * Returns:
 *     0 if an executable is found
 *    -1 if command is not foud
 *    -2 if command exisits but is not executable
 */
int 
find_executable(const char *command, char *path, size_t path_size)
{
    int path_index;
    int found_not_executable;

    if (command == NULL || path == NULL)
        return -1;

    /* The user provided a path*/
    if (strchr(command, '/') != NULL)
    {
        if (access(command, F_OK) == -1)
            return -1;

        if (access(command, X_OK) == -1)
            return -2;

        snprintf(path,
                path_size,
                "%s",
                command);
        return 0;
    }

    /* search the coded executable locations*/
    path_index = 0;
    found_not_executable = 0;

    while (search_paths[path_index] != NULL)
    {
        snprintf(path,
                path_size,
                "%s/%s",
                search_paths[path_index],
                command);
        if (access(path,F_OK) == 0)
        {
            if (access(path, X_OK) == 0)
                return 0;

            found_not_executable = 1;
        }
        path_index++;
    }
    if (found_not_executable)
        return -2;

    return -1;
}

/*
 * Executes two commands connected by one pipe
 * Left command writes to the pipe
 * Right command reads from the pipe
 */
void
execute_pipe(char **args)
{
    int pipe_index;
    int pipe_fd[2];
    int left_status;
    int right_status;
    int result;

    pid_t left_pid;
    pid_t right_pid;

    char **left_args;
    char **right_args;

    char left_path[BUFSIZ];
    char right_path[BUFSIZ];

    /*
     * Find location of "|"
     */
    pipe_index = find_pipe(args);

    if (pipe_index == -1)
        return;

    if (pipe_index == 0)
    {
        fprintf(stderr,
                "Error: missing command before pipe\n");
        return;
    }

    if (args[pipe_index + 1] == NULL)
    {
        fprintf(stderr,
                "Error: missing command after pipe\n");
        return;
    }

    /*
     * Split the argument array
     */
    args[pipe_index] = NULL;

    left_args = args;
    right_args = &args[pipe_index + 1];

    /*
     * Find executable for left command
     */
    result = find_executable(left_args[0],
                        left_path,
                        sizeof(left_path));
    if (result == -1)
    {
        fprintf(stderr,
                "%s: command not found\n",
                left_args[0]);
        return;
    }

    if (result == -2)
    {
        fprintf(stderr,
                "%s: not executable\n",
                left_args[0]);
        return;
    }
    /*
     * Find executable for right command
     */
    result = find_executable(right_args[0],
                        right_path,
                        sizeof(right_path));
    if (result == -1)
    {
        fprintf(stderr,
                "%s: command not found\n",
                right_args[0]);
        return;
    }

    if (result == -2)
    {
        fprintf(stderr,
                "%s: not executable\n",
                right_args[0]);
        return;
    }
    /*
     * Create the pipe
     * pipe_fd[0] = read end
     * pipe_fd[1] = write end
     */
    if (pipe(pipe_fd) == -1)
    {
        perror("pipe");
        return;
    }

    /*
     * Create left child
     */
    left_pid = fork();

    if (left_pid < 0)
    {
        perror("fork");
        close_pipe(pipe_fd);
        return;
    }

    if (left_pid == 0)
    {
        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1)
        {
            perror("dup2");
            _exit(EXIT_FAILURE);
        }

        /*
         * The duplicated descriptor is now stdout
         */
        if (close_pipe(pipe_fd))
            _exit(EXIT_FAILURE);

        execv(left_path, left_args);

        perror("execv");
        _exit(EXIT_FAILURE);
    }

    /*
     * Create right child.
     */
    right_pid = fork();

    if (right_pid < 0)
    {
        perror("fork");

        close_pipe(pipe_fd);

        if (waitpid(left_pid, NULL, 0) == -1)
            perror("waitpid");

        return;
    }

    if (right_pid == 0)
    {
        /*
         * Redirect stdin to the pipe's read end
         */
        if (dup2(pipe_fd[0], STDIN_FILENO) == -1)
        {
            perror("dup2");
            _exit(EXIT_FAILURE);
        }

        if (close_pipe(pipe_fd))
            _exit(EXIT_FAILURE);

        execv(right_path, right_args);

        perror("execv");
        _exit(EXIT_FAILURE);
    }

    /*
     * Parent does not read from or write to the pipe
     */
    close_pipe(pipe_fd);

    /*
     * Wait for both children
     */
    if (waitpid(left_pid, &left_status, 0) == -1)
        perror("waitpid");

    if (waitpid(right_pid, &right_status, 0) == -1)
    {
        perror("waitpid");
    }
    else if (WIFEXITED(right_status))
    {
        shell_status = WEXITSTATUS(right_status);
    }
}

/*
 * 1. Find the executable.
 * 2. fork().
 * 3. Call execv() in the child.
 * 4. wait() in the parent
 */
void
launch_command(char **args)
{
    pid_t child_pid;
    int child_status;
    char executable_path[BUFSIZ];
    int result;

    if (args == NULL || args[0] == NULL)
        return;
    /*
     * There is 2 cases:
     * 1) the command conaints '/'
     * 2) the command does not contain a path
     */
    result = find_executable(args[0], executable_path,
                            sizeof(executable_path));
    if (result == -1)
    {
        fprintf(stderr,
                "%s: command not found\n",
                args[0]);
        return;
    }

    if (result == -2)
    {
        fprintf(stderr,
                "%s: not executable\n",
                args[0]);
        return;
    }

    /* Create child process*/
    child_pid= fork();
    if(child_pid < 0)
    {
        perror("fork");
        return;
    }
    /* Child process*/
    if (child_pid == 0)
    {
        execv (executable_path,args);

        /* if the execution is failed*/
        perror("execv");
        _exit(EXIT_FAILURE);
    }
    
    /*parent wait*/
    if (waitpid(child_pid, &child_status, 0) == -1)
    {
        perror("waitpid");
        return;
    }
    /* store the child's exit status*/
    if (WIFEXITED(child_status))
        shell_status = WEXITSTATUS(child_status);
}

/*
 * Checks whether the requested command is a built in
 * If it is a built-in, the corresponding function is calle.
 * Otherwise, the command is passed to launch_command()
*/
 void
execute_command(char **args)
{
    int builtin_index;
    int pipe_count;
 
    if (args == NULL || args[0] == NULL)
        return;
    
    pipe_count = count_pipes(args);

    if (pipe_count > 1 )
    {
        fprintf(stderr,"Error: only one pipe is supported\n");
        return;
    }
    if (pipe_count == 1)
    {
        execute_pipe(args);
        return;
    }

    builtin_index = 0;
    while(builtin_commands[builtin_index].name != NULL)
    {
        if (strcmp(builtin_commands[builtin_index].name,args[0]) == 0)
        {
            shell_status = builtin_commands[builtin_index].function(args);
            return;
        }
        builtin_index++;
    }
    launch_command(args);
}

/* Main loop*/
int
main(void)
{
    char *line;
    char **args;

    /* 
     * Read -> Execute -> Print -> Loop 
     * 1/ prompt + get line
     */
    while((line = read_line()) != NULL)
    {
        /*2) get token*/
        args = split_line(line);
        
        if (args != NULL)
        {
            /*3) exec*/
            execute_command(args);
            free(args);
        }

        free(line);
    }
    return EXIT_SUCCESS;
}
