# Mini-Shell

==Description==
This program is a mini shell program in C for a Linux operating system. It receives commands from the user and executes them. The program can also handle variable assignment, many commands in one line, commands that uses pipes, writing into files, run backround procees, stop commands that run at forground and also make them continue in bacground.
The shell support commands like : ls, echo, cat etc. 
the program uses strtok_r to tokenize the input and execute the command using execvp.

Program DATABASE:
struct database that contain sub databases, the sub databases are:
1. vars struct - represents a variable with a name and a value that are inputs from the user.
2. node struct - contains a vars data field and a pointer to the next node. The nodes will create a linked list of all the variable that the user want to save.

functions:
- findCharFirstInd -returns the index of the first occurrence of a given character in a string.
- dollar – looking for a dollar in a string, if it found, the function replace it with the value of the variable
- insertToLinkedLIst – insert a new node to the head of the list.
- findNode – search for a node by his var name.
- freeList – free the memory that allocated to the linked list. 
- freeArgv - if there was a dynamic memory allocation in the argv array - 
the memory will free in this function.
- findCharFromIndex - returns the index of the first occurrence, after a given index, of a given character in a string .
- sig_child - a function that called every time that there is a SIGCHLD - the function will wait for any child process - this will prevent zombies process without wait.
- sig_stop - a function that called when there is SIGTSTP - the user enter ctrl Z, it make the last process that runs forground to stop.

==Program Files==
ex2.c – contains all the program 

==How to compile?==
compile: gcc ex2.c -o ex2
run: ./ex2

==Input:==
command in bash language

==Output:==
the commands results

