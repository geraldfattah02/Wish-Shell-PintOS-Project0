# Project 0, Part 2: Wish Shell

## Group Members: Gerald Fattah, Veeren Patel, James Bundgard

# 1. Unix Shell

In this project, you’ll build a simple Unix shell. The shell is the heart of the command-line interface, and thus is central to the Unix/C programming environment. Mastering use of the shell is necessary to become proficient in this world; knowing how the shell itself is built is the focus of this project.

**Objectives:**

- Further familiarize yourself with the Linux programming environment
- Learn how processes are created, destroyed, and managed
- Gain exposure to the necessary functionality in shells

In this assignment, you will implement a Command Line Interpreter (CLI) or, as it is more commonly known, a Shell. The shell should operate in this basic way: when you type in a command (in response to its prompt), the shell creates a child process that executes the command you entered and then prompts for more user input when it has finished.

The shells you implement will be similar to, but simpler than, the one you run every day in Unix. If you don’t know what shell you are running, it’s probably Bash. One thing you should do on your own time is learn more about your shell, by reading the man pages or other online materials.

# 2. Program Specifications - Basic Shell

Your basic shell, called wish, is basically an interactive loop. It prints a prompt and waits for some user input:

```
wish>
```

When the user inputs something and hits enter, the shell parses the input, executes the command specified on that line of input, and waits for the command to finish. This is repeated until the user types _exit_. The name of your final executable should be _wish_.

The shell can be invoked with either no arguments or a single argument; anything else is an error. Here is the no-argument way:

```
prompt> ./wish
wish>
```

At this point, _wish_ is running, and ready to accept commands. Type away!

The mode above is called _‘interactive mode’_, and allows the user to type commands directly. The shell also supports a _‘batch mode’_, which instead reads input from a batch file and executes commands from therein. Here is how you run the shell with a batch file named _batch.txt_:

```
prompt> ./wish batch.txt
```

One difference between batch and interactive modes is that, in interactive mode, a prompt is printed (wish> ). In batch mode, no prompt should be printed.

You should structure your shell such that it creates a process for each new command (the exception are _built-in commands_, discussed below). Your basic shell should be able to parse a command and run the program corresponding to the command. For example, if the user types

```bash
ls -la /tmp
```

your shell should run the program _/bin/ls_ with the given arguments _-la_ and _/tmp_ (how does the shell know to run _/bin/ls_? It’s something called the shell path; more on this in the next section).

# 3. Structure

The shell is very simple (conceptually): it runs in a while loop, repeatedly asking for input to tell it what command to execute. It then executes that command. The loop continues indefinitely, until the user types the built-in command **_exit_**, at which point it exits. That’s it!

For reading lines of input, you should use **_getline()_**. This allows you to obtain arbitrarily long input lines with ease. Generally, the shell will be run in _interactive mode_, where the user types a command (one at a time) and the shell acts on it. However, your shell will also support _batch mode_, in which the shell is given an input file of commands; in this case, the shell should not read user input (from _stdin_) but rather from this file to get the commands to execute.

In either mode, if you hit the end-of-file marker (EOF), you should call **_exit(0)_** and exit gracefully.

**To parse the input line** into constituent pieces, you might want to use **_strsep()_**. Read the man page (carefully) for more details.

**To execute commands**, look into **_fork()_**, **_exec()_**, and **_wait()_**/**_waitpid()_**. See the man pages for these functions, and also read the relevant book chapter for a brief overview.

You will note that there are a variety of commands in the exec family; for this project, you must use **_execv()_**. You should not use the **_system()_** library function call to run a command. Remember that if **_execv()_** is successful, it will not return; if it does return, there was an error (e.g., the command does not exist). The most challenging part is getting the arguments correctly specified.

# 4. Paths

In our example above, the user typed _ls_ but the shell knew to execute the program _/bin/ls_. How does your shell know this?

It turns out that the user must specify a _path variable_ to describe the set of directories to search for executables; the set of directories that comprise the path are sometimes called the _search path_ of the shell. The path variable contains the list of all directories to search, in order, when the user types a command.

---

🛈 _Note that the shell itself does not implement_ ls _or other commands (except built-ins). All it does is find
those executables in one of the directories specified by ‘path’ and create a new process to run them._

---

To check if a particular file exists in a directory and is executable, consider the **_access()_** system call. For example, when the user types _ls_, and path is set to include both _/bin_ and _/usr/bin_, try:

```c
access("/bin/ls", X_OK);
```

If that fails, try:

```c
access("/usr/bin/ls", X_OK);
```

If that fails too, it is an error.

Your initial shell path should contain one directory: _/bin_

---

🛈 *Note that most shells allow you to specify a binary specifically without using a search path, using either **absolute paths** *or* **relative paths**. For example, a user could type the absolute path* /bin/ls _and execute the_ ls *binary without a search path being needed. A user could also specify a relative path which starts with the current working directory and specifies the executable directly (*e.g. ./main)_. In this project, you do not have to worry about these features._

---

# 5. Built-In Commands

Whenever your shell accepts a command, it should check whether the command is a built-in command or not. If it is, it should not be executed like other programs. Instead, your shell will invoke your implementation of the built-in command. For example, to implement the exit built-in command, you simply call **_exit(0)_** in your wish source code, which then will exit the shell.

In this project, you should implement **_exit_**, **_cd_**, and **_path_** as built-in commands.

- **_exit_**: When the user types **_exit_**, your shell should simply call the **_exit()_** system call with 0 as a parameter. It is an error to pass any arguments to exit
- **\*\***cd**\*\***: The ********\*\*********cd********\*\********* command always takes one argument (0 or >1 args should be signaled as an error). To change directories, use the **_chdir()_** system call with the argument supplied by the user; if **_chdir()_** fails, that is also an error
- ****\*\*\*\*****path****\*\*\*\*****: The **_path_** command takes 0 or more arguments, with each argument separated by whitespace from the others. A typical usage would be like this:
  ```
  wish> path /bin /usr/bin
  ```
  which would add _/bin_ and _/usr/bin_ to the search path of the shell. If the user sets path to be empty, then the shell should not be able to run any programs (except built-in commands). The path command always overwrites the old path with the newly specified path

# 6. Redirection

Many times, a shell user prefers to send the output of a program to a file rather than to the screen. Usually, a shell provides this nice feature with the ‘>’ character. Formally this is named as redirection of standard output. To make your shell users happy, your shell should also include this feature, but with a slight twist (explained below).

For example, if a user types

```bash
ls -la /tmp > output
```

nothing should be printed on the screen. Instead, the standard output of the _ls_ program should be rerouted to the file named _output_. In addition, the standard error output of the program should be rerouted to the _output_ file (the twist is that this is a little different than standard redirection).

If the _output_ file exists before you run your program, you should simple overwrite it (after truncating it).

The exact format of redirection is a command (and possibly some arguments) followed by the redirection symbol followed by a filename. **Multiple redirection operators or multiple files to the right of the redirection sign are errors.**

---

🛈 _Note: don’t worry about redirection for built-in commands (e.g., we will not test what happens when you type path /bin > file)._

---

# 7. Parallel Commands

Your shell will also allow the user to launch parallel commands. This is accomplished with the ampersand operator as follows:

```
wish> cmd1 & cmd2 args1 args2 & cmd3 args1
```

In this case, instead of running _cmd1_ and then waiting for it to finish, your shell should run _cmd1_, _cmd2_, and _cmd3_ (each with whatever arguments the user has passed to it) in parallel, before waiting for any of them to complete.

Then, after starting all such processes, you must make sure to use **_wait()_** (or **_waitpid()_**) to wait for them to complete. After all processes are done, return control to the user as usual (or, if in batch mode, move on to the next line).

Handle parallel commands which are not built-in (not implemented by you). For example, you will have to handle the following:

```bash
ls & ls
ls & echo Hello
```

You need not handle when one (or all) of the parallel commands are built-in (implemented by you: ****\*\*\*****exit, cd,****\*\*\***** or **\***path**\***)

# 8. Program Errors

**The one and only error message you need to print whenever you encounter an error of any type:**

```c
char error_message[30] = "An error has occurred\n";
write(STDERR_FILENO, error_message, strlen(error_message));
```

The error message should be printed to _stderr_ (standard error), as shown above.

After most errors, your shell simply continues processing after printing the one and only error message. However, if the shell is invoked with more than one file, or if the shell is passed a bad batch file, it should exit by calling **_exit(1)_**.

There is a difference between errors that your shell catches and those that the program catches. Your shell should catch all the syntax errors specified in this project page. If the syntax of the command looks perfect, you simply run the specified program. If there are any program-related errors (e.g., invalid arguments to _ls_ when you run it, for example), the shell does not have to worry about that (rather, the program will print its own error messages and exit).

# 9. Hints

Remember to get the **basic functionality** of your shell working before worrying about all of the error conditions and end cases. For example, first get a single command running (probably first a command with no arguments, such as _ls_).

Next, add built-in commands. Then, try working on redirection. Finally, think about parallel commands. Each of these requires a little more effort on parsing, but each should not be too hard to implement.

At some point, you should make sure your code is robust to white space of various kinds, including spaces and tabs. In general, the user should be able to put variable amounts of white space before and after commands, arguments, and various operators; however, the operators (redirection and parallel commands) do not require whitespace.

Check the return codes of all system calls from the very beginning of your work. This will often catch errors in how you are invoking these new system calls. It’s also just good programming sense.

Beat up your own code! You are the best (and in this case, the only) tester of this code. Throw lots of different inputs at it and make sure the shell behaves well. Good code comes through testing; you must run many different tests to make sure things work as desired. Don’t be gentle – other users certainly won’t be.

Finally, keep versions of your code. It is highly recommended to use a source control system such as _git_. This is oftentimes an industry-standard skill to be able to utilize source control systems as a programmer. However, at the very least, when you get a piece of functionality working, make a copy of your .c file (perhaps a subdirectory with a version number, such as v1, v2, etc.). By keeping older, working versions around, you can comfortably work on adding new functionality, safe in the knowledge you can always go back to an older, working version if need be.

# 10. Running Tests

- Add your code to the ********\*\*********wish.c********\*\********* file
- Use the **********\***********Makefile**********\*********** to compile
- For running tests, run the corresponding test script. For example:
  ```
  prompt> ./test-wish.sh
  ```

# Appendix - Test Options & Turn-in Instructions

The _run-tests.sh_ script is called by various testers to do the work of testing. Each test is actually fairly simple: it is a comparison of standard output and standard error, as per the program specification.

In any given program specification directory, there exists a specific _tests/_ directory which holds the expected return code, standard output, and standard error in files called _n.rc_, _n.out_, and _n.err_ (respectively) for each test _n_. The testing framework just starts at 1 and keeps incrementing tests until it can't find any more or encounters a failure. Thus, adding new tests is easy; just add the relevant files to the tests directory at the lowest available number.

The files needed to describe a test number _n_ are:

- _n.rc_: the return code the program should return (usually 0 or 1)
- _n.out_: the standard output expected from the test
- _n.err_: the standard error expected from the test
- _n.run_: how to run the test (which arguments it needs, etc.)
- _n.desc_: a short text description of the test
- _n.pre_ (optional): code to run before the test, to set something up
- _n.post_ (optional): code to run after the test, to clean something up

There is also a single file called _pre_ which gets run once at the beginning of testing; this is often used to do a more complex build of a code base, for example. To prevent repeated time-wasting, pre-test activity, suppress this with the -s flag (as described below).

In most cases, a wrapper script is used to call **_run-tests.sh_** to do the necessary work.

The options for **_run-tests.sh_** include:

- -h (the help message)
- -v (verbose: print each test description)
- -t _n_ (run only test number **n**)
- -c (continue even after a test fails)
- -d (run tests not from **\*\***tests/**\*\*** directory but from this directory instead)
- -s (suppress running the one-time set of commands in \***\*pre\*\*** file)

To turn in your assignment, submit a zip file containing only your solution `.c` or `.patch` file as indicated on Canvas. Name the file exactly as shown on Canvas. Do not place the files inside a folder and then zip the folder, as this will result in a folder inside another folder when unzipped. Instead, zip with the following command from your terminal:

```bash
zip <zip-file-name> <your-solution-file>
```

For example, for Project 0, you would zip up your solution with the following:

```bash
zip YOUR_EID.zip wish.c pintos.patch
```

When you unzip your submission, your files should be at the top level of the directory and there should not be any nested folders.
