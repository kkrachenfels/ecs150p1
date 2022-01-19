#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define CMDLINE_MAX 512
#define MAX_ARGS 16
#define MAX_CMDS 4
#define MAX_CMDLENGTH 32

// struct to represent a command.
struct myCmdObj {
    char cmdName[MAX_CMDLENGTH];      // cmd name i.e. ls
    char *cmdOptions[MAX_ARGS+1];  // cmd options i.e.  ls -l
    int cmdOptionsCount;
    int redirectionType; //0 = no redir; 1 = std out; 2 = std out & err
    char redirectionFileName[MAX_CMDLENGTH];  // redirection file name.
    int isPipeCmd; //if pipe cmd
    //int status;        //seems like we don't need this for now               
};

// maximum of 4 commands.
struct myCmdObj commandsObj[MAX_CMDS] = {0};


// Reset and free memory for commandObj struct.
void ResetCommandObj()
{
        for(int i=0; i<MAX_CMDS; ++i) {
                for(int j=0; j<commandsObj[i].cmdOptionsCount; ++j) 
                {
                        if(commandsObj[i].cmdOptions[j] != NULL){
                                free(commandsObj[i].cmdOptions[j]);
                        }
                        commandsObj[i].cmdOptions[j] = NULL;
                }
                //terminate cmdOptions with NULL ptr for execvp()
                commandsObj[i].cmdOptions[MAX_ARGS] = NULL;

                commandsObj[i].cmdOptionsCount = 0;
                memset(commandsObj[i].cmdName, '\0', MAX_CMDLENGTH);    

                commandsObj[i].redirectionType = 0;
                memset(commandsObj[i].redirectionFileName, '\0', MAX_CMDLENGTH);
        }
}



//Parsing for redirection, removes redirection file name from command */
void FindRedirection(char *cmd, int cmdCount, int cmdSize)
{
        char *redirfile;        //file to redirect to
        int redirtype = 0; //0 if no redirect, 1 if stdout, 2 if stderr

        char *isRedirect = strstr(cmd, ">");    //extracts part begining with >
        int toredirect = strcspn(cmd, ">");     //finds location of >

        //if > is found (not null)
        if (isRedirect) 
        {
                //if it is >&
                if (isRedirect[1] == '&') 
                {
                        redirtype = 2; //redirect std out and err
                        redirfile = strtok(isRedirect, " >&"); //extact filename
                }
                //if it is only >
                else
                {
                        redirtype = 1; //redirect std out only
                        redirfile = strtok(isRedirect, " >"); //extract filename
                }

                strcpy(commandsObj[cmdCount].redirectionFileName,redirfile);
                commandsObj[cmdCount].redirectionType = redirtype;
                
                //new cmd is same as original cmd without the redirection and file
                char* tmpCmdHolder = calloc(cmdSize, sizeof(char));
                strncpy(tmpCmdHolder, cmd, toredirect);
                memset(cmd, '\0', cmdSize);
                strcpy(cmd, tmpCmdHolder);
                free(tmpCmdHolder);

                //strncpy(newcmd, cmd, toredirect);  //copy chars up to the > 
        }
        //else strcpy(newcmd, cmd); //cmd is unmodified

        //return newcmd;
}



//Parsing and tokenizing a command and its options
void ParseCommand(char *cmd, int cmdCounter) 
{
        int isFirstToken = 1;
        int cmdOptionCounter = 0;
        char *token = strtok (cmd," ");

        while (token != NULL && cmdOptionCounter <= MAX_ARGS)
        {
                //printf ("PRINTING current token: %s\n",token);
                if(isFirstToken){
                        // first token is the command name.
                        strcpy(commandsObj[cmdCounter].cmdName,token);
                        isFirstToken = 0;
                } 
                
                commandsObj[cmdCounter].cmdOptions[cmdOptionCounter] = (char *) malloc(MAX_CMDLENGTH * sizeof(char));
                strcpy(commandsObj[cmdCounter].cmdOptions[cmdOptionCounter],token);
                cmdOptionCounter++;

                token = strtok(NULL, " ");
        }
        //will check this for error code later
        if (cmdOptionCounter > MAX_ARGS)
                commandsObj[cmdCounter].cmdOptionsCount = MAX_ARGS+1;

        // save the number of options for this command
        commandsObj[cmdCounter].cmdOptionsCount = cmdOptionCounter;
}


//Splits command line into multiple commands, separated by pipes
int FindPipes(char *cmd, char **pipeCmds)
{
        int cmdCounter = 0;

        char *pipeToken = strtok (cmd,"|");

        while (pipeToken != NULL)
        {
                pipeCmds[cmdCounter] = (char *) malloc(strlen(pipeToken) * sizeof(char));
                strcpy(pipeCmds[cmdCounter], pipeToken);
                //printf("Current pipe token: %s\n", pipeCmds[cmdCounter]);
                cmdCounter++;
                pipeToken = strtok(NULL, "|");
        }
        return cmdCounter;
}


//Parses through the entire command line
int ParseCmdLine(char *cmd)
{
        char *splitCmds[MAX_CMDS];

        int numCmds = FindPipes(cmd, splitCmds);

        for (int i = 0; i < numCmds; i++)
        {
                FindRedirection(splitCmds[i], i, strlen(splitCmds[i]));
                ParseCommand(splitCmds[i], i);
        }
        //for debugging, to make sure everything was parsed correctly
        /*for (int i = 0; i < numCmds; i++)
        {
                printf("Command [%d]: %s", i, commandsObj[i].cmdName);
                for (int j = 1; j < commandsObj[i].cmdOptionsCount; j++)
                {
                        printf(" %s", commandsObj[i].cmdOptions[j]);
                }
                printf("\n Redirection type: %d and filename: %s\n",
                        commandsObj[i].redirectionType, 
                        commandsObj[i].redirectionFileName);
        }*/

        return numCmds;
}


// Phase 3, Implement Built-in commands cd in a shell. 
void CmdInShellCd(char* cmd)
{
        char *cdToken = strtok(cmd," ");
        cdToken = strtok (NULL, " ");  

        int returnValue = chdir(cdToken);
        if (returnValue == -1)
                fprintf(stderr, "Error: cannot cd into directory\n");
        fprintf(stderr, "+ completed '%s %s' [%d]\n", cmd, cdToken, (returnValue == -1) ? 1 : 0);
}

//Phase 6, built-in sls
void CmdSls(void)
{
        DIR *currentDir;                 
        struct dirent *cwdEntry;        //for a dir entry
        currentDir = opendir(".");      //open cwd

        if (currentDir == NULL)
        {
                fprintf(stderr, "Error: cannot open directory\n");
                fprintf(stderr, "+ completed 'sls' [1]\n");
                exit(1);
        }

        struct stat sb;       //for file info
        
        //cycle through cwd entries
        while ((cwdEntry = readdir(currentDir)) != NULL)
        {
                //ignore hidden directories
                if ((cwdEntry->d_name)[0] == '.')
                        continue;
                
                stat(cwdEntry->d_name, &sb);  //get file info
                printf("%s (%lld bytes)\n", cwdEntry->d_name, sb.st_size); //print file info
        }
        closedir(currentDir);
        fprintf(stderr, "+ completed 'sls' [0]\n");
}


void redirectStream(int redirType, int cmdNum)
{
        if (redirType != 0)
        {
                //open file to redirect to
                int fd = open(commandsObj[cmdNum].redirectionFileName
                        , O_WRONLY | O_CREAT | O_TRUNC, 0644);
                                        
                dup2(fd, STDOUT_FILENO); //redirect std out 
                                        
                if (redirType == 2)
                        dup2(fd, STDERR_FILENO); //redirect std err
                
                close(fd);
        }  
}

void setupPipes(int cmdNum, int totalCmds, int fds[][2])
{
        /*printf("Child here. Pid = %d, parent = %d. i = %d\n", 
        getpid(), getppid(), cmdNum);   //debugging*/
                        
        //child needs to write stdout to a pipe 
        if (cmdNum < totalCmds - 1)
        {
                close(fds[cmdNum][0]); //don't need read access to pipe
                dup2(fds[cmdNum][1], STDOUT_FILENO); //replace stdout with pipe
                //dup2(fds[i][1], STDERR_FILENO);
                close(fds[cmdNum][1]);       //close now unused fd
        }

        //child needs to read stdin from a pipe
        if (cmdNum > 0)
        {
                close(fds[cmdNum-1][1]);     //don't need write access to pipe
                dup2(fds[cmdNum-1][0], STDIN_FILENO); //replace stdin with pipe
                close(fds[cmdNum-1][0]); //close now unused fd
        }

        /*close all other pipelines since each child has fds to
        pipelines it doesn't need*/
        for (int i = 0; i < cmdNum; i++)
        {
                close(fds[i][0]);
                close(fds[i][1]);
        }
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

                /* Builtin command */
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
                        CmdInShellCd(cmd);
                        continue;
                } else if (cmd[0] == 's' && cmd[1] == 'l' && cmd[2] == 's') {
                        CmdSls();
                        continue;
                }

                //so original cmd line remains unmodified for status printing
                char cmdForParsing[CMDLINE_MAX] = {0};
                strcpy(cmdForParsing, cmd);

                int numOfCmds = ParseCmdLine(cmdForParsing);

                //pid_t pid; //declare pid

                int maxargs = 0;
                for (int i = 0; i < numOfCmds; i++)
                {
                        if (commandsObj[i].cmdOptionsCount > MAX_ARGS)
                               maxargs = 1;
                }

                if (maxargs){
                        fprintf(stderr, "Error: too many process arguments\n");
                        ResetCommandObj();
                        //free(newcmd);
                        continue;
                }

                if(commandsObj[0].cmdName[0] == '\0'){
                        continue;
                }
                
                /*STARTING FORKING....*/
                int fds[numOfCmds-1][2]; //n commands = n-1 pipes
                pid_t pids[numOfCmds]; //n pids

                //create pipes
                for (int i = 0; i < numOfCmds; i++)
                {
                        pipe(fds[i]);
                }

                for (int i = 0; i < numOfCmds; i++)
                {
                        //for each piped cmd, fork and then save child pid
                        pids[i] = fork();
                        
                        //child
                        if (pids[i] == 0)
                        {
                                setupPipes(i, numOfCmds, fds);
                                redirectStream(commandsObj[i].redirectionType, i);

                                execvp(commandsObj[i].cmdName, commandsObj[i].cmdOptions); //use execvp (the -p specifies to use $PATH)
                                
                                perror("Error in execv"); //if the exec doesn't work
                                exit(1);
                        }
                        //parent
                        else if (pids[i] > 0)
                        {
                                continue;
                                //debugging
                                /*printf("Parent here. Pid: %d. Just forked off child: %d\n",
                                        getpid(), pids[i]);*/ 
                        }
                        else
                        {
                                perror("Forking error");
                                exit(1);    
                        }
                }
                
                for (int i = 0; i < numOfCmds-1; i++)
                {
                        close(fds[i][0]);
                        close(fds[i][1]);
                }

                //wait for each child to finish
                int status;
                int statuses[numOfCmds];
                for (int i = 0; i < numOfCmds; i++)
                {
                        waitpid(pids[i], &status, 0);
                        statuses[i] = WEXITSTATUS(status);
                }

                //print completetion message
                fprintf(stderr, "+ completed '%s'", cmd);
                for (int i = 0; i < numOfCmds; i++)
                {
                        fprintf(stderr, " [%d]", statuses[i]);
                }
                fprintf(stderr, "\n");
                

                // Reset command object struct.
                ResetCommandObj();

        }

        return EXIT_SUCCESS;
}