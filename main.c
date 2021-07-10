#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

//Maximum characters allowed to be input
#define MAX_CHARS_INPUT 2048
//Maximum number of args allowed to be input
#define MAX_ARGS 512

//Max number of digits for pID
//Was not sure if PID could extend beyond 14 digits
#define MAX_PID_LENGTH 14

// Global foreground mode indicator variable
bool foreground_only_mode = false;


/*
 * struct:  _commands, Commands
 * --------------------------------------------------------------------------
 * Used to keep track of the following:
 *  1. Number of background proceses
 *  2. Number of arguments 
 *  3. Arguments inputed via commannd line.
 * 
 * Helps bundle important arguments together and streamline passing those arguments as 
 * function parameters.
 * 
 * Struct Members:
 *  bool exitStatus: flag is true if user entered "exit" into terminal; false by default.
 *  int is_background_process : 0 command is run in foreground; 1 command is run in background
 *  int numArgs: number of arguments entered by user (tokenized string values).
 *  pid_t background_processes[MAX_ARGS]: used to store backgroud process pid's.
 *  int bg_procs_count: total number of background processes stored
 *  char* inputArgs[MAX_ARGS]: tokenized arguments entered by user
 *  int processStatus: child process status.
 *
 */

typedef struct _commands {
    //exit status flag for exiting program
    bool exitStatus;
    //Flag to indicate if input commands are executed in background
    int is_background_process;
    //number of argument tokens that a user provides
    int numArgs;
    //pid_t array for background processes.
    pid_t background_processes[MAX_ARGS];
    //Total number of background process id's saved in the background_processes array
    int bg_procs_count;
    //Stores tokenized command arguments
    char* inputArgs[MAX_ARGS];
    //Stores child process status
    int processStatus;
}Commands;


/*
 * Function:  handler_SIGTSTP
 * --------------------------------------------------------------------------
 * Intercepts ctrl + z SIGTSTP signal
 * Will print a message to user when entering/exiting foreground only mode
 * Sets foreground_only_mode flag to true if fore_ground_only_mode was already false
 * and vice versa.
 * 
 * Important Variable:
 *  bool foreground_only_mode : true if we are in foreground only mode
 *      false if we are in normal mode.
 *   
 */
void handler_SIGTSTP(int signo) {
    //if we are not in foreground only mode
    fflush(stdout);
    if (foreground_only_mode == false) {
        //Set foreground-only mode flag to true
        foreground_only_mode = true;
        //Clear stdin
        clearerr(stdin);
        //Output entering foreground-only mode to user
        char* fgModeMsg = "\nEntering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, fgModeMsg, 52);
        //Clear stdout
        fflush(stdout);
    }
    //if already in foreground-only mode
    else {
        //Set foreground-only mode flag to false
        foreground_only_mode = false;
        clearerr(stdin);
        //Output exiting foreground-only mode to user
        char* fgModeMsg = "\nExiting foreground-only mode \n";
        write(STDOUT_FILENO, fgModeMsg, 32);
        //Clear stdout
        fflush(stdout);
    }
    //clear stdout
    fflush(stdout);
}


/*
 * Function: void kill_background_processes(Commands *cmds)
 * --------------------------------------------------------------------------
 * Kills all background processes stored in background_processes array member of
 * the Commands struct. Used primarly to kill processes before exiting the program and
 * after an error executing a child process.
 * 
 * Parameters
 *  Commands* cmds: Commands struct to access the following struct members
 * 
 * Helper Commands struct member:
 *  int bg_procs_count: number of background processes that have executed
 * 
 * Struct member reset:
 *  pid_t background_processes[MAX_ARGS]: array of child process pids executed in background.
 *  
 * 
 *  
 */
void kill_background_processes(Commands* cmds) {
    //temp variable to track the index of background processes
    int i = 0;
    //Loop through background process id's and kill those processes 
    while (i < cmds->bg_procs_count) {
        kill(cmds->background_processes[i], SIGTERM);
        i++;
    }
}


/*
* Function: init_Commands_List(Commands *cmds)
* --------------------------------------------------------------------
* Initializes Commands struct members with starting values
* 
* Member values initialized:
*   1. int numArgs: total number of tokenized arguments
*   2. int is_background_process: flag if process will be executed in background
*   3. int bg_procs_count: total count of background processes
*   4. bool exitStatus: flag to initiate exiting program
*   5. int processStatus: status of child process
* 
*/

// initialize Commands struct
void init_Commands_List(Commands* cmds) {
    //Number of tokenized user arguments
    cmds->numArgs = 0;
    //Flag value for indicating command is a foreground or background process
    cmds->is_background_process = 0;
    //Exit status flag
    cmds->exitStatus = false;
    //Count of background processes
    cmds->bg_procs_count = 0;
    //Status of child process monitored by parent process
    cmds->processStatus = 0;
}

/*
* Function: reset_inputArgs(Commands *cmds)
* --------------------------------------------------------------------
* Iterates over struct Commands member inputArgs and free's allocated
* memory in order to reuse inputArgs array for additional user commands.
* 
* Parameters:
*   Commands* cmds: pointer to Commands struct 
* 
* Helper Commands struct member: 
*   int numArgs: Total number of tokenized command arguments
* 
* Struct member reset:
*   char* inputArgs[MAX_ARGS]: Stores tokenized command arguments
* 
*/
void reset_inputArgs(Commands* cmds) {
    for (int i = 0; i <= (cmds)->numArgs; i++) {
        //Free memory allocated to inputArgs
        free((cmds)->inputArgs[i]);
        (cmds)->inputArgs[i] = NULL;
    }
    //Reset numArgs value to track next commandline arguments
    cmds->numArgs = 0;
    memset(cmds->inputArgs, '\0', MAX_ARGS);
}


/*
* Function: delete_commands(Commands *cmds)
* --------------------------------------------------------------------
* Iterates over struct Commands member inputArgs and free's allocated 
* memory.
* 
* Parameters:
*  Commands* cmds: pointer to Commands struct
*  
* Helper Commands struct member:
*   int numArgs: Total number of tokenized command arguments
* 
* Struct member memory deallocated:
*   char* inputArgs[MAX_ARGS]: Stores tokenized command arguments
* 
*/

void delete_commands(Commands* cmds) {
    for (int i = 0; i <= (cmds)->numArgs; i++) {
        //Free memory allocated to inputArgs
        free((cmds)->inputArgs[i]);
        //Set char pointers to NULL
        (cmds)->inputArgs[i] = NULL;
    }
    //Reset memory with null terminator
    memset(cmds->inputArgs, '\0', MAX_ARGS);
}

/*
 * Function:  void cd_command(Commands* cmds)
 * --------------------------------------------------------------------------
 * Takes a pointer to Commands struct and changes present working directory
 * respective to user's arguments stored in inputArgs array. cd command alone without additional
 * arguments will redirect the user to the location of the directory saved in HOME environment variable. 
 * 
 * Parameters: 
 *  Commands* cmds: pointer to Commands struct
 *  
 * Struct members utilized:
 *  char* inputArgs[MAX_ARGS]: Stores tokenized command arguments
 *  int numArgs: Total number of tokenized command arguments
 * 
 */

void cd_command(Commands* cmds) {
    //Temporary variable that saves number of argument tokens
    //entered by the user
    int num = cmds->numArgs;

    //Used to store target path if there are arguments after "cd"
    char* targetPath;

    //Create current working directory array
    char cwd[MAX_CHARS_INPUT];
    //Save the size of current working directory array
    size_t size = sizeof(cwd);

    // Get the current directory
    getcwd(cwd, size);
    //If there are arguments after "cd" command, process the token values after "cd"
    if (num != 1 && num > 1) {
        targetPath = cmds->inputArgs[num - 1];
        fflush(stdout);
        //Concatenate  '/' at the end of the current directory
        strcat(cwd, "/");
        //Concatenate the new path with '/' at the end with after current directory
        strcat(cwd, targetPath);
        //Change directory
        chdir(cwd);								
    }
    else // --- Only "cd" command was entered by user ---
    {
        //Get home directory
        chdir(getenv("HOME"));					
    }
    //Get new current directory
    getcwd(cwd, sizeof(cwd));
    //Output the new directory to terminal
    printf("%s\n", cwd);
    //Clear stdout
    fflush(stdout);
    //Reset targetPath
    targetPath = NULL;
};

/*
 * Function:  check_status(Commands* cmds)
 * --------------------------------------------------------------------------
 * Takes a pointer to Commands struct as parameter and checks if any child processes 
 * were terminated by accessing Commands struct member processStatus. 
 * If so, the exit status or termination signal will be printed to the user
 * Default: if run before any foreground commands, returns a value of 0.
 * 
 * Parameters:
 *  Commands* cmds: pointer to Commands struct
 *
 * Struct members utilized:
 *  int processStatus: status of child process
 * 
 */

//void check_status(int status)
void check_status(Commands* cmds) {
    //If the child process terminated normally
    if (WIFEXITED(cmds->processStatus)) {
        //Output exit value
        printf("exit value %d \n", WEXITSTATUS(cmds->processStatus));
        fflush(stdout);
    }
    //If the child was terminated by a signal
    else {
        //Output signal that terminated the process
        printf("terminated by signal %d\n", cmds->processStatus);
        fflush(stdout);
    }
}

/*
 * Function:  void expand_variable(char* inputBuffer, int buffLen)
 * --------------------------------------------------------------------------
 * Called by the function void get_user_input.Takes parameter char* inputBuffer 
 * and replaces pairs of $$ values with the process pid. 
 * Substring comparison is done via char *strstr(const char *main_string, const char *substring).
 * int sprintf(char *str, const char *format, ...) is used for reformatting
 * the string to have pid replace %d.
 *
 * Parameters:
 *  char* inputBuffer: user inputed arguments (untokenized)
 *  int buffLen: Length of the inputBuffer
 *
 */

void expand_variable(char* inputBuffer, int buffLen) {
    //Temporary string buffer used to save inputBuffer char values
    //and expand $$ values
    char string[buffLen + 1];
    strcpy(string, inputBuffer);
    //Substring to match
    char substr[] = "$$";
    //Char value used to replace substring $$ values
    char* replace_1 = "%";
    int substr_size = strlen(substr);
    //Make a copy of your `string` pointer (for safety)
    char* ptr = string;

    //Loop through string and replace $$ with pid
    while (1) {
        //Find pointer to next substring
        ptr = strstr(ptr, substr);
        //If no substring found, then break from the loop
        if (ptr == NULL) {
            break;
        }
        //If found, then replace it with "%"
        memset(ptr, *replace_1, substr_size);

        //Replace second character after % with a d to get %d
        *(ptr + 1) = 'd';
        //Increment our string pointer, pass replaced substring
        ptr += substr_size;
    }
    //Format string %d and replace it with pid
    sprintf(inputBuffer, string, getpid());
    //Reset pointers
    ptr = NULL;
    replace_1 = NULL;
}

/*
 * Function:  void get_user_input(Commands* cmds) 
 * --------------------------------------------------------------------------
 * void get_user_input takes user input into an initial inputBuffer and calls
 * expand_variable if '$$' is present. Then, inputBuffer is tokenized into individual
 * arguments and saved in Commands struct member inputArgs.
 * Then, the function checks for & at the end of arguments to check if the command
 * will be executed in the background or foreground.
 * 
 * Additional functionality:
 *  1. If & is present and foreground only mode is not true,
 *  is_background_process value is set to 1 to indicate the process will be 
 *  run in the background. This is to process & early and remove the &
 *  argument from the array of tokens. This simplifies executing commands 
 *  without the need to worry about "&" still being in inputArgs array of
 *  tokenized commands at the command execution stage in the "main" code block.
 * 
 * Parameters:
 *  Commands* cmds: Pointer to Commands struct
 * 
 * Struct members utilized:
 *  char* inputArgs[MAX_ARGS]: Stores tokenized command arguments
 *  int numArgs: total number of tokenized arguments provided by user
 * 
 */

void get_user_input(Commands* cmds) {
    //Used to store argument tokens
    char* token;
    //Input buffer to store initial user input via stdin
    char inputBuffer[MAX_CHARS_INPUT];
    //Reset inputBuffer before saving user input
    memset(inputBuffer, '\0', MAX_CHARS_INPUT);

    //Command line prompt message 
    fflush(stdout);
    fflush(stdin);
    char* prompt = ": ";
    //Output command line prompt ": "
    write(STDOUT_FILENO, prompt, strlen(prompt));
    //clear stdout
    fflush(stdout);

    //Read user input and save into inputBuffer
    fgets(inputBuffer, MAX_CHARS_INPUT, stdin);
    //Delete newline character at the end of user input
    strtok(inputBuffer, "\n");
    //Set buffer length 
    int bufferLength = strlen(inputBuffer);

    // $$ Variable expansion
    //Check for and expand $$ with shell's pid
    if (strstr(inputBuffer, "$$") != NULL) {
        //Call expand_variable to expand $$
        expand_variable(inputBuffer, bufferLength);
    }
    //Reset number of arguments to 0
    (cmds)->numArgs = 0;

    //Input tokenization step:
    //Tokenize inputBuffer delimited by spaces " "
    int arg_count = 0;
    //Begin tokenization
    token = strtok(inputBuffer, " ");
    //Copy initial value to inputArgs array
    cmds->inputArgs[arg_count] = strdup(token);
    //For commands or flags following the initial command
    if (token != NULL) {
        while (token != NULL)
        {
            //Save next token to inputArgs array
            cmds->inputArgs[arg_count] = strdup(token);
            //Increment the total count of arguments provide
            arg_count++;
            token = strtok(NULL, " \n");
        }
    }
    //Set null pointer at the end of inputArgs array
    cmds->inputArgs[arg_count + 1] = '\0';
    //Save the total count of arguments
    ((cmds)->numArgs) = arg_count;
    //Free memory for token
    free(token);
    prompt = NULL;
    //Reset token to NULL
    token = NULL;

    //Set num as a temp variable to save values of numArgs
    int num = cmds->numArgs;

    // Check for & at the end of arguments
    // If & is present and foreground only mode is not true,
    // is_background_process value is set to 1 to indicate the process will be
    // run in the background
    if (strcmp(cmds->inputArgs[num - 1], "&") == 0) {
        //make sure the array position is not null and the final 
        cmds->inputArgs[num - 1] = '\0';
        //Decrement total number of arguments to account for the removal of "&"
        cmds->numArgs = num - 1;
        //Check if foreground only mode is active
        if (foreground_only_mode == false) {
            //Set background flag to run command in background
            cmds->is_background_process = 1;
        }
        else {
            //Set background flag to 0
            //if foreground only mode is active
            cmds->is_background_process = 0;
        }
    }
    memset(inputBuffer, '\0', MAX_ARGS);
}


/*
 * Function:  void exec_other_commands(Commands* cmds)
 * --------------------------------------------------------------------------
 * Takes a pointer to Commands struct as a paraemter and runs non built-in commands.
 * Handles redirect commands as well. Commands are excuted via
 * execvp(cmds->inputArgs[0], cmds->inputArgs). Helps separate responsibilities 
 * of built-in commands and non built-in commands.
 * 
 *  Overall structure:
 *   Function iterates over inputArgs array which contains tokenized arguments provided by the user.
 *   If there is file redirection, there is no need to check if it is a background command and
 *   file redirection is done via dup2 calls.
 *   If there is no redirection, a background command will then use "/dev/null" for
 *   stdin, stdout.
 *
 *   Conditions: 
 *      1. If a process is a background process and input/output redirection was not specified, 
 *      then /dev/null must be used.
 *      2. If a process is a foreground process, 
 *           - input/output error if file not found 
 *           - Create file and write to file or open file and truncate file.
 *   
 * Parameters:
 *  Commands* cmds: Pointer to Commands struct
 * 
 * Struct members utilized:
 *  char* inputArgs[MAX_ARGS]: Stores tokenized command arguments
 *  int numArgs: total number of tokenized arguments provided by user
 * 
 */


void exec_other_commands(Commands* cmds) {
    //Redirection flag
    int redirection_flag = 0;
    //file descriptor variable
    int fileDescriptor;
    //Iterator variable
    int i = 0;
    // --------------- Process executed in FOREGROUND -------------------------
    //Check if this is a background process
    //Loop through tokens
    for (i = 0; i < cmds->numArgs; i++) {
        // "<" input redirection
        // stdin = 0
        if (strcmp(cmds->inputArgs[i], "<") == 0) {
            //open file read only
            fileDescriptor = open(cmds->inputArgs[i + 1], O_RDONLY, 0);
            // IF NO FILE FOUND TO OPEN AND READ
            //Check if open was successful
            if (fileDescriptor == -1) {
                //Output error to user that file was not found
                fprintf(stderr, "cannot open file %s for input\n", cmds->inputArgs[i + 1]);
                fflush(stdout);
                exit(1);
            }
            else {
                //Redirection occurs, set flag to 1
                redirection_flag = 1;
                //Redirect stdin to read from file
                dup2(fileDescriptor, 0);
            }
        }
        //stdout = 1, Redirect occurs
        else if (strcmp(cmds->inputArgs[i], ">") == 0) {
            // - open file for write only, then truncate the file if it exists
            // - if file doesn't exit, then create file and proceed to write to the file
            fileDescriptor = open(cmds->inputArgs[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fileDescriptor == -1) {
                //print an error message if there was an error
                //opening the file.
                fprintf(stderr, "cannot open %s for output\n", fileDescriptor);
                fflush(stdout);
                exit(1);
            }
            else { 
                redirection_flag = 1;
                //Redirect stdout to write to file
                dup2(fileDescriptor, 1);
            }
        }
        //If there is a redirection flag, set "<", ">", or file names to 0
        //so that we do not include those arguments in execvp
        if (redirection_flag == 1) {
            //Remove redirection or filename from arguments list
            cmds->inputArgs[i] = 0;
        }         
    }
    // ------ Check if command is a background process and has no redirection -------
    // ---------- if no redirection provided, stdout/stdin uses /dev/null -----------
    if (redirection_flag == 0 && cmds->is_background_process == 1) {
        fileDescriptor = open("/dev/null", O_WRONLY);
        if (fileDescriptor == -1) {
            fprintf(stderr, "cannot open /dev/null for input\n");
            fflush(stdout);
            exit(1);
        }
        //Redirect input stdin to "/dev/null" 
        dup2(fileDescriptor, 0);
        //redirect the output to /dev/null so 
        //that the input does not print
        //to the terminal and check for errors
        fileDescriptor = open("/dev/null", O_WRONLY);
        if (fileDescriptor == -1) {
            fprintf(stderr, "cannot open /dev/null for output\n");
            fflush(stdout);
            exit(1);
        }
        //Redirect output stdout to "/dev/null" 
        dup2(fileDescriptor, 1);
    }
    //Reset Redirection flag
    redirection_flag = 0;
    //Close file descriptor
    close(fileDescriptor);
    //execute the command, and print an error message
    //if the command was not found.
    fflush(stdout);
    int results = execvp(cmds->inputArgs[0], cmds->inputArgs);
    if (results) {
        //If file, directory, command not found
        fprintf(stderr, "%s: no such file or directory\n", cmds->inputArgs[0]);
        fflush(stdout);
        exit(1);
    }
}

/*Overall structure of main code block:
*   Initialize signal handlers, Commands struct pointer, and Commands struct members.
*   Proceeds to obtain user input and check tokenized arguments for matching
*   build-in commands. If there are no matching build-in commands, a child process
*   via fork() is created and the function exec_other_commands is called to execute
*   input/output redirection and other commands via execvp system call. 
* 
*   Forking child processes and using waitid, etc. example code used in lectures helped
*   create the overall structure of the while loop below.
*
*   Additional features and refactors:
*       While loop added at the end for parent process to monitor for
*       any terminated child process. This was easier to implement than looping
*       through the background_process array Command struct member and
*       checking each child process for termination.
*
*       Build-In Commands were initially an entirely separate function in itself.
*       This was similar to exec_other_commands function. There was an issue with
*       executing a child process and keeping track of variable values between different
*       processes. Just including build-in commands in the while loop helped simplify and
*       reduce any errors related to variable scope and process related data values being
*       different.
*/

int main() {

    //Variable to store process id
    int pid;

    //custom handler for SIGTSTP, toggles foreground-only mode, prints text noteice, and raises signal flag
    struct sigaction SIGINT_action = { 0 };
    struct sigaction SIGTSTP_action = { 0 };
    
    //Signal handler to ignore ^c
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    sigaction(SIG_IGN, &SIGINT_action, NULL);

    //signal handler to redirect ^Z
    SIGTSTP_action.sa_handler = &handler_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    //Commands struct Pointer
    Commands* ptrCMDS;
    //Allocate memory to point Commands struct
    ptrCMDS = (Commands*)malloc(sizeof(Commands));

    //Initialize Commands struct members
    init_Commands_List(ptrCMDS);

    while (1) {
        ((ptrCMDS)->numArgs) = 0;
        //reset struct member values
        ptrCMDS->is_background_process = 0;
        
        //GET USER INPUT
        get_user_input(ptrCMDS);
        fflush(stdout);

        // ------------ After getting user input, check for BUILT-IN COMMANDS ------------------------------
        // If it matches a built-in command, execute the built-in command
        // If there is no match, execute other commands via fork() child process and exec_other_commands().
        
        // --------------BUILT-IN COMMANDS--------------
        // Check if inputed arguments have a "#" as the first argment
        // Check if first argument is not NULL
        if (ptrCMDS->inputArgs[0] == NULL || strncmp(ptrCMDS->inputArgs[0], "#", 1) == 0) {
            //do nothing
        }
        // Check user input for the "status" command
        else if (strcmp(ptrCMDS->inputArgs[0], "status") == 0) {
            //call check_status
            //check_status(processStatus); --------------------------------------
            check_status(ptrCMDS);
            fflush(stdout);
        }
        //check user input for the "exit" command
        else if (strcmp(ptrCMDS->inputArgs[0], "exit") == 0) {
            //Set exitStatus flag to true
            ptrCMDS->exitStatus = true;
            //Proceed to cleanup allocated memory for Commands struct
            //and kill background processes
            if (ptrCMDS->exitStatus) {
                delete_commands(ptrCMDS);
                //clean up any background processes exit the shell
                kill_background_processes(ptrCMDS);
            }
            free(ptrCMDS);
            //Exit program
            exit(EXIT_SUCCESS);
        }
        //check user input for the "cd" command
        else if (strcmp(ptrCMDS->inputArgs[0], "cd") == 0) {
            cd_command(ptrCMDS);
        }
        else {
            //--------------Create child process ----------------------
            // fork Child Process and execute custom command
            // CHILD PROCESS
            pid = fork();
            //if there was an error forking the child process
            if (pid < 0) {
                //print an error message, clean up and exit
                perror("Error!/n");
                //Kill any background processes
                kill_background_processes(ptrCMDS);
                //Set process status
                ptrCMDS->processStatus = 1;
                break;
            }
            //instructions for the child process
            else if (pid == 0) {
                  //if this is a foreground process
                if (ptrCMDS->is_background_process == 0) {
                    // change to default signal handling
                    SIGINT_action.sa_handler = SIG_DFL;
                    SIGINT_action.sa_flags = 0;
                    sigaction(SIGINT, &SIGINT_action, NULL);
                }
                // EXECUTE Redirection and other commands via child process
                exec_other_commands(ptrCMDS);
            }
            //--------For the parent process -------------
            else {
                //if this is a foreground process
                if (ptrCMDS->is_background_process == 0) {
                    //wait for the process to complete				
                    pid = waitpid(pid, &(ptrCMDS->processStatus), 0);
                    //if process was terminated, print an error 
                    //message with terminating signal
                    if (WIFSIGNALED(ptrCMDS->processStatus)) {
                        printf("terminated by signal %d\n", ptrCMDS->processStatus);
                        fflush(stdout);
                    }
                }
                // If this is a background process
                if (ptrCMDS->is_background_process == 1) {
                    //do not wait for the process to complete
                    waitpid(pid, &(ptrCMDS->processStatus), WNOHANG);
                    //add PID to background PID array for later
                    ptrCMDS->background_processes[ptrCMDS->bg_procs_count] = pid;
                    //print PID and increment background PID
                    printf("background pid is %d\n", pid);
                    fflush(stdout);
                    //Increment count of background processes 
                    ptrCMDS->bg_procs_count++;
                }
            }
        }
        //Reset inputArgs to 0
        reset_inputArgs(ptrCMDS);
        //Monitor any child background processes that have completed
        //WNOHANG, continue without waiting and proceed to the next loop
        //Where parent process waits for any terminated child process.
        pid = waitpid(-1, &(ptrCMDS->processStatus), WNOHANG);
        while (pid > 0) {
            //if process completes normally
            //print the PID and exit value 
            if (WIFEXITED(ptrCMDS->processStatus) != 0 && pid > 0) {
                printf("background pid %d is done: exit value %d\n", pid, ptrCMDS->processStatus);
                fflush(stdout);
            }
            //If process was terminated, then output respective PID and signal number
            else {
                printf("background pid %d is done: terminated by signal %d\n", pid, ptrCMDS->processStatus);
                fflush(stdout);
            }
            //Continue monitoring for any child process that terminates
            pid = waitpid(-1, &(ptrCMDS->processStatus), WNOHANG);
        }
    }
    return 0;
}




