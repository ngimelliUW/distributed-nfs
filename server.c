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

// //These two functions take the pointer to the beginning of the inode or data block bitmap region
// //and an integer that is the inode or data block number.
// unsigned int get_bit(unsigned int *bitmap, int position) {
//    int index = position / 32;
//    int offset = 31 - (position % 32);
//    return (bitmap[index] >> offset) & 0x1;
// }

// void set_bit(unsigned int *bitmap, int position) {
//    int index = position / 32;
//    int offset = 31 - (position % 32);
//    bitmap[index] |=  0x1 << offset;
// }

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

        // if (msg.func == STAT){
        //     if (superBlock.num_inodes < msg.inum || msg.inum < 0){
        //         printf("This inum is not in the inode table");
        //     }
        //     else{
        //         if (!get_bit((unsigned int *) (long) superBlock.inode_bitmap_addr, msg.inum)){ //If the bit for this inum is 0
        //             printf("There's no allocated file at this inode");
        //         }
        //         else{
        //             res.rc = 0;
        //             inode_t curr_inode = *(inode_t *)(long) (superBlock.inode_region_addr + MFS_INODE_SIZE * msg.inum);
        //             msg.m -> type = curr_inode.type;
        //             msg.m -> size = curr_inode.size;
        //         }

        //     }
        // }

        if (msg.func == SHUTDOWN)
        {
            server_shutdown();
            res.rc = 0;
        }

        // after reqs:
        // ACKNOWLEDGEMENT
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
