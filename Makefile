sshell: sshell.o
	gcc -Wall -Wextra -Werror -o sshell sshell.o

clean:
	rm -f sshell sshell.o


