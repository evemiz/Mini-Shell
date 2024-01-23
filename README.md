# Mini-Shell

==Description==
This program is a mini shell program in C for a Linux operating system. It receives commands from the user and executes them. The program can also handle variable assignment, multiple commands in one line, commands that use pipes, writing into files, running background processes, stopping commands that run in the foreground, and also making them continue in the background. The shell supports commands like `ls`, `echo`, `cat`, etc. The program uses `strtok_r` to tokenize the input and executes the command using `execvp`.

**Program Database:** 

- `vars` struct: Represents a variable with a name and a value inputted by the user.
- `node` struct: Contains a `vars` data field and a pointer to the next node. The nodes create a linked list of all the variables that the user wants to save.

**Functions:**

- `findCharFirstInd`: Returns the index of the first occurrence of a given character in a string.
- `dollar`: Looks for a dollar sign in a string; if found, the function replaces it with the value of the variable.
- `insertToLinkedList`: Inserts a new node at the head of the list.
- `findNode`: Searches for a node by its variable name.
- `freeList`: Frees the memory allocated to the linked list.
- `freeArgv`: If there was dynamic memory allocation in the `argv` array, the memory will be freed in this function.
- `findCharFromIndex`: Returns the index of the first occurrence, after a given index, of a given character in a string.
- `sig_child`: A function called every time there is a `SIGCHLD`; the function waits for any child process to prevent zombie processes without waiting.
- `sig_stop`: A function called when there is `SIGTSTP` (user enters Ctrl Z); it makes the last foreground process stop.

==Program Files==
- ex2.c: Contains the entire program.

==How to Compile?==
Compile: gcc ex2.c -o ex2
Run: ./ex2

==Input==
Commands in bash language.

==Output==
The command results.


