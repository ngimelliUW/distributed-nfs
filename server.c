#include <stdio.h>
#include "udp.h"
#include "ufs.h"
#include "msg.h"
#include <sys/stat.h>
#include <signal.h>

#define BUFFER_SIZE (1000)

int sd;    // connection fd
int fileD; // from file system image
super_t superBlock;

res_t res;

void intHandler(int dummy)
{
    UDP_Close(sd);
    exit(130);
}

int server_shutdown()
{
    fsync(fileD);
    close(fileD);
    return 0;
}

// server code
//portnum, file-system-image
int main(int argc, char *argv[])
{
    int portnum = atoi(argv[1]);
    char *fsi = argv[2];

    fileD = open(fsi, O_RDWR | O_CREAT, S_IRWXU);
    read(fileD, &superBlock, sizeof(super_t));

    res.rc = -1; // default return val

    signal(SIGINT, intHandler);
    int sd = UDP_Open(portnum);
    assert(sd > -1);
    while (1)
    {
        struct sockaddr_in addr;

        msg_t msg;
        UDP_Read(sd, &addr, (char *)&msg, sizeof(msg));

        if (msg.func == SHUTDOWN)
        {
            server_shutdown();
            res.rc = 0;
        }

        // after reqs:
        UDP_Write(sd, &addr, (char *)&res, sizeof(res));

        if (msg.func == SHUTDOWN)
        {
            UDP_Close(fileD);
            exit(0);
        }

        char message[BUFFER_SIZE];
        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &addr, message, BUFFER_SIZE);
        printf("server:: read message [size:%d contents:(%s)]\n", rc, message);
        if (rc > 0)
        {
            char reply[BUFFER_SIZE];
            sprintf(reply, "goodbye world");
            rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
            printf("server:: reply\n");
        }
    }

    return 0;
}
