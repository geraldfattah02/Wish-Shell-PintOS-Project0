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

void execute_command(char **args);
void execute_parallel_commands(char **commands);
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
            fprintf(stderr, "An error has occurred\n");
            exit(1);
        }
    }
    if (argc > 2)
    {
        fprintf(stderr, "An error has occurred\n");
        exit(1);
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
        if (arg == NULL)
        {
            // no string input (only whitespace)
            return;
        }
        if (arg[0] == '>')
        {
            fprintf(stderr, "An error has occurred\n");
            return;
        }
        // printf("command: %s, arg: %s\n", commands[0], arg);

        while (arg != NULL)
        {
            // check if > is stand alone.
            if (strcmp(arg, ">") == 0)
            {
                arg = strtok(NULL, DELIM);
                if (arg == NULL)
                {
                    fprintf(stderr, "An error has occurred\n");
                    return;
                }
                output_file = arg;
                if (strtok(NULL, DELIM) != NULL)
                {
                    fprintf(stderr, "An error has occurred\n");
                    return;
                }
            }
            else
            {
                // checking edge cases of where > might be
                char *redirect_pos = strchr(arg, '>'); // finds the index of >
                if (redirect_pos != NULL)
                {
                    //> is attached to the beginning (`>out.txt`)
                    if (redirect_pos == arg)
                    {
                        output_file = arg + 1;
                    }
                    else
                    {
                        //> is in the middle of the string (`abc>out.txt`)
                        *redirect_pos = '\0';           // split at >
                        args[j++] = arg;                // store first part as normal arg
                        output_file = redirect_pos + 1; // store second part as output file
                    }
                    if (output_file == NULL || *output_file == '\0' || strtok(NULL, DELIM) != NULL)
                    {
                        fprintf(stderr, "An error has occurred\n");
                        return;
                    }
                }
                else
                {
                    args[j++] = arg;
                }
            }

            arg = strtok(NULL, DELIM);
        }
        args[j] = NULL;

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
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644); //write only, create if not present, truncate, and give permissionss
            if (fd < 0)
            {
                perror("An error has occurred\n");
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
    // printf("Executing\n");

    pid_t pid; // Process ID for the child process and the parent process
    int status;

    pid = fork();
    if (pid == 0)
    { // Child processf
        char *executable = find_executable(args[0]);
        if (executable == NULL)
        { // If the executable is not found
            fprintf(stderr, "An error has occurred\n");
            exit(1);
        }
        execv(executable, args);           // Execute the command with the arguments
        perror("An error has occurred\n"); // If execv fails, print an error message, this should never be reached because execv will replace the current process with the new process and shouldnt return
        exit(1);
    }
    else if (pid < 0)
    {
        // Error forking
        perror("An error has occurred\n");
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
            char *output_file = NULL;
            
            arg = strtok(commands[i], DELIM); // Split the command by " "
            if (arg ==  NULL)
            {
                exit(1);
            }
            // Parse command and check for redirection
            while (arg != NULL) 
            {
                // Check for standalone >
                if (strcmp(arg, ">") == 0) {
                    arg = strtok(NULL, DELIM);
                    if (arg == NULL) {
                        fprintf(stderr, "An error has occurred\n");
                        exit(1);
                    }
                    output_file = arg;
                    if (strtok(NULL, DELIM) != NULL) {
                        fprintf(stderr, "An error has occurred\n");
                        exit(1);
                    }
                    break;
                }
                else {
                    // Check for attached > (like command>file.txt)
                    char *redirect_pos = strchr(arg, '>');
                    if (redirect_pos != NULL) {
                        if (redirect_pos == arg) { // >file.txt case
                            output_file = arg + 1;
                        }
                        else {
                            *redirect_pos = '\0';
                            args[j++] = arg;
                            output_file = redirect_pos + 1;
                        }
                        if (output_file == NULL || *output_file == '\0') {
                            fprintf(stderr, "An error has occurred\n");
                            exit(1);
                        }
                        break;
                    }
                    else {
                        args[j++] = arg;
                    }
                }
                arg = strtok(NULL, DELIM);
            }
            args[j] = NULL; // Set the last element of the array to NULL so we know where the end is

            if (args[0] == NULL)
            { // If there are no arguments, return
                exit(1);
            }
        
            // Handle redirection if > operator was found
            if (output_file) {
                int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    fprintf(stderr, "An error has occurred\n");
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }

            char *executable = find_executable(args[0]);
            if (executable == NULL)
            { // If the executable is not found
                fprintf(stderr, "An error has occurred\n");
                exit(1);
            }

            execv(executable, args); // Execute the command with the arguments for example ls -l would be ["ls", "-l"] so args[0] = "ls" and args[1] = "-l" or echo hello world
            fprintf(stderr, "An error has occurred\n");
            // If execv fails, print an error message, this should never be reached because execv will replace the current process with the new process and shouldnt return
            exit(1);
        }
        else if (pid < 0)
        { // If there was an error forking
            fprintf(stderr, "An error has occurred\n");
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
    if (strcmp(args[0], "exit") == 0)
    {
        // printf("Exiting\n");
        if (args[1] != NULL)
        {
            fprintf(stderr, "An error has occurred\n");
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

        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
            perror("An error has occurred\n");
        }
        if (args[1] == NULL || args[2] != NULL)
        {
            fprintf(stderr, "An error has occurred\n");
        }
        else if (chdir(dir) != 0)
        {
            perror("An error has occurred\n");
        }

        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
            perror("An error has occurred\n");
        }
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
        { // Check if the executable exists and is executable
            return executable;
        }
    }
    free(executable); // Free the memory allocated for the executable
    return NULL;      // Return NULL if the executable is not found
}