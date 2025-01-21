// Add your code here
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    while (true)
    {
        printf("wish> ");
        size_t len = 0;
        char *line = NULL;

        size_t readCount = getline(&line, &len, stdin);
        if (readCount == -1)
        {
            perror("getsline");
        }

        // remove newline char and add null terminator
        if (line[readCount - 1] == '\n')
        {
            line[readCount - 1] = '\0';
        }

        if (strcmp(line, "exit") == 0)
        {
            exit(0);
        }
    }
}

// 3.
// Use getline() to parse inputs
// call exit(0) when EOF marker is hit -> read man page
// use strsep() to parse input line
//  fork(),exec(), wait()/waitpid() -> see man page
//  must use execv() -> if sucessful it will not return. do not use system()
// 4.
// must use access() to check if file exists in a directory and is executable. when ls is typed  and path is /bin and /usr/bin try:
// access("/bin/ls", X_OK); if that failes try access("/usr/bin/ls", X_OK); if that fails its an error
