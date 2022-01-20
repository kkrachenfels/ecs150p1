## Project 1 Report
#### By Kay Krachenfels & Aliya Abbas

### Overview
The goal of this project was to implement a shell called sshell, our shell takes commands from the user and executes them based on the arguments given, wether they are built-in commands, commands that include redirection, or piped commands. 
To assure the commands typed by the user weren’t more than 512 characters, there were no more than 16 arguments given, and the length of a single token wasn’t more than 32 characters, we declared  global variables `CMDLINE_MAX`, `MAX_ARGS`, `MAX_CMDS`, `MAX_CMDLENGTH` and set them to their respective lenghts.

### Builtin Commands 
We started by getting the command from the user, then worked on implementing built-in commands such as exit, pwd, and cd. Each of these functions were implemented by calling system calls, the corresponding system call for each function are as follows:  
```
pwd -> getcwd()
cd -> chdir()
sls -> opendir(), readdir()
exit -> exit()
```


### Command Parsing
Once we detected that it was a built-in command we parsed the command line for regular commands and tokenized them (split into tokens). Inside `ParseCmdLine()`  we find what kind of command it is and we check for things like redirection, pipe, and errors.

For example in `ParseCmdLine()` we parse and check for redirection then set flags accordingly. We also do error checking here, using the `FindPipeError` this function is used to find command line errors for piped commands. we search for if the command has > symbol and | symbol in the same string then we give an error message. We check to see if there is anything on the right side of the pipe and if it is NULL then it means theres a missing command. We also do other error checking to see if the first charecter of the string has a pipe symbol then it means theres no command on the left side so we issue an error message.

For parsing the chain of pipe commands we first split/tokenize the command line based on the pipe symbol and keep tokenizing based on the pipe symbol and keep storing each chain of command.  
The last thing we check for in pipe command is we search for the right side of the command and if we cant find the right sife then we tokenize again.  

When we initially parse a command line from the user we also use dynamically alloacted string buffers inside command object structures based on the number of commands and options

After the completion of one command line (success or failure)
we call the `ResetCommandObj()` function, this function returns all the memory buffers.  
We clear all the memory buffers that were used and reset all the caount to 0 again to make it ready to parse the next command line again.  



### Command Structure
Once we know that there are no errors in redirection and piping, we split the command. Each command is stored in `myCmdObj` here the command name and command options are stored. `cmdName` tells us the command name, for example if it is ls, `cmdOptions` tells us the command options for example if its ls -l
we also find things like if theres redirection, the file name for redirection, 
and piping.

We also have an array of `myCmdObj` called `struct myCmdObj commandsObj[MAX_CMDS] = {0}` that stores all the parsed commands.  
for example if the user types ls -a | wc  
commandsObj[0] = ls -l  
commandsObj[1] = wc  




### Executing Shell Commands (fork exec)

based on the number of commands we have we create equivelant child processess and we have to create the same number of pipes. 
for each piped command we fork that many child proccess and save their process id's.
we create and setup pipes in the function `setupPipes()` funtion
we also check if the command strucure has a redirection type 
we call the `redirectStream()` function to setup redirection based on the arguments that were passed.

`redirectStream()` sets up the standard input, output, and error based off the redirection field that was set.  
the actual command is executed in the child process using the system call execvp  
the parent process waits for each command to finish and checks its exit status.





