#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CMDLINE_MAX 512

// struct to represent a command.
struct myCmdObj {
    char cmdName[32];      // cmd name i.e. ls
    char *cmdOptions[16];  // cmd options i.e.  ls -l
    int cmdOptionsCount;
    char redirectionFileName[32];  // redirection file name.
    int status;               
};

// maximum of 16 commands.
struct myCmdObj commandsObj[16] = {0};

// Reset and free memory for commandObj struct.
void ResetCommandObj()
{
        for(int i=0; i<16; ++i) {
                for(int j=0; j<=commandsObj[i].cmdOptionsCount; ++j) 
                {
                        if(commandsObj[i].cmdOptions[j] != NULL){
                                free(commandsObj[i].cmdOptions[j]);
                        }
                        commandsObj[i].cmdOptions[j] = NULL;
                }

                commandsObj[i].cmdOptionsCount = 0;
                commandsObj[i].cmdName[0] = '\0';      
        }
}

// Phase 2, parsing and tokenizing the command line and the options.
void ParseCommandLine(char *cmd) 
{
        int isFirstToken = 1, isRedirection = 0;
        int cmdCounter = 0, cmdOptionCounter = 0;
        char *token = strtok (cmd," ");

        while (token != NULL)
        {
                printf ("PRINTING current token: %s\n",token);
                if(isFirstToken){
                        // first token is the command name.
                        strcpy(commandsObj[cmdCounter].cmdName,token);
                        commandsObj[cmdCounter].cmdOptions[cmdOptionCounter] = (char *) malloc(32 * sizeof(char));
                        strcpy(commandsObj[cmdCounter].cmdOptions[cmdOptionCounter],token);
                        isFirstToken = 0;
                        cmdOptionCounter++;
                } else {
                        // not first token, then command option.
                        if (!strcmp(token, ">")) {
                                isRedirection = 1;
                                printf("  Got Redirection\n");
                        } else if(isRedirection) {
                                //save redirection filename.
                                strcpy(commandsObj[cmdCounter].redirectionFileName,token);
                                isRedirection = 0;
                        }
                        else {
                                commandsObj[cmdCounter].cmdOptions[cmdOptionCounter] = (char *) malloc(32 * sizeof(char));
                                strcpy(commandsObj[cmdCounter].cmdOptions[cmdOptionCounter],token);
                                cmdOptionCounter++;
                        }
                }

                if (!strcmp(token, "|")) {
                        cmdCounter++;
                        printf("  Got Pipe: TODO.\n");
                        break;      
                } 

                token = strtok(NULL, " ");
        }
        // save the number of options for this command
        commandsObj[cmdCounter].cmdOptionsCount = cmdOptionCounter;
}

// Phase 3, Implement Built-in commands cd in a shell. 
void CmdInShellCd(char* cmd)
{
        char *cdToken = strtok(cmd," ");
        char *dirNameToken = strtok (NULL, " ");

        int returnValue = chdir(dirNameToken);
        fprintf(stderr, "+ completed '%s' [%d]\n", cmd, returnValue);
}

int main(void)
{
        char cmd[CMDLINE_MAX];

        while (1) {
                char *nl;
                int retval;

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
                        break;
                } else if (!strcmp(cmd, "pwd")) {
                        char *cwd = getcwd(NULL,0);
                        fprintf(stdout, "%s\n",cwd);
                        fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
                        continue;
                } else if (cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' ' ) {
                        CmdInShellCd(cmd);
                        continue;
                }

                ParseCommandLine(cmd);

                pid_t pid; //declare pid
                //no real args for now (replace second NULL in phase 2):
                if(commandsObj[0].cmdName[0] == '\0'){
                        continue;
                }
                pid = fork(); //fork off new process
                if (pid == 0) {
                        //child
                        //execvp(cmd, args);
                        execvp(commandsObj[0].cmdName, commandsObj[0].cmdOptions); //use execvp (the -p specifies to use $PATH)
                        perror("error in execv"); //if the exec doesn't work
                        exit(1);
                } 
                else if (pid > 0) {
                        //parent
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

        }

        return EXIT_SUCCESS;
}