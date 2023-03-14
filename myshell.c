#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

typedef struct command {

    char *name;               // Name of command to run
    char **args;             
    int numArgs;
    int error;               // 0 if results in an error, 1 otherwise 
    int redirect;            // 0 if no redirect, 1 otherwise, 2 or more means too many!
    char *redirectFile;      // Name of the file that the output is redirected to. Null if no redirect.
    int builtin;             // 1 if the command is built in, 0 if not.

} command;   

command* createCommand(char* name, char** args, int numArgs, int error, int redirect, char* redirectFile, int builtin) {
//creates a command given a linelet
   
    command* command = malloc(sizeof (*command));
    command->name = name;
    command->args = args;
    command->numArgs = numArgs;    
    command->error = error;
    command->redirect = redirect;
    command->redirectFile = redirectFile;
    command->builtin = builtin;

    return command;
}

void freeCommand(command* command) {
//frees a command struct

   free(command->args);
   free(command);
}

void myPrint(char *msg) {
	write(STDOUT_FILENO, msg, strlen(msg));
}

void printError() {
//prints an error

	char error_message[30] = "An error has occurred\n";
    write(STDOUT_FILENO, error_message, strlen(error_message));
}

int isEmptyLine(char* line) {
//returns 1 if line is empty, 0 otherwise
    
    while (*line) {
        if (!isspace(*line++)) {
            return 0;
        }
    }
    return 1;
}

void checkRedirect(command* command) {
//checks if output will be redirected to a file and updates command accordingly

    if ((command->numArgs > 2) && (! strcmp(command->args[command->numArgs - 2], ">"))
        && command->redirect == 1) {
        command->redirectFile = command->args[command->numArgs - 1];
    }
    else if (command->redirect > 1){
        command->error = 0;
    }
}

void callNonBuiltin(command* command) {
//calls a non-built-in command
    
    pid_t pid = fork();
    char* name = command->name;
    char** args = command->args;
    if (pid == 0) {
        if (command->redirect == 1){
            int outputFile = open(command->redirectFile, O_EXCL | O_CREAT | O_RDWR, 0777); 
            if (outputFile == -1) {                                                 
                command->error = 0;                                                 
                printError();
                exit(1);
            }                                                                       
            else {                                                                  
                if (dup2(outputFile, STDOUT_FILENO) != -1){                         
                   close(outputFile);                                            
                }                                                                   
            }
            char** new_args = malloc((command->numArgs - 1) * sizeof(char*));   
            for (int i = 0; i < command->numArgs-2; i++){
                new_args[i] = args[i]; 
            }
            new_args[command->numArgs-2] = NULL; 
            args = new_args;
        }
        else if (command->redirect > 1){
            command->error = 0;
            exit(1);
        }
        if (execvp(name, args) == -1) {
            command->error = 0;
            printError();
            exit(0);
        }
    }
    else {
        wait(NULL);
    }
}

void myExit(command* command) {
//executes my exit
    
    if (command->numArgs > 1) {
        command->error = 0;
    }
    else {
        exit(0);
    }
}

void myPwd(command* command) {
//executes my pwd
    if (command->numArgs > 1) {
        command->error = 0;
    }
    else {
        char path[500];
        getcwd(path, 500);
        myPrint(path);
        myPrint("\n");
    }
}

void myCd(command* command) {
//executes my cd
    
    if (command->numArgs == 1) {
        chdir(getenv("HOME"));
    }
    else {
        if (command->numArgs == 2) {
            int success = chdir(command->args[1]);
            if (success != 0) {
                command->error = 0; 
            }
        }
        else {
            command->error = 0;
        }
    }
}

void executeCmd(command* command) {
//executes a command

    char* name = command->name;

    if (((command->name) != NULL) && ( !(strcmp (name, "exit")) || !(strcmp (name, "pwd")) || !(strcmp (name, "cd")))) {
        
        command->builtin = 0;
        
        if (!(strcmp (name, "exit"))) {
            myExit(command);
        }
        else if (!(strcmp (name, "pwd"))) {
            myPwd(command);
        }
        else {
            myCd(command);
        }
    }
    else {
        command->builtin = 1;
        callNonBuiltin(command);
    }
}

command* parseLinelet(char* linelet) {
//parses a linelet and returns a pointer to a command with the line's information

    char* dupLinelet = strdup(linelet);
    int numRedirects = 0;
    for (int i=0; i < strlen(dupLinelet); i++){
        numRedirects += (dupLinelet[i] == '>');
    }
    char* modifiedLinelet =  malloc((strlen(dupLinelet) + 1 + (2 * numRedirects)) * sizeof(char));
    int j = 0;
    for (int i = 0; i < strlen(dupLinelet) + 1 + (2 * numRedirects); i++){
        if (dupLinelet[j] == '>'){
            modifiedLinelet[i] = ' ';
            modifiedLinelet[i+1] = '>';
            modifiedLinelet[i+2] = ' ';
            i = i+2;
        } else {
            modifiedLinelet[i] = dupLinelet[j];
        }
        j++;
    } 
    modifiedLinelet[strlen(dupLinelet) + 2 * numRedirects] = 0;
    char* modifiedCopy = strdup(modifiedLinelet);    

    int numArgs = 0; 
    char* token = strtok(modifiedLinelet, " \t");
    while (token != NULL) {
        numArgs ++;
        token = strtok(NULL, " \t");
    }
    char** args = malloc((numArgs + 1) * sizeof(char*));
    token = strtok(modifiedCopy, " \t");
    int i = 0;
    while (token != NULL) {
        args[i] = token;
        i++;
        token = strtok(NULL, " \t");
    }
    args[numArgs] = NULL;
    char* name = args[0];
    command* command = createCommand(name, args, numArgs, 1, numRedirects, NULL, 1);
    return command;
}

void parseLine(char* line) {
//separates line by semicolon, then calls functions to parse further
    
    char* saveptr1;
    char* linelet = strtok_r(line, ";", &saveptr1);    
    command* command;

    while (linelet != NULL) {
        if (strlen(linelet) != 0) {        
            command = parseLinelet(linelet);
            checkRedirect(command);
            if (command->error) {
                executeCmd(command);
            } 
            // executeCmd can change command->error, so we check the condition again
            if (!command->error) {
                printError();
            }
            freeCommand(command);
        }
        linelet = strtok_r(NULL, ";", &saveptr1);
    }
}

int main(int argc, char *argv[]) {

	char userInput[10000];
    char *pinput;
    FILE *batchfile = fopen(argv[1], "r");

    //Batch mode
    if (batchfile != NULL) {
        // Only allow one batch file
        for (int i = 2; i < argc; i++){
            if (fopen(argv[i], "r") != NULL){
                printError();
                exit(0);
            }
        }
        while (1) {            
            char* line = fgets(userInput, 10000, batchfile);
            
            if (!line) {
                exit(0);
            }
            else if (strlen(line) > 512) {
                if (!isEmptyLine(line)) {
                    write(STDOUT_FILENO, line, strlen(line));
                }
                printError();
            }
            else if (!isEmptyLine(line)) {
                write(STDOUT_FILENO, line, strlen(line));
                char* lineDup = strdup(line);
                lineDup[strlen(lineDup) - 1] = '\0';
                parseLine(lineDup);
                
            }
        }
    }
    
    //Interactive mode
    else {
        if (argc > 1){
            printError();
            exit(0);
        }
        while (1) {
            myPrint("myshell> ");
            pinput = fgets(userInput, 10000, stdin);
         
            if (!pinput) {
                exit(0);
            }
            else if (strlen(pinput) > 512) {
                if (!isEmptyLine(userInput)) {
                    write(STDOUT_FILENO, userInput, strlen(userInput));
                }
                printError();
            }
            else if (!isEmptyLine(userInput)) {
                char* lineDup = strdup(userInput);
                lineDup[strlen(lineDup) - 1] = '\0';
                parseLine(lineDup);
            }
        }  
   }     
}

