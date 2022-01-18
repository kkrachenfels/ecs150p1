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
char* FindRedirection(char *cmd, int cmdCount, int cmdSize)
{
        char *redirfile;        //file to redirect to
        char *newcmd = malloc(cmdSize * sizeof(char)); //cmd without file part
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
                strncpy(newcmd, cmd, toredirect);  //copy chars up to the > 
        }
        else strcpy(newcmd, cmd); //cmd is unmodified

        return newcmd;
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

        char *tempCmdHolder;

        for (int i = 0; i < numCmds; i++)
        {
                tempCmdHolder = FindRedirection(splitCmds[i], i, strlen(splitCmds[i]));
                ParseCommand(tempCmdHolder, i);
                free(tempCmdHolder);
        }
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
                char cmdForParsing[CMDLINE_MAX];
                strcpy(cmdForParsing, cmd);

                int numOfCmds = ParseCmdLine(cmdForParsing);

                pid_t pid; //declare pid
                int fd[2];

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
                // if it is a pipe command
                /*if(commandsObj[0].isPipeCmd == 1) {
                        pipe (fd);
                }*/
                
                pid = fork(); //fork off new process
                //child
                if (pid == 0) {
                        int redirection = commandsObj[0].redirectionType;
                        //printf("redirection status: %d\n", redirection);
 
                        if (redirection != 0)
                        {
                                //open file to redirect to
                                int fd = open(commandsObj[0].redirectionFileName
                                        , O_WRONLY | O_CREAT | O_TRUNC, 0644);
                                
                                dup2(fd, STDOUT_FILENO); //redirect std out 
                                
                                if (redirection == 2)
                                {
                                        dup2(fd, STDERR_FILENO); //redirect std err
                                }
                                close(fd);

                        }
        
                        execvp(commandsObj[0].cmdName, commandsObj[0].cmdOptions); //use execvp (the -p specifies to use $PATH)
                        perror("error in execv"); //if the exec doesn't work
                        exit(1);
                        
                } 
                else if (pid > 0) {
                        //parent
                        // if pipe cmd left side
                        int status;
                        waitpid(pid, &status, 0); //wait for child process to finish exec
                        fprintf(stderr, "+ completed '%s' [%d]\n",
                                cmd, WEXITSTATUS(status)); //print exit status to stderr
                        
                }
                else {
                        perror("forking error");
                        exit(1);
                }

                // Reset command object struct.
                ResetCommandObj();
                //free(newcmd);

        }

        return EXIT_SUCCESS;
}