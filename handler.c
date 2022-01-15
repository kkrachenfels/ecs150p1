void handler(int signum)
{
    int c;
    read(STDIN_FILENO, &C, 1);
    fprint(stderr, "%c\n", c++);
}

int main(void)
{
    int c = 'a';
    int fd[2];
    struct sigaction sa = {0};

    pipe(fd);
    
}