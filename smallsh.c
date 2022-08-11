// Author: Brayden Murphy
// Class: CS 344
// Assignment 3
// Date: 2/7/22
// sources cited: adapted from OSU CS 344 class materials

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <ctype.h>
#include <signal.h>

// global variable to track when parent process receives SIGTSTP signal 
// and enters foreground only mode
// updated by custom signal handler for SIGTSTP
// declared as global var so custom signal handler can access it 
bool state = false;

// struct to store components of command line input
// including whether the command is a comment
// or meant to be run in the background 
struct command
{
    char *arguments[513];  
    int argNum; 
    bool comment; 
    bool background; 
    bool cd; 
    bool status; 
    bool exit; 
    bool input; 
    char* inputSrc; 
    bool output; 
    char* outputDest; 
    
}; 

/*
* creates command struct out of user input to command line 
* allows program to execute commands with needed information about 
* type of command and how it is to be run  
*/ 
struct command *createCommand(char *currLine)
{
    struct command *currCommand = malloc(sizeof(struct command));

    // for use with strtok_r
    char *saveptr; 

    // command and command arguments are separated by spaces, creates
    // first token and saves it to first position of arguments array 
    char *token = strtok_r(currLine, " \n", &saveptr);  // includes \n for last argument in input
    currCommand->arguments[0] = calloc(strlen(token) + 1, sizeof(char)); 
    strcpy(currCommand->arguments[0], token); 
    int x = 1;  //x will be index position of arguments array 
    
    // while loop finishes tokenizing each argument of command line input
    // while iterating through arguments array of command struct and saving each token 
    while(token != NULL)
    {
        token = strtok_r(NULL, " \n", &saveptr);
        // stops saving tokens once token is set to null 
        if(token == NULL)
        { 
            break; 
        }
        currCommand->arguments[x] = calloc(strlen(token) + 1, sizeof(char)); 
        strcpy(currCommand->arguments[x], token); 
        x++; // iterates through command struct's arguments array which stores arguments of input 
    }
    currCommand->argNum = x; //saves number of arguments for later use in iteration 
    
    // compares first stored token from input to "#", if they are equal
    // input is meant to be non-executed comment, so command bool comment is set to 
    // true
    int commentcmp = strcmp(currCommand->arguments[0], "#"); 
    if(commentcmp == 0)
    {
        currCommand->comment = true; 
    }
    else
    {
        currCommand->comment = false;
    }
    int lastIndex = currCommand->argNum - 1; 

    // compares last token from input to "&", if they are equal command is meant to
    // be run as a background process, so command bool member background set to true
    int backgroundcmp1 = strcmp(currCommand->arguments[lastIndex], "&\n"); 
    int backgroundcmp2 = strcmp(currCommand->arguments[lastIndex], "&");
    if(backgroundcmp1 == 0 || backgroundcmp2 == 0)
    {
        currCommand->background = true; 
        currCommand->arguments[lastIndex] = NULL;   //removes & from token list so it is not used in exec()  
    }
    else
    {
        currCommand->background = false;
    }

    // compares first token from input to "cd", if they are equal command is
    // built-in command cd, so command member cd is set to true 
    int cdcmp1 = strcmp(currCommand->arguments[0], "cd");
    int cdcmp2 = strcmp(currCommand->arguments[0], "cd\n");
    if(cdcmp1 == 0 || cdcmp2 == 0)
    {
        currCommand->cd = true; 
    }
    else
    {
        currCommand->cd = false; 
    }

    // compares first token from input to "exit", if they are equal command is
    // built-in command exit, so command member exit is set to true 
    int exitcmp1 = strcmp(currCommand->arguments[0], "exit");
    int exitcmp2 = strcmp(currCommand->arguments[0], "exit\n");
    if(exitcmp1 == 0 || exitcmp2 == 0)
    {
        currCommand->exit = true; 
    }
    else
    {
        currCommand->exit = false; 
    }

    // compares first token from input to "status", if they are equal command is
    // built-in command status, so command member status is set to true
    int statcmp1 = strcmp(currCommand->arguments[0], "status");
    int statcmp2 = strcmp(currCommand->arguments[0], "status\n");
    if(statcmp1 == 0 || statcmp2 == 0)
    {
        currCommand->status = true; 
    }
    else
    {
        currCommand->status = false; 
    }

    // initializes input and output members of command struct to false
    // these members will be updated to true if input or output redirection is
    // included in command line input 
    currCommand->input = false; 
    currCommand->output = false; 

    // iterates through tokens saved from command line input to check if < or > symbols
    // were included, which would indicate input or output redirection in command 
    for(int z = 0; z < currCommand->argNum; z++)
    { 
        if(currCommand->arguments[z] != NULL)
        { 
            int outputcmp1 = strcmp(currCommand->arguments[z], ">");
            if(outputcmp1 == 0)
            {
                currCommand->output = true; 
                currCommand->outputDest = calloc(strlen(currCommand->arguments[z+1]) + 1, sizeof(char));

                // saves token after >, which is where output redirection will go, to 
                // command member outputDest
                strcpy(currCommand->outputDest, currCommand->arguments[z+1]); 

                // removes > symbol and output redirection destination from arguments so neither 
                // token is used as arguments when executing the commmand 
                currCommand->arguments[z] = NULL; 
                currCommand->arguments[z+1] = NULL; 
                continue;   //ensures arguments array is check for < symbol and src as well 
            }

            int inputcmp2 = strcmp(currCommand->arguments[z], "<");
            if(inputcmp2 == 0)
            {
                currCommand->input = true; 
                currCommand->inputSrc = calloc(strlen(currCommand->arguments[z+1]) + 1, sizeof(char));

                // saves token after <, which is where input redirection src is directed from 
                strcpy(currCommand->inputSrc, currCommand->arguments[z+1]);

                // removes < symbol and input redirection source from arguments so neither is 
                // used as arguments when executing the command  
                currCommand->arguments[z] = NULL; 
                currCommand->arguments[z+1] = NULL; 
            }
        }
        
    }
    return currCommand; //returns newly created command struct with all info needed to execute 
}; 

//built-in execution of "cd" command
void execCD(struct command *command)
{ 
    // if cd has no arguments, working directory is changed to env var HOME 
    if(command->arguments[1] == NULL)
    {  
        const char *name = "HOME"; 
        const char *path = getenv(name);  
        chdir(path); 
    }

    // if cd command input includes argument, this argument is where working directory
    // is changed to 
    else
    { 
        const char *path =  command->arguments[1];  
        chdir(path);
    }
};

// built-in execution of "status" command
// prints out current value stored in exitStatus variable
// which is updated to store exit value or signal termination of 
// last executed foreground process 
void execStatus(struct command *command, int* exitStatus)
{
    printf("exit value %d \n", *exitStatus);
    fflush(stdout);  
}; 

// function executes user input commands 
void execCommand(struct command *command, int* exitStatus, int openProcesses[], int* openIndex)
{ 
    // runs command as a foreground process
    if(!command->background)
    {
        pid_t spawnPid = -5; 
        int childExitStatus = -5; 

        // calls fork() to create child process 
        spawnPid = fork(); 
        switch (spawnPid) {
            // prints error message if fork() returns error value of -1
            case -1: { perror("Error:\n"); exit(1); break;}

            // runs the child process 
            case 0: {
                // signal handler take_action to make foreground process terminate upon SIGINT 
                // signal handler ignore_action to make foreground process ignore SIGTSTP 
                struct sigaction take_action = {0}; 
                struct sigaction ignore_action = {0};
                take_action.sa_handler = SIG_DFL; 
                ignore_action.sa_handler = SIG_IGN;
                sigaction(SIGINT, &take_action, NULL);
                sigaction(SIGTSTP, &ignore_action, NULL);

                // if command member input is true, input redirection is performed 
                if(command->input)
                {
                    int fd_in = open(command->inputSrc, O_RDONLY, 0600); 
                    if(fd_in == -1)
                    {
                        perror("Problem opening file"); 
                        *exitStatus = 1; 
                        break; 
                    }
                    else
                    {
                        dup2(fd_in, 0); // redirects stdin to inputSrc member of command struct 
                    }
                }

                // if command member output is true, output redirection is performed 
                if(command->output)
                {
                    int fd_out = open(command->outputDest, O_WRONLY | O_CREAT | O_TRUNC, 0600); 
                    if(fd_out == -1)
                    {
                        perror("Problem opening file"); 
                        *exitStatus = 1; 
                        break; 
                    }
                    else
                    {
                        dup2(fd_out, 1); // redirects stdout to outputDest member of command struct 
                    }
                }

                // executes user input command 
                execvp(command->arguments[0], command->arguments); 
                perror(""); 
                exit(2); 
                break; 
            }

            // parent process
            default: {
                waitpid(spawnPid, &childExitStatus, 0); 

                // updates exitStatus if child process exited
                if(WIFEXITED(childExitStatus))
                { 
                    *exitStatus = WEXITSTATUS(childExitStatus); 
                }

                // updates exitStatus if child process terminated 
                else
                {
                    printf("terminated by signal %d\n", WTERMSIG(childExitStatus)); 
                    *exitStatus = WTERMSIG(childExitStatus); 
                }
                break; 
            }
        }
    }

    // runs command as background process 
    else
    {
        int spawnPid = -5;  
        spawnPid = fork(); 
 
        switch (spawnPid) {
            // prints error message if fork() returns error 
            case -1: { perror("Error:\n"); exit(1); break;}

            // child process
            case 0: {
                // custom signal handlers so background child process ignores
                // SIGINT and SIGTSTP 
                struct sigaction ignore_action = {0}; 
                ignore_action.sa_handler = SIG_IGN; 
                sigaction(SIGINT, &ignore_action, NULL);
                sigaction(SIGTSTP, &ignore_action, NULL);

                // displays pid of background process to user 
                printf("background pid is %d \n", getpid()); 
                fflush(stdout); 
                
                // performs input redirection if command member input is true 
                if(command->input)
                {
                    int fd_in = open(command->inputSrc, O_RDONLY, 0600); 
                    if(fd_in == -1)
                    {
                        perror("Problem opening file"); 
                        *exitStatus = 1; 
                        break; 
                    }
                    else
                    {
                        dup2(fd_in, 0); // redirects stdin to inputSrc command member 
                    }
                }

                // redirects stdin to "/dev/null" if user did not specify input redirection 
                else
                {
                    int fd_in = open("/dev/null", O_RDONLY, 0600); 
                    dup2(fd_in, 0);        //stdin redirection 
                }

                // performs output redirection if command member output is true 
                if(command->output)
                {
                    int fd_out = open(command->outputDest, O_WRONLY | O_CREAT | O_TRUNC, 0600); 
                    if(fd_out == -1)
                    {
                        perror("Problem opening file"); 
                        *exitStatus = 1; 
                        break; 
                    }
                    else
                    {
                        dup2(fd_out, 1); // stdout redirected to outputDest command member 
                    }
                }

                // redirects stdout to "/dev/null" if user did not specify output redirection
                else
                {
                    int fd_out = open("/dev/null", O_WRONLY, 0600);
                    dup2(fd_out, 1);        //stdout redirection 
                }

                // executes user command 
                execvp(command->arguments[0], command->arguments);
                perror(""); 
                break; 
            }

            // parent process 
            default: {
                sleep(1);  
                openProcesses[*openIndex] = spawnPid; // saves child process PID in openProcesses aray  
                for(int i = 0; i < 200; i ++)
                {
                    if(openProcesses[i] == 0)
                    {
                        *openIndex = i; // updates openIndex to point to first "empty" position in array where child PID is not stored  
                        break; 
                    }
                } 
                break; 
            }
        }
    }
}; 

// uses isspace() function to check each character of 
// user input to see if any characters are non-blank
// returns bool value indicating whether entire line of input
// is blank or not - allows blank lines to be ignored and not run 
// as commands 
bool isBlank(char line[])
{
    bool notBlank = false; 

    for(int i = 0; i < strlen(line); i++)
    {
        char var = line[i]; 
        if(!isspace(var))
        {
            notBlank = true; 
        }
    }
    return notBlank; 
}

// replaces sequence "$$" in user input with process PID
char* expansion(char line[])
{
    // gets current pid and stores it as a string array 
    // in variable str by using sprintf() so 
    // pid value can be inserted into input string 
    int mypid = getpid();
    char str[10]; 
    sprintf(str, "%d", mypid);  
    
    // iterates through user input string line with index x and 
    // counts the number of expansions that will be performed 
    int x = 0; 
    int n = 0; 
    while(x < strlen(line))
    {
        if(line[x] == '$' && line[x+1] == '$')
        {
            n ++; 
            x++; 
            x++; 
        }
        else
        { 
            x++; 
        }
    }

    // multiples number of expansions n by 10 to ensure string2 has enough room for 
    // new output including pids inserted into string
    // adds string length of original string for same reason 
    n = n * 10; 
    n = n + strlen(line); 
    char *string2 = NULL;
    string2 = (char*)calloc(n, sizeof(char)); //allocates memory for new string 

    // iterates through original string and concatenates characters from line to new string2
    // replacing "$$" with pid in concatenation, otherwise concatenating chars unaltered
    int i = 0; 
    while(i < strlen(line))
    {
        // if "$$" substring is detected, concatenates pid to string2 and advances index i
        // two positions to skip over $ symbols on to next char in line
        if(line[i] == '$' && line[i+1] == '$')
        {
            strncat(string2, str, strlen(str));
            i++; 
            i++; 
        }

        // concatenates current char of line indexed by i to string2
        // and increments i 
        else
        {
            strncat(string2, line + i, 1); 
            i++; 
        }
    }
    char *pointer = string2; 
    return pointer; //returns pointer to new string with expansion performed on it 
}

// custom signal handler function for SIGTSTP signal (control-z)
void handle_SIGTSTP(int signo)
{
    // if state global var is currently false, set value to true 
    // and inform user shell is entering foreground-only mode 
    // and background process symbol "&" will be ignored 
    if(!state)
    {
        char* message = "Entering foreground-only mode (& is now ignored)\n"; 
        write(STDOUT_FILENO, message, 50);
        state = true; 
    }

    // if state global var is currently true (so shell is in foreground only mode)
    // set value to false and inform user foreground-only mode is being exited
    // and user will be able to run processes in background again 
    else
    {
        char* message = "Exiting foreground-only mode\n"; 
        write(STDOUT_FILENO, message, 30);
        state = false; 
    }
    
}

/*
* function which will prompt user for input and make calls to execute input commands 
*/ 
void CommandPrompt(int* exitStatus, int openProcesses[], int* openIndex)
{
    // creates custom signal handler so parent process of shell ignores
    // SIGINT signals 
    struct sigaction ignore_action = {0}; 
    ignore_action.sa_handler = SIG_IGN; 
    sigaction(SIGINT, &ignore_action, NULL);
 
    // creates custom signal handler so parent process of shell 
    // responds to SIGTSTP signal by switching between foreground-only mode 
    // and back 
    struct sigaction custom_action = {0}; 
    custom_action.sa_handler = handle_SIGTSTP; 
    sigfillset(&custom_action.sa_mask); 
    custom_action.sa_flags = 0; 
    sigaction(SIGTSTP, &custom_action, NULL); 

    bool userExit = false; //if user input equals "exit", while loop will end 

    // while loop continously prompts user for commands and executes them 
    while (userExit==false)
    {
        // iterates through array of background process PIDs which have been executed
        // and uses waitpid() to check if process has exited or terminated
        // informs user before prompt if child PIDs has exited/terminated 
        for(int i = 0; i < 200; i++)
        {
            if(openProcesses[i] != 0)
            {
                int childExitStatus = -5;
                pid_t childPid = waitpid(openProcesses[i], &childExitStatus, WNOHANG);
                if(childPid != 0)
                {
                    // uses macro to check if child process has exited to exit status can be displayed to user 
                    if(WIFEXITED(childExitStatus))
                    {
                        printf("background pid %d is done: exit value %d\n",openProcesses[i] , WEXITSTATUS(childExitStatus));
                        fflush(stdout); 
                        openProcesses[i] = 0; //sets PID back to 0 in array because it is done and user has been informed 
                    } 

                    // if process has not exited and waitpid() did not return 0, the process must have been terminated by signal
                    // uses macro to display terminating signal value to user 
                    else
                    {
                        printf("background pid %d is done: terminated by signal %d\n", openProcesses[i], WTERMSIG(childExitStatus));
                        fflush(stdout); 
                        openProcesses[i] = 0; //sets PID back to 0 in array because it is done and user has been informed  
                    } 
                    fflush(stdout); 
                }
            }
        }

        // prompts user for command with ":" and calls getline() to read input 
        fflush(stdout); 
        printf(": "); 
        fflush(stdout); 
        char *line = NULL;
        size_t len = 0;  
        int numChars = getline(&line, &len, stdin); 

        // if getline() returns an error from a signal interrupt, reset stdin status 
        if(numChars == -1)
        {
            clearerr(stdin); // resets status of stdin so getline() can read user input  
        }

        else
        { 
            bool valid = isBlank(line); 
            if(valid) // continues to run input if it is not blank, otherwise skips to next ":" prompt
            {
                char* expandedString = expansion(line); 
                // creates command struct from user input after expansion of "$$" is performed by expansion() 
                struct command *newCommand = createCommand(expandedString); 
                free(line); 

                // if global variable state is true, toggles command member background to false, 
                // so commands are run in foreground even if they include "&" argument 
                if(state)
                {
                    newCommand->background = false; 
                }

                // if user input is "exit", iterates through open process array
                // and kills those processes, then ends while loop by setting userExit to true
                // next line of code will be RETURN EXIT_SUCCESS; in main()
                if(newCommand->exit)
                { 
                    for(int i = 0; i < 200; i++)
                    {
                        if(openProcesses[i] != 0)
                        {
                            kill(openProcesses[i], 9);  
                        }
                    }
                    userExit = true; 
                }

                // runs built-in function status if user entered "status"
                else if(newCommand->status)
                {
                    execStatus(newCommand, exitStatus); 
                }

                // runs built-in function cd if user entered "cd" as first argument 
                else if(newCommand->cd)
                {
                    execCD(newCommand); 
                }

                // executes user entered command if it is not exit, status, or cd and 
                // input is not a comment or blank line 
                else if(!newCommand->comment)
                {
                    execCommand(newCommand, exitStatus, openProcesses, openIndex); 
                }
            }
        }
    }
}; 

int main()
{
    // initialize exitStatus to track exit status/terminating signal of last foreground process
    // ran by commandPrompt() function, openProcesses array to store PIDs of background processes
    // and openIndex to track empty index of openProcesses in which to store next child PID created
    int exitStatus = 0; 
    int openProcesses[200] = {0}; 
    int openIndex = 0; 
    CommandPrompt(&exitStatus, openProcesses, &openIndex); 
    return EXIT_SUCCESS; 
}