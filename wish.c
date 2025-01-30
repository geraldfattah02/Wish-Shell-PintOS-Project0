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
const char *DELIM = " \r\n\a\t"; // Delimiters for strtok, this will split the string by

char *path[MAX_TOKENS] = {"/bin", NULL};

pid_t spawn_child(char **args, char *ouput_filename);
void execute_command();
void execute_parallel_commands(char **commands);
int handle_builtin_commands(char **args);
void execute_line(char *line);
char *find_executable(char *command);

int main(int argc, char *argv[])
{
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
        char *line = NULL;   // Pointer to the buffer that will hold the input line, initially set to NULL for dynamic allocation
        size_t len = 0;      // Variable to hold the size of the buffer, initially set to 0
        ssize_t nread;       // Variable to hold the number of characters read by getline()

        if (input == stdin)
        {
            printf("wish> "); // print the prompt
        }

        nread = getline(&line, &len, input); // read the line from the input into, nread holds the number of characters read
        if (nread == -1)
        { // if -1 that means the end of the file has been reached or there's nothing in the file
            break;
        }
        
        execute_line(line); // parse and execute the command
        free(line);              // free the memory allocated for the line buffer
    }

    if (input != stdin)
    {
        fclose(input); // close the input file if it's not standard input
    }

    return 0;
}

void execute_line(char *line)
{
    //printf("%s\n", line);
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
    
    //printf("%s\n", commands[0]);
    //printf("%d\n", i);
    // Spawn child processes for each command
    for (int j = 0; j < i; j++)
    {
        //printf("%d\n", j);
        execute_command(commands[j]);
    }

    for (int j = 0; j < i; j++)
    {
        wait(NULL);
    }
}

void execute_command(char *full_command)
{
    //printf("%s\n", full_command);
    //   ls -al   >   out.txt
    // ^command    ^redirect_file
    char *command = strsep(&full_command, ">");
    char *redirect_file = full_command;
    
    if (redirect_file != NULL) // ">" was found in the command
    {
        //printf("Redirect: %s\n", redirect_file);
        if (strchr(redirect_file, '>') != NULL)
        { 
            // another ">" exists
            fprintf(stderr, "An error has occurred\n");
            return;
        }

        char *filename = strtok(redirect_file, DELIM);
        if (filename == NULL)
        {
            // No file name after '>'
            fprintf(stderr, "An error has occurred\n");
            return;
        }

        if (strtok(NULL, DELIM) != NULL)
        {   
            // Multiple file(s) after '>'
            fprintf(stderr, "An error has occurred\n");
            return;
        }

        redirect_file = filename;
    }

    //printf("Command: %s\n", command);

    char *args[MAX_TOKENS];
    int i = 0;
    char *arg = strtok(command, DELIM);
    while (arg != NULL)
    {
        args[i++] = arg;
        //printf("Arg: %s\n", arg);
        arg = strtok(NULL, DELIM);
    }
    args[i] = NULL;

    if (i == 0 && redirect_file != NULL)
    {
        // No program name before '>'
        fprintf(stderr, "An error has occurred\n");
        return;
    }
    else if (i == 0)
    {
        // Empty command, so return
        return;
    }

    //printf("Args: %s\n", args[0]);

    spawn_child(args, redirect_file);
}

pid_t spawn_child(char **args, char *ouput_filename)
{
    if (handle_builtin_commands(args)) // Don't spawn for built-in command
    {
        return 0;
    }
    
    // printf("Executing\n");

    // Process ID for the child process and the parent process
    pid_t pid = fork();

    if (pid == 0) // Child Process
    {
        char *executable = find_executable(args[0]);
        if (executable == NULL)
        { // If the executable is not found
            fprintf(stderr, "An error has occurred\n");
            exit(1);
        }

        if (ouput_filename)
        {
            int fd = open(ouput_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0)
            {
                fprintf(stderr, "An error has occurred\n");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
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

    return pid;
}

int handle_builtin_commands(char **args)
{
    if (strcmp(args[0], "exit") == 0)
    {
        // printf("Exiting\n");
        if (args[1] != NULL)
        {
            fprintf(stderr, "An error has occurred\n");
            return 1;
        }

        exit(0);
    }
    else if (strcmp(args[0], "cd") == 0)
    {
        if (args[1] == NULL || args[2] != NULL)
        {
            fprintf(stderr, "An error has occurred\n");
            return 1;
        }
        else
        {
            int err = chdir(args[1]);
            if (err != 0)
            {
                perror("An error has occurred\n");
            }
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