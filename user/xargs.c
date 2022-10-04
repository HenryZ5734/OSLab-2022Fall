#include "kernel/types.h"
#include "user/user.h"

void
xargs(int argc, char *argv[], char *cmd[])
{
    /* for input string */
    char inString[50];
    int inSize = -1;

    /* parse input string */
    char buffer[100];
    char* bufferPointer = buffer;
    char tmpChar;
    int bufferSize = 0;

    int cmdSize = argc - 1;

    while((inSize = read(0, inString, sizeof(inString))) > 0){
        for(int i=0;i<inSize;i++){
        tmpChar = inString[i];

        /* excute the command*/
        switch(tmpChar){
            case '\n':
                cmd[cmdSize++] = bufferPointer;
                cmd[cmdSize] = 0;
                buffer[bufferSize] = 0; 

                if(fork() == 0){
                exec(argv[1],cmd);
                }

                /* wait for child */
                wait(0);

                /* re-initialize */
                cmdSize = argc - 1;
                bufferSize = 0;
                bufferPointer = buffer;
                break;
            case ' ':
                cmd[cmdSize++] = bufferPointer;
                buffer[bufferSize++] = 0;
                bufferPointer = buffer+bufferSize;
                break;
            default:
                buffer[bufferSize++] = tmpChar;
                break;
            }
        }
    }
}

int
main(int argc, char *argv[])
{
    char *cmd[32];

    /* initialize */
    for(int i=0;i<argc-1;i++)
        cmd[i]=argv[i+1];

    xargs(argc, argv, cmd);
    exit(0);
}