## Project 1 Report
#### By Kay Krachenfels & Aliya Abbas

### Overview
The goal of this project was to implement a simpler shell called   
sshell.
Our shell takes commands from the user and executes them based on
the arguments given, determining whether they are built-in 
commands, commands
that include redirection, or piped commands. 
To set basic program constraints, we declared macros `CMDLINE_MAX`,
 `MAX_ARGS`, 
`MAX_CMDLENGTH`, `MAX_CMDS`, and set them to their respective 
lengths.

### Builtin Commands 
After getting the command line from the user, built-in commands 
`exit`, `pwd`, `cd`, and `sls` could be implemented with the 
corresponding system calls:

```
pwd -> getcwd()
cd -> chdir()
sls -> opendir(), readdir()
exit -> exit()
```

We implemented `pwd` and `exit` simply in the main function, but 
put `cd` and `sls` in their own functions as they were slightly 
more extensive.

For `cd` we used `strtok()` to extract the directory name for 
`chdir()`, and for`sls` we used the dirent and stat structs so 
that information returned from the syscalls could be temporarily 
stored and then printed.

Error checking for `cd` and `sls` was also implemented, using 
return values from the syscalls to ensure that a directory could 
be accessed and/or read.


### Regular Command Parsing
For commands that would not be built-in, we parsed and tokenized 
the command 
line. Most of the command line parsing was done with string 
library functions, particularly `strtok()`. 

Because of this, we pass a *copy* of the original command string 
to our parser functions; we learned the hard way that `strtok` 
could sometimes unhelpfully modify original command, unlike most 
other string library functions, and wanted to parse a copy of the 
command that we didn't mind being modified while keeping the 
original command untouched for eventual completion status messages.

For the parsing itself, we have a main parser function, 
`parseCmdLine()`, where we then call other, sub-parser functions, 
to simplify the parsing process. Inside `parseCmdLine()`  we 
methodically parsed the command line in steps, first by searching 
for any pipe characters '|' and splitting the line into multiple 
commands with the function `findPipes()`. 
Then, with `findRedirection()`, we parsed each command for any 
redirection as indicated by the '>' or '>&' symbols. We extracted 
the redirection type in addition to the redirection file and 
stored these the command object.
Finally, we parsed the remaining part of the command (without any 
redirection file name) with `parseCommand()`. This function copies 
remaining information for the command to its respective command 
object, dynamically allocating space for the command arguments as 
necessary.

Following parsing, we checked for errors in the `checkErrors()` 
function. With all the information stored in the command objects, 
most error checking could be done by inspecting each command 
object and checking that it contained all the information it 
needed to execute a command. For example, if the command object 
indicated redirection type was 1 (redirect std out) but its 
filename was empty, we knew to throw a `no output file` error. Or 
if the first element of the command arguments array (`cmdOptions[0]
`, AKA the name of the command itself) was empty, we could then 
throw a `missing command` error.

Eventually, after the completion of one command line (success or 
failure) we would call the `ResetCommandObj()` function, which 
frees all the memory buffers. We would clear all the memory 
buffers that were used and reset all the counts/non-array 
variables to 0 again to make it ready to parse the next command 
line.  



### Command Structure
As mentioned in the parsing section, the command object `myCmdObj` 
contains all the information necessary to execute a command. 
The command name and command options are stored; `cmdName` is 
staticalloy allocated based on program constraints, while we left 
the array of `cmdOptions`, for command options/arguments, to be 
dynamically allocated. This is because commands could vary wildly 
in the number of arguments passed to the command, and staticlly 
allocating space for the maximum sixteen possible arguments would 
likely be a waste of memory most times. 
Other information the command object contains are variables 
checking if there is redirection, the file name for redirection, 
and certain piping conditions.

To track all the commands in a command line, we have an array of 
`myCmdObj` called `struct myCmdObj commandsObj[MAX_CMDS] = {0}` 
that stores all the parsed commands.

For example if the user types `ls -a | wc`, commandsObj[0] would 
store `ls -l` and commandsObj[1] would store `wc`. 



### Executing Shell Commands (fork(), exec(), and pipe())

Based on the number of commands/pipes we got on one command line, 
we create the pipes and `fork()` off the desired number of 
children processess.
The number of pipes would always be the number of commands, minus 
1.
To prepare for this we created two arrays, one for tracking open 
file destriptors to pipes (`int fds[numOfCmds-1][2]`), and one for 
tracking all the children's pids (`pid_t pids[numOfCmds]`). The 
idea to then use a loop for forking multiple children and tracking 
their forked pids in an array was taken from the top reply to 
[this post](https://stackoverflow.com/questions/1381089/
multiple-fork-concurrency).

Additionally, we ran a couple of our own functions within the 
forking loop. The `setupPipes()` function was used to close and 
copy file descriptors for each child's pipes as necessary. The 
logic we used here was that every child/command except the last 
child/command in a pipeline would need to redirect its standard 
output to a pipe (`if (cmdNum < totalCmds - 1)`), and every child/
command except the first child/command would need to redirect its 
standard input to be from a pipe (`if (cmdNum > 0)`). Also, since 
our method of looping and forking ended up opening each pipe in 
each child, we had to then close all irrelevant file descriptors 
to pipes to prevent hanging. We also wrote the `redirectStream()` 
function to be run after pipes were set up to further redirect any 
output on the final command of a pipeline - calling `dup2()` on 
only STDOUT_FILENO or also on STDERR_FILENO if error was being 
redirected as well. Finally, we would have everything set up to 
execute each command in its respective child with the `execvp()` 
syscall.

Meanwhile, the parent process waits for each command to finish and 
checks its 
exit status. We also had to close all the open file descriptors in 
the parent process to prevent hanging, before calling `waitpid()` 
on any children. Since we had stored all the children pids in an 
array, we simply looped through this array and waited on all the 
children, collecting their exit statuses in another array and then 
looping through that to print the completion statement. Then the 
command object could be reset and we'd take in a new command and 
go for another loop again!
