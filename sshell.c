#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CMDLINE_MAX 512
#define MAX_ARGS 16
#define MAX_CMDLENGTH 32
#define MAX_CMDS 4

//Struct to represent a command
struct myCmdObj {
    char cmdName[MAX_CMDLENGTH];
    char *cmdOptions[MAX_ARGS+1]; //args for each cmd
    int cmdOptionsCount;
    int redirectionType; //0 = no redir; 1 = std out; 2 = std out & err
    char redirectionFileName[MAX_CMDLENGTH];  
    int pipeStdErr; //if piping std error in addition to std out
};

//Hold info for each command
struct myCmdObj commandsObj[MAX_CMDS] = {0};

//Reset and free memory for commandObj struct
void resetCommandObj()
{
        for(int i=0; i<MAX_CMDS; ++i) {
                for(int j=0; j<commandsObj[i].cmdOptionsCount; ++j) {
                        if(commandsObj[i].cmdOptions[j] != NULL)
                                free(commandsObj[i].cmdOptions[j]);
                        commandsObj[i].cmdOptions[j] = NULL;
                }
                //terminate cmdOptions with NULL ptr for execvp()
                commandsObj[i].cmdOptions[MAX_ARGS] = NULL;

                memset(commandsObj[i].cmdName, '\0', MAX_CMDLENGTH);
                memset(commandsObj[i].redirectionFileName, '\0', MAX_CMDLENGTH);
                commandsObj[i].cmdOptionsCount = 0;
                commandsObj[i].redirectionType = 0;
                commandsObj[i].pipeStdErr = 0;
        }
}

//Parsing and tokenizing a command and its options
void parseCommand(char *cmd, int cmdCounter) 
{
        int isFirstToken = 1;
        int cmdOptionCounter = 0;
        char *token = strtok (cmd," ");

        while (token != NULL && cmdOptionCounter <= MAX_ARGS) {
                if(isFirstToken){
                        strcpy(commandsObj[cmdCounter].cmdName,token);
                        isFirstToken = 0;
                }
                commandsObj[cmdCounter].cmdOptions[cmdOptionCounter] = 
                        (char *) malloc(MAX_CMDLENGTH * sizeof(char));
                strcpy(commandsObj[cmdCounter].cmdOptions[cmdOptionCounter],token);
                
                cmdOptionCounter++;
                token = strtok(NULL, " ");
        }
        if (cmdOptionCounter > MAX_ARGS)
                commandsObj[cmdCounter].cmdOptionsCount = MAX_ARGS+1;

        //Save the number of options for this command
        commandsObj[cmdCounter].cmdOptionsCount = cmdOptionCounter;
}

//Parsing for redirection, removes redirection file name from command
void findRedirection(char *cmd, int cmdCount, int cmdSize)
{
        char *redirfile; //file to redirect to
        int redirtype = 0; //0 if no redirect, 1 if stdout, 2 if stderr

        char *isRedirect = strstr(cmd, ">"); //extracts part begining with >
        int toredirect = strcspn(cmd, ">"); //finds location of >

        if (isRedirect) {
                if (isRedirect[1] == '&') {
                        redirtype = 2; //redirect std out and err
                        redirfile = strtok(isRedirect, " >&"); //extact filename
                } else {
                        redirtype = 1; //redirect std out only
                        redirfile = strtok(isRedirect, " >"); //extract filename
                }

                if (redirfile != NULL)
                    strcpy(commandsObj[cmdCount].redirectionFileName,redirfile);
                commandsObj[cmdCount].redirectionType = redirtype;
                
                //new cmd is same as original cmd without redirection file
                char* tmpCmdHolder = calloc(cmdSize, sizeof(char));
                strncpy(tmpCmdHolder, cmd, toredirect);
                memset(cmd, '\0', cmdSize);
                strcpy(cmd, tmpCmdHolder);
                free(tmpCmdHolder);
        }
}

//Splits command line into multiple commands, separated by pipes
int findPipes(char *cmd, char **pipeCmds)
{
        int cmdCounter = 0;

        char *pipeToken = strtok (cmd,"|");

        while (pipeToken != NULL) {
                if (pipeToken[0] == '&') {
                        pipeToken++;
                        commandsObj[cmdCounter-1].pipeStdErr = 2;
                }
                pipeCmds[cmdCounter] = 
                        (char *) malloc(strlen(pipeToken) * sizeof(char));
                strcpy(pipeCmds[cmdCounter], pipeToken);

                cmdCounter++;
                pipeToken = strtok(NULL, "|");
        }
        return cmdCounter;
}

//Parses through the entire command line
int parseCmdLine(char *cmd)
{
        char *splitCmds[MAX_CMDS];

        int numCmds = findPipes(cmd, splitCmds);

        for (int i = 0; i < numCmds; i++) {
                findRedirection(splitCmds[i], i, strlen(splitCmds[i]));
                parseCommand(splitCmds[i], i);
        }
        return numCmds;
}

//Built-in cd 
void cmdCd(char* cmd)
{
        char *cdToken = strtok(cmd," ");
        cdToken = strtok (NULL, " ");  

        int returnValue = chdir(cdToken);

        if (returnValue == -1)
                fprintf(stderr, "Error: cannot cd into directory\n");
        fprintf(stderr, "+ completed '%s %s' [%d]\n", cmd, cdToken,
                (returnValue == -1) ? 1 : 0);
}

//Built-in sls
void cmdSls(void)
{
        DIR *currentDir;
        struct dirent *cwdEntry; //for a dir entry
        currentDir = opendir("."); //open cwd

        if (currentDir == NULL) {
                fprintf(stderr, "Error: cannot open directory\n");
                fprintf(stderr, "+ completed 'sls' [1]\n");
                exit(1);
        }
        struct stat sb; //for file info
        
        //cycle through cwd entries
        while ((cwdEntry = readdir(currentDir)) != NULL) {
                //ignore hidden directories
                if ((cwdEntry->d_name)[0] == '.')
                        continue;
                
                stat(cwdEntry->d_name, &sb); //get file info
                printf("%s (%lld bytes)\n", cwdEntry->d_name, sb.st_size);
        }
        closedir(currentDir);
        fprintf(stderr, "+ completed 'sls' [0]\n");
}

//Redirect output as needed
void redirectStream(int redirType, int cmdNum)
{
        if (redirType != 0) {
                int fd = open(commandsObj[cmdNum].redirectionFileName
                        , O_WRONLY | O_CREAT | O_TRUNC, 0644);
                 
                dup2(fd, STDOUT_FILENO); //redirect std out 

                if (redirType == 2)
                        dup2(fd, STDERR_FILENO); //redirect std err
                
                close(fd);
        }
}

//Set up pipes between commands
void setupPipes(int cmdNum, int totalCmds, int fds[][2])
{
        //child needs to write stdout/std err to a pipe 
        if (cmdNum < totalCmds - 1) {
                close(fds[cmdNum][0]);
                dup2(fds[cmdNum][1], STDOUT_FILENO);
                if (commandsObj[cmdNum].pipeStdErr == 2)
                        dup2(fds[cmdNum][1], STDERR_FILENO);
                close(fds[cmdNum][1]);
        }

        //child needs to read stdin from a pipe
        if (cmdNum > 0) {
                close(fds[cmdNum-1][1]);
                dup2(fds[cmdNum-1][0], STDIN_FILENO);
                close(fds[cmdNum-1][0]);
        }

        //close other open/irrelevant file descriptors
        for (int i = 0; i < cmdNum; i++) {
                close(fds[i][0]);
                close(fds[i][1]);
        }
}

//Main function for parsing errors
int checkErrors(int totalCmds, char* cmd)
{
        //cmd starts with a pipe
        if (cmd[0] == '|') {
                fprintf(stderr, "Error: missing command\n");
                return 1;
        }

        for (int i = 0; i < totalCmds; i++) {
                if (commandsObj[i].redirectionType > 0 && 
                        commandsObj[i].cmdOptions[0] == NULL) {
                        fprintf(stderr, "Error: missing command\n");
                        return 1;
                } else if (commandsObj[i].cmdOptionsCount > MAX_ARGS) {
                        fprintf(stderr, "Error: too many process arguments\n");
                        return 1;
                } else if (commandsObj[i].redirectionType > 0 &&
                        commandsObj[i].redirectionFileName[0] == '\0') {
                        fprintf(stderr, "Error: no output file\n");
                        return 1;
                } else if (commandsObj[i].redirectionType > 0 && 
                        i != totalCmds-1) {
                        fprintf(stderr, "Error: mislocated output redirection\n");
                        return 1;
                } else if (commandsObj[i].cmdOptions[0] == NULL) {
                        fprintf(stderr, "Error: missing command\n");
                        return 1;
                } else if (i == totalCmds-1 && commandsObj[i].redirectionType > 0) {
                        //try opening file
                        int fd = open(commandsObj[i].redirectionFileName
                                , O_WRONLY | O_CREAT | O_TRUNC, 0644);

                        if (fd == -1) {
                                fprintf(stderr, "Error: cannot open output file\n");
                                return 1;
                        }
                        close(fd);
                }
        }

        //if pipes are missing commands between them
        if (strstr(cmd, "||") != NULL || strstr(cmd, "|||") != NULL) {
                fprintf(stderr, "Error: missing command\n");
                return 1;
        }

        //if command ends with a pipe
        char* lastPipe = strrchr(cmd, '|');
        if (lastPipe != NULL && lastPipe[1] == '\0') {
                fprintf(stderr, "Error: missing command\n");
                return 1;
        }

        return 0; //no errors
}


int main(void)
{
        char cmd[CMDLINE_MAX];

        while (1) {
                char *nl;

                /* Print prompt */
                printf("sshell$ ");
                fflush(stdout);

                /* Get command line */
                fgets(cmd, CMDLINE_MAX, stdin);

                /* Print command line if stdin is not provided by terminal */
                if (!isatty(STDIN_FILENO)) {
                        printf("%s", cmd);
                        fflush(stdout);
                }

                /* Remove trailing newline from command line */
                nl = strchr(cmd, '\n');
                if (nl)
                        *nl = '\0';

                // Builtin commands
                if (!strcmp(cmd, "exit")) {
                        fprintf(stderr, "Bye...\n");
                        fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
                        break;
                } else if (!strcmp(cmd, "pwd")) {
                        char *cwd = getcwd(NULL,0);
                        fprintf(stdout, "%s\n",cwd);
                        fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
                        continue;
                } else if (cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' ' ) {
                        cmdCd(cmd);
                        continue;
                } else if (cmd[0] == 's' && cmd[1] == 'l' && cmd[2] == 's') {
                        cmdSls();
                        continue;
                }

                //Leave original cmd unmodified during parsing
                char cmdForParsing[CMDLINE_MAX] = {0};
                strcpy(cmdForParsing, cmd);

                int numOfCmds = parseCmdLine(cmdForParsing);

                //If blank command line, do nothing
                if(numOfCmds <= 1 && commandsObj[0].redirectionType == 0 &&
                        strstr(cmd, "|") == NULL && 
                        commandsObj[0].cmdOptions[0] == NULL) {
                        continue;
                }

                //Checking for parsing errors
                if (checkErrors(numOfCmds, cmd)) {
                        resetCommandObj();
                        continue;
                }
                
                //Start forking
                int fds[numOfCmds-1][2]; //n commands = n-1 pipes
                pid_t pids[numOfCmds]; //n pids

                //Create pipes
                for (int i = 0; i < numOfCmds; i++) {
                        pipe(fds[i]);
                }

                for (int i = 0; i < numOfCmds; i++) {
                        //For each piped cmd, fork and then save child pid
                        pids[i] = fork();
                        
                        if (pids[i] == 0) {
                                //Child
                                setupPipes(i, numOfCmds, fds);
                                redirectStream(commandsObj[i].redirectionType, i);

                                execvp(commandsObj[i].cmdName, 
                                        commandsObj[i].cmdOptions);
                                
                                fprintf(stderr, "Error: command not found\n");
                                exit(1);
                        } else if (pids[i] > 0) {
                                //Parent
                                continue;
                        } else {
                                perror("Forking error\n");
                                exit(1);
                        }
                }
                
                //Close all file descriptors in parent
                for (int i = 0; i < numOfCmds-1; i++) {
                        close(fds[i][0]);
                        close(fds[i][1]);
                }

                //Wait for each child to finish
                int status;
                int statuses[numOfCmds];
                for (int i = 0; i < numOfCmds; i++) {
                        waitpid(pids[i], &status, 0);
                        statuses[i] = WEXITSTATUS(status);
                }

                //Print completetion message
                fprintf(stderr, "+ completed '%s' ", cmd);
                for (int i = 0; i < numOfCmds; i++) {
                        fprintf(stderr, "[%d]", statuses[i]);
                }
                fprintf(stderr, "\n");

                //Reset command object struct
                resetCommandObj();
        }
        return EXIT_SUCCESS;
}
