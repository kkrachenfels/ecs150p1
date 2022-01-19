#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define CMD_MAX 2


//test pipeline
//ls | grep "shell" | head -n 6 | grep "sshell"


/*char *cmds[CMD_MAX] = {"ls", "grep", "head", "grep"};
char *cmdargs[CMD_MAX][CMD_MAX] = {{"ls", NULL, NULL, NULL}, 
	{"grep", "shell", NULL, NULL}, 
	{"head", "-n", "6", NULL}, {"grep", "sshell", NULL, NULL}};*/

char *cmds[CMD_MAX] = {"ls", "grep"};
char *cmdargs[CMD_MAX][3] = {{"ls", "nonexistent_file", NULL}, {"grep", "shell", NULL}};



int main(void)
{
	//arr of groups of 2 fds for piping
	int fds[CMD_MAX-1][2];	//n-1 pipes

	//arr of forked pids to track
	pid_t pids[CMD_MAX] = {0};

	
	//set up pipes
	for (int i = 0; i < CMD_MAX-1; i++)
	{
		pipe(fds[i]);
	}


	for (int i = 0; i < CMD_MAX; i++)
	{
		//for each piped cmd, fork and then save child pid
		pids[i] = fork();
		if (pids[i] != 0)
		{
			printf("Parent here. Pid: %d. Just forked off child: %d\n",
				getpid(), pids[i]);
		}
		else
		{
			printf("Child here. Pid = %d, parent = %d. i = %d\n", 
				getpid(), getppid(), i);
			
			//child needs to write stdout to a pipe 
			if (i < CMD_MAX - 1)
			{
				close(fds[i][0]);	//don't need read access to pipe
				dup2(fds[i][1], STDOUT_FILENO);	//replace stdout with pipe
				dup2(fds[i][1], STDERR_FILENO);
				close(fds[i][1]);	//close now unused fd
			}

			//child needs to read stdin from a pipe
			if (i > 0)
			{
				close(fds[i-1][1]);	//don't need write access to pipe
				dup2(fds[i-1][0], STDIN_FILENO); //replace stdin with pipe
				close(fds[i-1][0]); //close now unused fd
			}

			/*close all other pipelines since each child has fds to
			pipelines it doesn't need*/
			for (int j = 0; j < i; j++)
			{
				close(fds[j][0]);
				close(fds[j][1]);
			}

			//exec piped cmd
			execvp(cmds[i], cmdargs[i]);
			
			//if the exec doesn't work
			perror("error in execv"); 
            exit(1);
		}
	}

	//close fds to pipe in parent shell 
	for (int i = 0; i < CMD_MAX-1; i++)
	{
		close(fds[i][0]);
		close(fds[i][1]);
	}

	//wait for each child to finish executing
	int status;
	for (int i = 0; i < CMD_MAX; i++)
	{
		waitpid(pids[i], &status, 0);
		fprintf(stderr, "child [%d] completed, status: %d\n",
        	pids[i], WEXITSTATUS(status)); 
	}
	fprintf(stderr, "all children completed and exited!!\n");
	

}