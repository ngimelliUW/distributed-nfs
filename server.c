#include <stdio.h>
#include "udp.h"
#include "ufs.h"
#include "msg.h"

#define BUFFER_SIZE (1000)

int sd;
super_t superBlock;
void intHandler(int dummy) {
    UDP_Close(sd);
    exit(130);
}

// server code
//portnum, file-system-image
int main(int argc, char *argv[]) {
    int portnum = atoi(argv[1]);
    char* fsi = argv[2];

    int fd = open(fsi, O_RDWR | O_CREAT, S_IRWXU);
    read(fd, &superBlock, sizeof(super_t));
    // printf("Inode bitmap address is %d\n", superBlock.inode_bitmap_addr);
    // printf("Inode bitmap len is %d\n", superBlock.inode_bitmap_len);
    // printf("Data bitmap address is %d\n", superBlock.data_bitmap_addr);
    // printf("Data bitmap len is %d\n", superBlock.data_bitmap_len);

    signal(SIGINT, intHandler);
    int sd = UDP_Open(portnum);
    assert(sd > -1);
    while (1) {
	struct sockaddr_in addr;
	char message[BUFFER_SIZE];
	printf("server:: waiting...\n");
	int rc = UDP_Read(sd, &addr, message, BUFFER_SIZE);
	printf("server:: read message [size:%d contents:(%s)]\n", rc, message);
	if (rc > 0) {
            char reply[BUFFER_SIZE];
            sprintf(reply, "goodbye world");
            rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
	    printf("server:: reply\n");
	} 
    }
    
    return 0; 
}
