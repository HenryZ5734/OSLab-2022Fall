#include "kernel/types.h"
#include "user.h"

int main(int argc, char* argv[]){

    int fd1[2];
    int fd2[2];
    int child;
    int status;
    char read_content[10];
    char text[] = ": received ";

    /* create pipe */
    pipe(fd1);
    pipe(fd2);

    /* parent process*/
    if((child = fork()) != 0){
        close(fd1[0]);
        close(fd2[1]);
        char write_content[] = "ping"; 
        write(fd1[1], write_content, 20);
        read(fd2[0], read_content, 20);
        printf("%d%s%s\n", getpid(), text, read_content);
    }

    /* child process */
    else{
        close(fd1[1]);
        close(fd2[0]);
        read(fd1[0], read_content, 20);
        printf("%d%s%s\n", getpid(), text, read_content);
        char write_content[] = "pong"; 
        write(fd2[1], write_content, 20);
    }

    wait(&status);
    exit(0);
}