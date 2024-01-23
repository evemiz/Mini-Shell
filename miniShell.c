#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>

#define PATH_MAX 1024

pid_t lastPid=-1;
pid_t pidLastStop=-1;


//variable struct
typedef struct  {
    char name[100];
    char value[100];
}vars;

//node struct
struct node{
    vars data;
    struct node *next;
};

//functions
void freeList(struct node *head);
void insertToLinkedList(struct node **head, vars newData);
struct node* findNode(struct node* head, const char* valueName);
int findFirstOccurrenceIndex(char *str, char targetChar);
int findFirstOccurrenceIndexFrom(char *str, char targetChar, int startIndex);
char *getVariableValue(struct node *head, const char *variableName, int listSize, char **argv, int argc);
void freeArguments(char** argv, const int* shouldFree);
void sig_child(int sig);
void sig_stop(int sig);

    int main() {

        //Variables
        struct node *head = NULL;

        char currentCommandToExecute[511];
        char savedCommands[510];
        char savedCommandsFromPipe[510];
        char saveFileName[510];

        bool hasMoreCommandsAfterPipe = false;
        bool hasMoreCommands = false;
        bool isLeftmostCommand = false;
        bool isMiddleCommand = false;
        bool isRightmostCommand = false;
        bool runInBackground = false;
        bool isOutputRedirected = false;

        int **pipe_fd;
        int commandCounter = 0;
        int enterCounter = 0;
        int numberOfPipes = 0;
        int currentPipeNumber = -1;
        int variableListSize = 0;
        int flagsCounter = 0;

        signal(SIGTSTP, sig_stop);
        signal(SIGCHLD, sig_child);

        // Get the current working directory
        char currentPath[PATH_MAX];
        if ((getcwd(currentPath, sizeof(currentPath))) == NULL) {
            perror("getcwd");
            return 1;
        }

        //Main loop to keep the program running continuously
        while (1) {

            // Check if there are more commands to process
            if (hasMoreCommandsAfterPipe || hasMoreCommands) {
                if (isRightmostCommand) {
                    // Copy characters from savedCommandsFromPipe to currentCommandToExecute
                    strncpy(currentCommandToExecute, savedCommandsFromPipe, sizeof(currentCommandToExecute) - 1);
                    currentCommandToExecute[sizeof(currentCommandToExecute) - 1] = '\0';
                    savedCommandsFromPipe[0] = '\0';
                    isMiddleCommand = false;
                } else {
                    // Copy characters from savedCommands to currentCommandToExecute
                    strncpy(currentCommandToExecute, savedCommands, sizeof(currentCommandToExecute) - 1);
                    currentCommandToExecute[sizeof(currentCommandToExecute) - 1] = '\0';
                    hasMoreCommands = false;
                }
            }
                // Input from the user
            else {
                printf("\033[35m#cmd:%s\033[0m ",currentPath);
                fgets(currentCommandToExecute, sizeof(currentCommandToExecute), stdin);
            }

            // Initializing
            int commandLength = (int) strlen(currentCommandToExecute);
            if (currentCommandToExecute[commandLength - 1] == '\n') {
                currentCommandToExecute[commandLength - 1] = '\0';
            }
            char *arguments[11] = {NULL};  // The arguments array
            int memoryCleanupFlags[11] = {-1}; // Array to track whether to free memory for each argument

            // Handle consecutive 'enter' key presses: if entered three times in a row, exit; otherwise, reset counter
            if (currentCommandToExecute[0] == '\0') {
                if (enterCounter == 2) {
                    freeList(head);
                    exit(0);
                }
                enterCounter++;
                continue;
            }
            enterCounter = 0;

            // Check for multiple commands separated by semicolons
            int semicolonIndex = findFirstOccurrenceIndex(currentCommandToExecute, ';');
            int quoteStartIndex = findFirstOccurrenceIndex(currentCommandToExecute, '"');
            int quoteEndIndex = (quoteStartIndex == -1) ? -1 : findFirstOccurrenceIndexFrom(currentCommandToExecute,
                                                                                            '"', quoteStartIndex + 1);
            while (semicolonIndex != -1) {
                // Ensure the semicolon is not within a pair of double quotes
                if (!(quoteStartIndex < semicolonIndex && semicolonIndex < quoteEndIndex)) {
                    int copyIndex = semicolonIndex + 1;
                    // Copy the remaining part of the command after the semicolon to 'save'
                    for (int i = 0; copyIndex <= strlen(currentCommandToExecute); ++i) {
                        savedCommands[i] = currentCommandToExecute[copyIndex++];
                    }
                    hasMoreCommands = true;
                    //howManyComm = 0;
                    currentCommandToExecute[semicolonIndex] = '\0';
                    break;
                }
                semicolonIndex = findFirstOccurrenceIndexFrom(currentCommandToExecute, ';', semicolonIndex + 1);
            }

            // Check for background execution indicator '&' in the command
            int backgroundIndicatorIndex = findFirstOccurrenceIndex(currentCommandToExecute, '&');
            if (backgroundIndicatorIndex != -1) {
                runInBackground = true;
                currentCommandToExecute[backgroundIndicatorIndex] = '\0';
            }

            // Check if output should be redirected to a file
            int outputRedirectIndex = findFirstOccurrenceIndex(currentCommandToExecute, '>');
            if (outputRedirectIndex != -1) {
                int fileNameStartIndex = outputRedirectIndex + 1;
                // Skip whitespace characters after '>'
                while (currentCommandToExecute[fileNameStartIndex] == ' ') {
                    fileNameStartIndex++;
                }
                // Extract the filename after '>'
                int i = 0;
                while (currentCommandToExecute[fileNameStartIndex] != ' ' && currentCommandToExecute[fileNameStartIndex] != '\0') {
                    saveFileName[i] = currentCommandToExecute[fileNameStartIndex++];
                    i++;
                }
                saveFileName[i] = '\0';
                currentCommandToExecute[outputRedirectIndex] = '\0';
                isOutputRedirected = true;
            }

            // Check for the presence of a pipe '|'
            int pipeIndex = findFirstOccurrenceIndex(currentCommandToExecute, '|');
            quoteStartIndex = findFirstOccurrenceIndex(currentCommandToExecute, '"');
            quoteEndIndex = (quoteStartIndex != -1) ? findFirstOccurrenceIndexFrom(currentCommandToExecute, '"',
                                                                                   quoteStartIndex + 1) : -1;
            // Adjust pipeIndex if it is within a pair of double quotes
            if (quoteStartIndex < pipeIndex && pipeIndex < quoteEndIndex) {
                pipeIndex = findFirstOccurrenceIndexFrom(currentCommandToExecute, '|', quoteEndIndex + 1);
            }
            if (pipeIndex != -1) {
                if (numberOfPipes == 0) {
                    // Count the total number of pipes in the command
                    numberOfPipes = 0;
                    for (int i = pipeIndex; i < strlen(currentCommandToExecute); ++i) {
                        if (currentCommandToExecute[i] == '|') {
                            numberOfPipes++;
                        }
                    }
                    // Allocate memory for pipe file descriptors
                    pipe_fd = malloc(numberOfPipes * sizeof(int *));
                    for (int i = 0; i < numberOfPipes; ++i) {
                        pipe_fd[i] = malloc(2 * sizeof(int));
                    }
                    isMiddleCommand = false;
                } else {
                    isMiddleCommand = true;
                }
                currentPipeNumber++;
                isLeftmostCommand = true;
                isRightmostCommand = false;
                // Extract the command after the pipe '|'
                int j = pipeIndex + 1;
                for (int i = 0; j <= strlen(currentCommandToExecute); ++i) {
                    savedCommandsFromPipe[i] = currentCommandToExecute[j++];
                }
                hasMoreCommandsAfterPipe = true;
                currentCommandToExecute[pipeIndex] = '\0';
            }

            // Check for variable assignment
            int equalSignIndex = findFirstOccurrenceIndex(currentCommandToExecute, '=');
            if (equalSignIndex != -1) {
                // Extract the name of the variable
                char *variableName = strtok(currentCommandToExecute, "=");
                vars newVariable;
                int index = 0;
                for (int i = 0; variableName[i] != '\0'; i++) {
                    if (variableName[i] != ' ') {
                        newVariable.name[index] = variableName[i];
                        index++;
                    }
                }
                newVariable.name[index] = '\0';
                // Extract the value of the variable
                char *variableValue = strtok(NULL, " ");
                index = 0;
                for (int i = 0; variableValue[i] != '\0'; i++) {
                    newVariable.value[index] = variableValue[i];
                    index++;
                }
                newVariable.value[index] = '\0';
                char *extraToken = strtok(NULL, "\0");
                if (extraToken != NULL) {
                    printf("%s: command not found\n", extraToken);
                    continue;
                }
                // Insert the new variable into the array
                variableListSize++;
                insertToLinkedList(&head, newVariable);
            }

                // If it is not a variable assignment, treat it as a command to be executed
            else {
                // Tokenize the command using space as delimiter
                char *rest;
                char *token = strtok_r(currentCommandToExecute, " ", &rest);
                int argc = 0;
                bool isFirstCharDollar = false;
                // Check if the first token does not start with '$'
                if (findFirstOccurrenceIndex(token, '$') != 0) {
                    arguments[0] = token;
                    argc = 1;  // Arguments for the current command counter
                } else {
                    isFirstCharDollar = true;
                }

                bool invalidInput = false;
                bool varNotFound = false;

                while (token != NULL) {

                    // Check if the input is not legal (more than 10 arguments)
                    if (argc > 10 && arguments[10] != NULL) {
                        invalidInput = true;
                        break;
                    }

                    // Check if this is a variable
                    if ((rest != NULL && rest[0] == '$') || isFirstCharDollar) {
                        int charIndex = -1;
                        if (!isFirstCharDollar)
                            charIndex = findFirstOccurrenceIndex(rest, '"');
                        // Handle cases where variable value is enclosed in double quotes
                        if (!isFirstCharDollar && (rest != NULL && rest[0] == '$')) {
                            if (charIndex != -1) {
                                token = strtok_r(NULL, "\"", &rest);
                                token[charIndex] = '\0';
                            } else {
                                token = strtok_r(NULL, " ", &rest);
                            }
                        }
                        isFirstCharDollar = false;

                        int charInd = findFirstOccurrenceIndex(token, ';');
                        if (charInd != -1) {
                            token[charInd] = '\0';
                        }
                        char *dollarResult = getVariableValue(head, token + 1, variableListSize, arguments, argc);

                        if (dollarResult == NULL) {
                            varNotFound = true;
                            break;
                        }

                        memoryCleanupFlags[flagsCounter] = argc;
                        flagsCounter++;

                        // Handle cases where variable value is enclosed in double quotes
                        if (charIndex != -1) {
                             char* tempResult= (char *) realloc(dollarResult, strlen(dollarResult) + strlen(rest) + 1);
                            if (tempResult != NULL) {
                                dollarResult = tempResult;
                            } else {
                                printf("An error occurred.\n");
                                exit(EXIT_FAILURE);
                            }
                            strcat(dollarResult, rest);
                            dollarResult[findFirstOccurrenceIndex(dollarResult, '"')] = '\0';
                            token = strtok_r(NULL, "\"", &rest);
                            token = strtok_r(NULL, " ", &rest);
                        }

                        arguments[argc] = dollarResult;
                        argc++;

                        // Break if a semicolon is encountered
                        if (charInd != -1) {
                            break;
                        }
                    }

                    // If this is a sentence between ""
                    else if (rest != NULL && rest[0] == '"') {
                        token = strtok_r(NULL, "\"", &rest);
                        int dollarInd = findFirstOccurrenceIndex(token, '$');

                        if (dollarInd == -1) {
                            // No variable substitution needed, directly use the content between quotes
                            arguments[argc] = token;
                        } else {
                            // Variable substitution needed
                            char *newTemp = strtok_r(token, " ", &rest);
                            char* dollarResult = getVariableValue(head, newTemp + 1, variableListSize, arguments, argc);

                            if (dollarResult == NULL) {
                                varNotFound = true;
                                break;
                            }

                            memoryCleanupFlags[flagsCounter] = argc;
                            flagsCounter++;
                            arguments[argc] = dollarResult;
                        }

                        argc++;
                    }

                    else {
                        token = strtok_r(NULL, " ", &rest);
                        arguments[argc] = token;
                        argc++;
                    }
                }

                // If the input is a variable that doesn't exist
                if (varNotFound) {
                    varNotFound = false;
                }

                // Calculate the total number of arguments in argv
                int argvInd = 0;
                argc = 0;
                while (arguments[argvInd] != NULL) {
                    argc++;
                    argvInd++;
                }

                // If there are more than 10 arguments
                if (invalidInput) {
                    // Free memory and print an error message
                    freeArguments(arguments, memoryCleanupFlags);
                    printf("Invalid input\n");
                    invalidInput = false;
                    continue;
                }

                // If the input is a 'cd' command, which is not supported
                if (strcmp(arguments[0], "cd") == 0) {
                    // Replace 'cd' with 'echo' and provide a message
                    arguments[0] = "echo";
                    arguments[1] = "cd not supported";
                    arguments[2] = NULL;
                    // Adjust counters to skip execution
                    commandCounter--;
                    argc = 0;
                }

                // If the input is a 'bg' command
                if (strcmp(arguments[0], "bg") == 0) {
                    // Check if there's a process in the background
                    if (pidLastStop != -1) {
                        // Resume the last stopped process
                        pidLastStop = -1;
                        kill(pidLastStop, SIGCONT);
                    }
                    // Skip further processing
                    continue;
                }

                // Check if it is the first command in the pipeline and the leftmost part
                if (currentPipeNumber == 0 && isLeftmostCommand) {
                    // Create pipes for the entire pipeline
                    for (int i = 0; i < numberOfPipes; ++i) {
                        if (pipe_fd!=NULL && pipe(pipe_fd[i]) == -1) {
                            perror("pipe");
                            exit(1);
                        }
                    }
                }

                int status;
                pid_t pid = fork();

                // Increment counters for non-background commands
                if (runInBackground) {
                    commandCounter++;
                }
                else {
                    lastPid = pid;
                }

                // Child process
                if (pid == 0) {
                    // Set up redirection for input/output based on pipeline position
                    if (isMiddleCommand && pipe_fd!=NULL) {
                        close(pipe_fd[currentPipeNumber - 1][1]);
                        dup2(pipe_fd[currentPipeNumber - 1][0], STDIN_FILENO);
                        close(pipe_fd[currentPipeNumber][0]);
                        dup2(pipe_fd[currentPipeNumber][1], STDOUT_FILENO);
                    }
                    if (isRightmostCommand && pipe_fd!=NULL) {
                        close(pipe_fd[currentPipeNumber][1]);
                        dup2(pipe_fd[currentPipeNumber][0], STDIN_FILENO);
                    } else if (isLeftmostCommand && pipe_fd!=NULL) {
                        close(pipe_fd[currentPipeNumber][0]);
                        dup2(pipe_fd[currentPipeNumber][1], STDOUT_FILENO);
                    }

                    // Redirect output to a file if needed
                    if (isOutputRedirected && (isRightmostCommand || numberOfPipes == 0)) {
                        int fd_file = open(saveFileName, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
                        if (fd_file == -1) {
                            perror("open");
                            exit(1);
                        }
                        dup2(fd_file, STDOUT_FILENO);
                    }

                    // Execute the command
                    if (strcmp(arguments[0], "bg") != 0) {
                        execvp(arguments[0], arguments);
                        perror("exec");
                        dup2(STDOUT_FILENO, STDOUT_FILENO);
                        exit(1);
                    }
                }

                // Check if fork failed
                if (pid < 0) {
                    perror("fork");
                    return 1;
                }

                // Parent process
                if (pid > 0 && !runInBackground && pid != pidLastStop) {
                    // Wait for the child to finish
                    waitpid(pid, &status, WUNTRACED);

                    // Handle pipeline and file redirection
                    if (isMiddleCommand && pipe_fd!=NULL) {
                        close(pipe_fd[currentPipeNumber - 1][0]);
                        close(pipe_fd[currentPipeNumber][1]);
                        isLeftmostCommand = false;
                        isRightmostCommand = true;
                    } else if (isLeftmostCommand && pipe_fd!=NULL) {
                        close(pipe_fd[currentPipeNumber][1]);
                        isLeftmostCommand = false;
                        isRightmostCommand = true;
                    } else if (isRightmostCommand && pipe_fd!=NULL) {
                        close(pipe_fd[currentPipeNumber][0]);
                        isRightmostCommand = false;
                        hasMoreCommandsAfterPipe = false;

                        // Free allocated memory for pipes
                        if (numberOfPipes == (currentPipeNumber + 1)) {
                            for (int i = 0; i < numberOfPipes; ++i) {
                                free(pipe_fd[i]);
                            }
                            free(pipe_fd);
                            pipe_fd = NULL;
                        }
                        currentPipeNumber = -1;
                        numberOfPipes = 0;
                    }

                    if (isOutputRedirected && !isRightmostCommand && !isMiddleCommand) {
                        isOutputRedirected = false;
                        dup2(STDOUT_FILENO, STDOUT_FILENO);
                    }

                    // Handle exit status and update counters
                    if (WIFEXITED(status)) {
                        int exitStatus = WEXITSTATUS(status);

                        if (exitStatus == 0) {
                            if (hasMoreCommands )
                                hasMoreCommands = false;
                            commandCounter++;
                        }
                        freeArguments(arguments, memoryCleanupFlags);
                    }
                }

            }
        }
    }

// Free the memory allocated for a linked list
    void freeList(struct node *head) {
        struct node *current = head;
        struct node *next;
        while (current != NULL) {
            next = current->next;
            free(current);
            current = next;
        }
    }

// Inserts a new node at the beginning of the linked list
    void insertToLinkedList(struct node **head, vars newData) {
        struct node *newNode = (struct node *) malloc(sizeof(struct node));
        newNode->data = newData;
        newNode->next = *head;
        *head = newNode;
    }

// Find and return the node with the specified value name in the linked list
    struct node *findNode(struct node *head, const char *valueName) {
        struct node *current = head;

        // Traverse the linked list
        while (current != NULL) {
            // Check if the current node's name matches the specified value name
            if (strcmp(current->data.name, valueName) == 0) {
                return current;  // Node found, return the pointer to it
            }
            current = current->next;
        }
        return NULL;  // Node not found
    }

// Find the first occurrence index of a character in a string
    int findFirstOccurrenceIndex(char *str, char targetChar) {
        char *charPtr = strchr(str, targetChar);
        return (charPtr != NULL) ? (int) (charPtr - str) : -1;
    }

// Find the first occurrence index of a character in a string, starting from a given index
    int findFirstOccurrenceIndexFrom(char *str, char targetChar, int startIndex) {
        // Extract the substring starting from the given index
        char substring[strlen(str) - startIndex + 1];
        int j = 0, i = startIndex;
        while (i < strlen(str))
            substring[j++] = str[i++];
        substring[j] = '\0';

        // Find the index of the target character in the substring
        int indexInSubstring = findFirstOccurrenceIndex(substring, targetChar);

        // If the character is not found, return -1; otherwise, return the actual index in the original string
        return (indexInSubstring == -1) ? -1 : indexInSubstring + startIndex;
    }

// Function to retrieve the value of a variable and update argv
    char *getVariableValue(struct node *head, const char *variableName, int listSize, char **argv, int argc) {
        struct node *result = findNode(head, variableName);

        if (listSize == 0 || result == NULL) {
            return NULL;
        }

        char *variableValue = (char *) malloc(sizeof(char) * (strlen(result->data.value) + 1));
        strcpy(variableValue, result->data.value);

        // Update argv if applicable
        if (argc < 10) {
            argv[argc] = variableValue;
        }

        return variableValue;
    }

// Frees the dynamically allocated memory for specific elements in arguments based on shouldFree array
void freeArguments(char** argv, const int* shouldFree) {
    int i = 0;
    while (shouldFree[i] != -1) {
        free(argv[shouldFree[i]]);
        i++;
    }
}

// Signal handler for SIGCHLD
void sig_child(int sig) {
    // Reap terminated child processes
    waitpid(-1, NULL, WNOHANG);
}

// Signal handler for SIGSTOP
void sig_stop(int sig) {
    // Pause the last running background process
    if (lastPid != -1) {
        // Send SIGSTOP to the last running background process
        kill(lastPid, SIGSTOP);
        // Update the pidLastStop to keep track of the stopped process
        pidLastStop = lastPid;
        lastPid = -1; // Reset lastPid to indicate no running background process
    }
}
