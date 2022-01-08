#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CMDLINE_MAX 512

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
                }

                /* Regular command */
                /*retval = system(cmd);
                fprintf(stdout, "Return status value for '%s': %d\n",
                        cmd, retval); */


                //replacing code above:

                pid_t pid; //declare pid
                //no real args for now (replace second NULL in phase 2):
                char *args[] = {cmd, NULL, NULL};

                pid = fork(); //fork off new process
                if (pid == 0) {
                        //child
                        execvp(cmd, args); //use execvp (the -p specifies to use $PATH)
                        perror("error in execv"); //if the exec doesn't work
                        exit(1);
                } 
                else if (pid > 0) {
                        //parent
                        int status;
                        waitpid(pid, &status, 0); //wait for child process to finish exec
                        printf("Return status value for '%s': %d\n",
                                cmd, WEXITSTATUS(status));
                }
                else {
                        perror("forking error");
                        exit(1);
                }

        }

        return EXIT_SUCCESS;
}