#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <limits.h> // Include the header file that defines PATH_MAX
#include <linux/limits.h>

#define MAX_INPUT_SIZE 1024
#define MAX_TOKENS 64          // Upper limit of arguements that can be passed
const char *DELIM = " \r\n\a"; // Delimiters for strtok, this will split the string by

char *path[MAX_TOKENS] = {"/bin", "/usr/bin", NULL};
int redirect = 0;

void execute_command(char **args);
void execute_parallel_commands(char **commands);
void redirect_output(char **args, char *output_file);
int handle_builtin_commands(char **args);
void parse_and_execute(char *line);
char *find_executable(char *command);

int main(int argc, char *argv[])
{

    char *line = NULL;   // Pointer to the buffer that will hold the input line, initially set to NULL for dynamic allocation
    size_t len = 0;      // Variable to hold the size of the buffer, initially set to 0
    ssize_t nread;       // Variable to hold the number of characters read by getline()
    FILE *input = stdin; // File pointer for input, initially set to standard input (stdin)

    if (argc == 2)
    {                                // if there's more than one argument (batch file)
        input = fopen(argv[1], "r"); // open the file in read mode
        if (!input)
        { // if the file cannot be opened
            fprintf(stderr, "wish: cannot open file %s\n", argv[1]);
            exit(1);
        }
    }

    while (1)
    { // infinite loop to keep the shell running
        if (input == stdin)
        {
            printf("wish> "); // print the prompt
        }

        nread = getline(&line, &len, input); // read the line from the input into, nread holds the number of characters read
        if (nread == -1)
        { // if -1 that means the end of the file has been reached or there's nothing in the file
            break;
        }

        printf("Read line: %s", line);

        parse_and_execute(line); // parse and execute the command
    }

    free(line); // free the memory allocated for the line buffer
    if (input != stdin)
    {
        fclose(input); // close the input file if it's not standard input
    }

    return 0;
}

void parse_and_execute(char *line)
{
    printf("Parsing line: %s", line);

    char *commands[MAX_TOKENS]; // Array where each element is a pointer to a command separated by "&"
    char *command;              // Pointer to hold the command
    int i = 0;

    command = strtok(line, "&"); // Split the line by "&"
    while (command != NULL)
    {                                // Loop through the commands
        commands[i++] = command;     // Add the command to the array
        command = strtok(NULL, "&"); // Move to the next command, strtok will continue using line, if you put the first argument as NULL
    }
    commands[i] = NULL; // Set the last element of the array to NULL so we know where the end is

    // LOGGING
    printf("Parsed commands:\n");
    for (int j = 0; j < i; j++)
    {
        printf("  Command %d: %s\n", j, commands[j]);
    }

    if (i > 1)
    { // If there are multiple commands, execute
        execute_parallel_commands(commands);
    }
    else
    {                           // If there's only one command, execute
        char *args[MAX_TOKENS]; // Array where each element is a pointer to an argument separated by " ", for example ls -l would be ["ls", "-l"]
        char *arg;              // Pointer to hold the argument
        int j = 0;

        char *output_file = NULL; // Output file for Redirection
        int saved_stdout = -1, saved_stderr = -1;

        arg = strtok(commands[0], DELIM);
        while (arg != NULL)
        {
            if (strcmp(arg, ">") == 0)
            {
                // Redirection detected
                output_file = strtok(NULL, DELIM);
                if (output_file == NULL || strtok(NULL, DELIM) != NULL)
                {
                    fprintf(stderr, "wish: invalid redirection syntax\n");
                    return;
                }
                break; // Stop parsing further args bc we dont expect there to be anymore
            }
            args[j++] = arg;
            arg = strtok(NULL, DELIM);
        }
        args[j] = NULL;

        // LOGGING
        printf("Parsed arguments:\n");
        for (int k = 0; k < j; k++)
        {
            printf("  Arg %d: %s\n", k, args[k]);
        }
        if (output_file)
        {
            printf("Redirection to file: %s\n", output_file);
        }

        if (args[0] == NULL)
        { // If there are no arguments, return
            return;
        }

        if (handle_builtin_commands(args))
        { // Handle builtin commands, if the command is builtin, execute it. This is only in a single command, not in parallel commands
            return;
        }
        if (output_file)
        {
            // Open the output file
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC);
            if (fd < 0)
            {
                perror("wish");
                exit(1);
            }
            // Save original stdout and stderr
            saved_stdout = dup(STDOUT_FILENO);
            saved_stderr = dup(STDERR_FILENO);

            // Redirect stdout and stderr to the file
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        execute_command(args); // If the command is not builtin, execute it

        // Restore stdout and stderr if redirection occurred
        if (output_file)
        {
            dup2(saved_stdout, STDOUT_FILENO);
            dup2(saved_stderr, STDERR_FILENO);
            close(saved_stdout);
            close(saved_stderr);
        }
    }
}

void execute_command(char **args)
{
    printf("Executing\n");

    pid_t pid; // Process ID for the child process and the parent process
    int status;

    pid = fork();
    if (pid == 0)
    { // Child process
        char *executable = find_executable(args[0]);
        if (executable == NULL)
        { // If the executable is not found
            fprintf(stderr, "wish: %s: command not found\n", args[0]);
            exit(1);
        }
        execv(executable, args); // Execute the command with the arguments
        perror("wish");          // If execv fails, print an error message, this should never be reached because execv will replace the current process with the new process and shouldnt return
        exit(1);
    }
    else if (pid < 0)
    {
        // Error forking
        perror("wish");
    }
    else
    {
        // Parent process
        do
        {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
}

void execute_parallel_commands(char **commands)
{
    pid_t pid;
    int i = 0;

    while (commands[i] != NULL)
    {
        pid = fork(); // Fork a new process, commands are run parallely so we fork for each command
        if (pid == 0)
        { // If we are in the child process
            char *args[MAX_TOKENS];
            char *arg;
            int j = 0;

            arg = strtok(commands[i], DELIM); // Split the command by " "
            while (arg != NULL)
            {
                args[j++] = arg;
                arg = strtok(NULL, DELIM); // Move to the next argument
            }
            args[j] = NULL; // Set the last element of the array to NULL so we know where the end is

            if (args[0] == NULL)
            { // If there are no arguments, return
                exit(1);
            }

            char *executable = find_executable(args[0]);
            if (executable == NULL)
            { // If the executable is not found
                fprintf(stderr, "wish: %s: command not found\n", args[0]);
                exit(1);
            }

            execv(executable, args); // Execute the command with the arguments for example ls -l would be ["ls", "-l"] so args[0] = "ls" and args[1] = "-l" or echo hello world
            perror("wish");          // If execv fails, print an error message, this should never be reached because execv will replace the current process with the new process and shouldnt return
            exit(1);
        }
        else if (pid < 0)
        { // If there was an error forking
            perror("wish");
        }
        i++;
    }

    for (int j = 0; j < i; j++)
    {               // Loop through all the child processes
        wait(NULL); // Wait for all the child processes to finish to avoid zombie processes
    }
}

int handle_builtin_commands(char **args)
{

    printf("Handling builtin commands\n");

    if (strcmp(args[0], "exit") == 0)
    {
        printf("Exiting\n");
        if (args[1] != NULL)
        {
            fprintf(stderr, "wish: exit with arguments\n");
        }
        else
        {
            exit(0);
        }
        return 1;
    }
    else if (strcmp(args[0], "cd") == 0)
    {
        char *dir = args[1];
        char cwd[PATH_MAX];
        // char new_path[PATH_MAX]; // Buffer to hold the new path

        // LOGGING
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            printf("Current directory: %s\n", cwd);
        }
        else
        {
            perror("getcwd() error");
        }
        //--------------------------------------------------------------------------------
        printf("Changing directory to: %s\n", dir); // Debugging output

        if (args[1] == NULL || args[2] != NULL)
        {
            fprintf(stderr, "wish: cd requires exactly one argument\n");
        }
        else
        {
            if (chdir(dir) != 0)
            { // Change the directory to the one specified in the argument
                perror("wish");
            }
        }

        // LOGGING
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            printf("New directory: %s\n", cwd);
        }
        else
        {
            perror("getcwd() error");
        }
        //--------------------------------------------------------------------------------

        return 1;
    }
    else if (strcmp(args[0], "path") == 0)
    {
        // Handle path command
        // need to free up the path array space
        // iterate through args[1] to args[n] and update the path array.

        // Clear the existing path array
        for (int i = 0; path[i] != NULL; i++)
        {
            path[i] = NULL;
        }

        // Update the path array with the args
        int i = 1;
        int path_index = 0;
        while (args[i] != NULL)
        {
            if (path_index < MAX_TOKENS - 2) // Ensure we don't exceed the array size
            {
                path[path_index] = strdup(args[i]); // duplicates the string passed in (handles mem allocation for us)
                path_index++;
            }
            i++;
        }
        path[path_index] = NULL; // Null-terminate the array
        i = 0;
        while (path[i] != NULL)
        {
            printf("%s \n", path[i]);
            i++;
        }

        return 1;
    }
    return 0;
}

char *find_executable(char *command)
{
    char *executable = malloc(MAX_INPUT_SIZE); // Allocate memory for the executable
    for (int i = 0; path[i] != NULL; i++)
    {                                                                    // Loop through the path array
        snprintf(executable, MAX_INPUT_SIZE, "%s/%s", path[i], command); // Create the path to the executable
        if (access(executable, X_OK) == 0)
        {                                                 // Check if the executable exists and is executable
            printf("Executable found: %s\n", executable); // Print the path to the executable

            return executable; // Return the path to the executable
        }
    }
    free(executable); // Free the memory allocated for the executable
    return NULL;      // Return NULL if the executable is not found
}