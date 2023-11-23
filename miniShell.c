#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>


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
void insertToLinkedLIst(struct node **head, vars newData);
char* dollar(struct node* head, char* temp, int listSize, char** argv, int argc);
void freeArgv(char** argv, const int* shouldFree);
struct node* findNode (struct node *head, char* valueName);
void freeList(struct node *head);
int findCharFirstInd(char* command, char c);
int findCharFromInd(char* command, char c, int i);
void sig_child(int sig);
void sig_stop(int sig);



int main() {

    int numCommand = 0;         //valid commands counter
    int numArgs = 0;            //arguments counter
    char command[511];          //the string from the user
    char save[510];             //array that keep the next command (if there is ;)
    char saveFileName[510];     //array to save the name of a file
    char savePipe[510];         //array to save the name of a file
    int enterCounter=0;         //how many enters from the users
    int moreCommand =1;         //if equal 0 - there is another command
    int moreCommandPipe=1;      //if equal 0 - there is another pipe's command
    int howManyComm =1;         //if equal 0 - need to add command to the numCommand
    int varNotFound=1;          //if equal 0 - the var not exist
    int invalidInput=1;         //if equal 0 - there are more than 10 arguments
    int firstCharIsDollar=1;    //if equal 0 - the first char in the command string is $
    int intoAFile=1;            //if equal 0 - the output should get into a file
    struct node *head = NULL;   //linked list that will store the variables
    int listSize=0;             //how many variables are in the linked list
    int background=1;           //if equal 0 - the current command should run on background
    int left=1;                 //if equal 0 - this is the left command in the pipe
    int mid=1;                  //if equal 0 - this is the middle command in the pipe
    int right=1;                //if equal 0 - this is the right command in the pipe
    int shouldFree[11];         //array to store all the indexes in argv that should free after the exec
    int shouldFreeCounter=0;    //how many pointers should be free
    int** pipe_fd;              //two-dimensions array of pipes
    int numberOfPipes=0;        //the number of pipes in the array
    int curPipe=-1;             // the current pipe iteration

    signal(SIGTSTP, sig_stop);
    signal(SIGCHLD, sig_child);

    //the current path-----------------------------------------------------------------------------------------------------------
    char currentPath[1024];
    if ((getcwd(currentPath, sizeof(currentPath))) == NULL) {
        printf("path error");
        return 1;
    }

//------------------------------------------------------------------------------------------------------------------------------
    while (1) {
        shouldFreeCounter=0;
        background=1;
        signal(SIGCONT, SIG_IGN);

        //in case there is another command to execute (the input include ;)--------------------------------------------------------------------
        if ( moreCommandPipe==0 || moreCommand == 0) {
            int saveINd = 0;
            if(right==0){
                for (; saveINd < strlen(savePipe); saveINd++)
                    command[saveINd] = savePipe[saveINd];
                savePipe[0]='\0';
                mid=1;
            }
            else{
                for (; saveINd < strlen(save); saveINd++)
                    command[saveINd] = save[saveINd];       //the array "save" will be the rest of the command - after the first ;
                moreCommand=1;
            }
            command[saveINd] = '\0';
        }

            //in case of input from user--------------------------------------------------------------------------------------------------------
        else {
            printf("\033[35m#cmd:%d|#args:%d@%s\033[0m ", numCommand, numArgs, currentPath);
            fgets(command, sizeof(command), stdin);
        }

        //-----------------------------------------------------------------------------------------------------------------------------------

        int len = (int)strlen(command);

        if(command[len-1]=='\n')
            command[len-1]='\0';
        char *argv[11]; //the arguments array
        for (int i = 0; i < 11; ++i) {
            argv[i]=NULL;
            shouldFree[i]=-1;
        }


        //in case the input is 'enter'-------------------------------------------------------------------------------------------------------
        if (command[0] == 0) {
            if (enterCounter == 2) {        //if the user click on the enter 3 times in a row - exit
                freeList(head);
                exit(0);
            }
            enterCounter++;
            continue;
        }
        enterCounter = 0;

        //check if there are more commands ---------------------------------------------------------------------------------------------

        int ind2= findCharFirstInd(command, ';');
        int i1= findCharFirstInd(command,'"');
        int i2;
        if(i1==-1)
            i2=-1;
        else
            i2= findCharFromInd(command,'"',i1+1);
        while(ind2!=-1){
            if(!(i1<ind2 && ind2<i2)){
                int j = ind2+1;
                for ( int i=0 ; j <= strlen((command)); ++i) {
                    save[i] = command[j++];
                }
                moreCommand = 0;
                howManyComm=0;
                command[ind2]='\0';
                break;
            }
            ind2= findCharFromInd(command,';',ind2+1);
        }

        int ind5 = findCharFirstInd(command, '&');
        if(ind5!=-1){
            background=0;
            command[ind5]='\0';
        }

        //if the output should insert into a file
        int ind4= findCharFirstInd(command,'>');
        if(ind4!=-1){
            int j = ind4+1;
            while (j <= strlen((command))) {
                if(command[j]!=' ')
                    break;
                j++;
            }
            for ( int i=0 ; j <= strlen((command)); ++i) {
                saveFileName[i] = command[j++];
                if (command[j]==' ')
                    break;
            }
            command[ind4]='\0';
            intoAFile=0;
        }

        //if there is a pipe
        int ind6= findCharFirstInd(command,'|');
        int sent1= findCharFirstInd(command,'"');
        int sent2=-1;
        if (sent1!=-1)
            sent2= findCharFromInd(command,'"',sent1+1);
        if ((sent1<ind6 && sent2>ind6)){
            ind6= findCharFromInd(command,'|',sent2+1);
        }
        if(ind6 !=-1){
            if (numberOfPipes==0){
                for (int i = ind6; i < strlen(command); ++i) {
                    if (command[i]=='|')
                        numberOfPipes++;
                }
                pipe_fd= malloc(numberOfPipes*sizeof(int*));
                for (int i = 0; i < numberOfPipes; ++i) {
                    pipe_fd[i]= malloc(2* sizeof(int));
                }
                mid=1;
            }
            else
                mid=0;

            curPipe++;
            left=0;
            right=1;
            int j = ind6+1;
            for ( int i=0 ; j <= strlen((command)); ++i) {
                savePipe[i] = command[j++];
            }
            moreCommandPipe = 0;
            command[ind6]='\0';
        }

        int ind1= findCharFirstInd(command,'=');
        //there is a variable assignment - add new struct to the array-------------------------------------------------------------------------
        if (ind1!=-1) {

            //Assigning a name to the variable
            char *temp1 = strtok(command, "=");
            vars var;
            int ind=0;
            for(int i =0; temp1[i]!='\0'; i++){
                if(temp1[i]!=' ') {
                    var.name[ind] = temp1[i];
                    ind++;
                }
            }
            var.name[ind]='\0';

            //Assigning a value to the variable
            temp1 = strtok(NULL, " ");
            ind=0;
            for(int i =0; temp1[i]!='\0'; i++){
                var.value[ind] = temp1[i];
                ind++;
            }
            var.value[ind]='\0';

            temp1 = strtok(NULL, "\0");
            if(temp1!=NULL){
                printf("%s: command not found\n",temp1);
                continue;
            }

            //insert the new var into the array
            listSize++;
            insertToLinkedLIst(&head, var);
        }

            //the input is a command that need to exec ----------------------------------------------------------------------------------------

        else {
            char *rest;
            char *temp = strtok_r(command, " ", &rest);
            int argc = 0;

            if (findCharFirstInd(temp, '$') != 0) {
                argv[0] = temp;
                argc = 1;  //arguments of the current command counter
            } else
                firstCharIsDollar = 0;


            while (temp != NULL) {

//if the input is not legal - there are more than 10 arguments------------------------------------------------------------
                if (argc > 10 && argv[10] != NULL) {
                    invalidInput = 0;
                    break;
                }


//if this is a variable--------------------------------------------------------------------------------------------------------------
                if (rest[0] == '$' || firstCharIsDollar==0) {
                    int charIndex= findCharFirstInd(rest,'"');
                    if(rest[0]=='$' && firstCharIsDollar!=0){
                        if (charIndex!=-1){
                            temp = strtok_r(NULL, "\"", &rest);
                        }
                        else
                            temp = strtok_r(NULL, " ", &rest);
                    }
                    temp[charIndex]='\0';
                    firstCharIsDollar=1;
                    int charInd= findCharFirstInd(temp,';');
                    if(charInd!=-1)
                        temp[charInd]='\0';
                    char* dollarI = dollar(head,temp+1,listSize,argv,argc);
                    if(dollarI==NULL){
                        varNotFound=0;
                        break;
                    }
                    shouldFree[shouldFreeCounter]=argc;
                    shouldFreeCounter++;
                    if (charIndex!=-1){
                        dollarI=(char*) realloc(dollarI,sizeof(dollarI)+sizeof(rest)+1);
                        strcat(dollarI,rest);
                        dollarI[findCharFirstInd(dollarI,'"')]='\0';
                        temp = strtok_r(NULL, "\"", &rest);
                        temp = strtok_r(NULL, " ", &rest);
                    }
                    argv[argc]=dollarI;
                    argc++;

                    if(charInd!=-1){
                        break;
                    }
                }


//if this is a sentence between ""------------------------------------------------------------------------------------------


                else if (rest[0] == '"' && firstCharIsDollar == 1) {
                    temp = strtok_r(NULL, "\"", &rest);
                    int dollarInd = findCharFirstInd(temp, '$');
                    if (dollarInd == -1)
                        argv[argc] = temp;
                    else {
                        char *newTemp = strtok_r(temp, " ", &rest);
                        char* dollarI = dollar(head, newTemp + 1, listSize, argv, argc);
                        if (dollarI == NULL) {
                            varNotFound = 0;
                            break;
                        }
                        shouldFree[shouldFreeCounter]=argc;
                        shouldFreeCounter++;
                        argv[argc]=dollarI;
                    }
                    argc++;
                }

                else {
                    temp = strtok_r(NULL, " ", &rest);
                    argv[argc] = temp;
                    argc++;
                }
            }

//if the input is a variable that doesn't exist-------------------------------------------------------------------------------
            if(varNotFound==0){
                varNotFound=1;
                if(argv[0]==NULL){
                    continue;
                }
                argc++;
            }

            int argvInd=0;
            argc=0;
            while (argv[argvInd]!=NULL){
                argc++;
                argvInd++;
            }


            //if there are more than 10 arguments--------------------------------------------------------------------------------------------
            if(invalidInput==0){
                freeArgv(argv,shouldFree);
                printf("Invalid input\n");
                invalidInput=1;
                continue;
            }


            //if the input is cd- command that not supported----------------------------------------------------------------------------------
            if (strcmp(argv[0], "cd") == 0) {
                argv[0] = "echo";
                argv[1] = "cd not supported";
                argv[2] = NULL;
                numCommand--;
                argc=0;
            }

            if (strcmp(argv[0], "bg") == 0) {
                if (pidLastStop!=-1){
                    pidLastStop=-1;
                    kill(pidLastStop,SIGCONT);
                }
                continue;
            }

//-------------------------------------------------------------------------------------------------------------------------------------------


            if (curPipe==0 && left==0){
                for (int i = 0; i < numberOfPipes; ++i) {
                    if (pipe(pipe_fd[i])==-1){
                        perror("pipe");
                        exit(1);
                    }
                }
            }

            int status;
            pid_t pid = fork();

            if (background==0){
                numCommand++;
                numArgs += (argc+1);
            }
            else {
                lastPid=pid;
            }

            //child process
            if (pid == 0) {
                if (mid==0){
                    close(pipe_fd[curPipe-1][1]);
                    dup2(pipe_fd[curPipe-1][0],STDIN_FILENO);
                    close(pipe_fd[curPipe][0]);
                    dup2(pipe_fd[curPipe][1],STDOUT_FILENO);
                }
                if(right==0){
                    close(pipe_fd[curPipe][1]);
                    dup2(pipe_fd[curPipe][0],STDIN_FILENO);
                }
                else if (left==0){
                    close(pipe_fd[curPipe][0]);
                    dup2(pipe_fd[curPipe][1],STDOUT_FILENO);
                }

                if(intoAFile==0 && (right==0 || numberOfPipes==0)){
                    int fd_file = open(saveFileName,O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU);
                    if(fd_file==-1){
                        perror("open");
                        exit(1);
                    }
                    dup2(fd_file,STDOUT_FILENO);
                }


                if(strcmp(argv[0],"bg")!=0){
                    execvp(argv[0], argv);
                    perror("exec");
                    exit(1);
                }
            }

            //the fork didn't work
            if (pid < 0) {
                perror("fork");
                return 1;
            }

            //the father process
            if(pid > 0 && background==1 && pid!=pidLastStop) {
                waitpid(pid, &status, WUNTRACED);
                if (mid == 0) {
                    close(pipe_fd[curPipe - 1][0]);
                    close(pipe_fd[curPipe][1]);
                    left = 1;
                    right = 0;
                } else if (left == 0) {
                    close(pipe_fd[curPipe][1]);
                    left = 1;
                    right = 0;
                } else if (right == 0) {
                    close(pipe_fd[curPipe][0]);
                    right = 1;
                    moreCommandPipe = 1;
                    if (numberOfPipes == (curPipe + 1) ) {
                        for (int i = 0; i < numberOfPipes; ++i) {
                            free(pipe_fd[i]);
                        }
                        free(pipe_fd);
                    }
                    curPipe = -1;
                    numberOfPipes = 0;
                }

                if (WIFEXITED(status)) {
                    int exitStatus = WEXITSTATUS(status);
                    if (intoAFile == 0 && left == 1 && right == 1 && mid == 1) {
                        intoAFile = 1;
                        dup2(STDOUT_FILENO, STDOUT_FILENO);
                        argc += 2;
                    }
                    if (exitStatus == 0) {
                        if (howManyComm == 0)
                            howManyComm = 1;
                        numCommand++;
                        numArgs += argc;
                    }
                    freeArgv(argv, shouldFree);
                }
            }
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------------------------------
//function to find the index of the first char apperance in string
int findCharFirstInd(char* command, char c){
    int toReturn=-1;
    char *ptr1 = strchr(command, c);
    if (ptr1 != NULL) {
        toReturn = (int)(ptr1 - command);
    }
    return toReturn;
}

int findCharFromInd(char* command, char c, int ind){
    char afterChar[strlen(command)];
    int j=0,i=ind;
    while(i< strlen(command))
        afterChar[j++]=command[i++];
    afterChar[j]='\0';
    int toReturn=findCharFirstInd(afterChar,c);
    if (toReturn==-1)
        return toReturn;
    return toReturn+ind;
}

//function that change the value of the variable in the argv array
char* dollar(struct node* head, char* temp, int listSize, char** argv, int argc){
    struct node *result = findNode(head, temp);
    if (listSize == 0 || result==NULL)
        return NULL;
    char *temp3=(char*) malloc(sizeof(char)*(strlen(result->data.value) +1 ));
    strcpy(temp3,result->data.value);
    return temp3;
}

void freeArgv(char** argv, const int* shouldFree){
    int i=0;
    while (shouldFree[i]!=-1){
        free(argv[shouldFree[i]]);
        i++;
    }
}



//linked list functions-------------------------------------------------------------------------------------------------------------------------
void insertToLinkedLIst(struct node **head, vars newData){
    struct node *newNode = (struct node*) malloc(sizeof(struct node));
    newNode->data = newData;
    newNode->next=*head;
    *head=newNode;
}

struct node* findNode (struct node *head, char* valueName){
    struct node *current =head;
    while (current!=NULL){
        if(strcmp(current->data.name, valueName)==0){
            return current;
        }
        current=current->next;
    }
    return NULL;
}

void freeList(struct node *head){
    struct node *current = head;
    struct node *next;
    while(current!=NULL){
        next=current->next;
        free(current);
        current=next;
    }
}

void sig_child(int sig){
    waitpid(-1,NULL,WNOHANG);
}

void sig_stop(int sig){
    if(lastPid!=-1){
        kill(lastPid,SIGSTOP);
        pidLastStop=lastPid;
        lastPid=-1;
    }
}
