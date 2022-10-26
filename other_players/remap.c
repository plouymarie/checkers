#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

char *args[1024];
char buffer[1024];
char filed1[1024];
char filed2[1024];
int in[2];
int out[2];
int n;

int main(int argc, char **argv)
{
    struct timeval to;
    fd_set ready;
    int i;

    if (pipe(in) == -1 || pipe(out) == -1)
    {
        fprintf(stderr, "Problems creating pipes.\n");
        exit(1);
    }
    
    /* Figure out the name of the program to launch. */
    sprintf(buffer, "%s.orig", argv[0]);
    sprintf(filed1, "%d", out[1]);
    sprintf(filed2, "%d", in[0]);

    /* Setup the argument list. */
    args[0] = buffer;
    args[1] = filed1;
    args[2] = filed2;
    for (i = 1; i < argc; i++)
    {
fprintf(stderr,"Arg: %s\t", argv[i]);
        args[i + 2] = argv[i];
    }
fprintf(stderr,"\n");
    args[i + 2] = NULL;

    if (fork() != 0)
    {
        /* Parent. */
        while (1)
        {
            FD_ZERO(&ready);
            FD_SET(out[0], &ready);
            FD_SET(fileno(stdin), &ready);
            to.tv_sec = 60;
            to.tv_usec = 0;
            if (select(out[0] + 1, &ready, NULL, NULL, &to) < 0)
                continue;
memset(buffer,0,1024);
            if (FD_ISSET(out[0], &ready))
            {
                i = read(out[0], buffer, 1024);
//fprintf(stderr,"remap read: %s\n", buffer);
//buffer[strlen(buffer)]='\n'; i++;
                if (i <= 0)
                    exit(1);
                write(fileno(stdout), buffer, i);
            }
            
            if (FD_ISSET(fileno(stdin), &ready))
            {
                i = read(fileno(stdin), buffer, 1024);
//fprintf(stderr,"remap read: %s\n", buffer);
//buffer[strlen(buffer)]='\n'; i++;
                if (i <= 0)
                    exit(1);
                write(in[1], buffer, i);
            }
        }
        
    } else {
        /* Child. */

        /* Open /dev/null */
        i = open("/dev/null", O_WRONLY);
        if (i == -1)
        {
            fprintf(stderr, "Problems opening /dev/null.\n");
            exit(1);
        }

        /* Remap stdin/stdout to /dev/null to prevent tampering. */
        dup2(i, fileno(stdin));
        dup2(i, fileno(stdout));
        close(i);

        execvp(buffer, args);

        fprintf(stderr, "Error executing program.\n");
        kill(getppid(), 9);
        
    }
}
