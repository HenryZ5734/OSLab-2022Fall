#include "kernel/types.h"
#include "user.h"

int main(){
    int remain[34];
    int count=34;
    int out_num;
    int fd[2];
    int child;

    /* initial */
    for(int i=0;i<count;i++){
        remain[i] = i+2;
    }

    while(count != 0){

        /* print first*/
        out_num = remain[0];
        printf("prime %d\n", out_num);

        /* remove and shift*/
        int tag=0;
        for(int i=0;i<count;i++){
            if(remain[i] % out_num != 0){
                remain[tag] = remain[i];
                tag++;
            }
        }
        count = tag;

        /* pipe */
        if(count != 0){
            pipe(fd);
            /* parent */
            if((child = fork()) != 0){
                close(fd[0]);
                write(fd[1], remain, 100);
                break;
            }
            /* child */
            else{
                close(fd[1]);
                read(fd[0], remain, 100);
            }
        }
    }

    wait(&child);
    exit(0);
}