#include <stdio.h>
#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (1000)

int sd;
// client code
int main(int argc, char *argv[])
{
    // struct sockaddr_in addrSnd, addrRcv;

    // int sd = UDP_Open(20000);
    // int rc = UDP_FillSockAddr(&addrSnd, "localhost", 10003);

    char message[BUFFER_SIZE];
    sprintf(message, "hello world");

    sd = MFS_Init("localhost", 72727);

    MFS_Lookup(0, ".");
    MFS_Lookup(0, "..");

    printf("%d\n", MFS_Creat(0, MFS_DIRECTORY, "a"));
    printf("%d\n", MFS_Creat(1, MFS_DIRECTORY, "b"));
    printf("%d\n", MFS_Creat(2, MFS_DIRECTORY, "c"));
    printf("%d\n", MFS_Creat(3, MFS_DIRECTORY, "d"));

    // printf("unlinking a/b: %d\n", MFS_Unlink(1, "b"));
    // printf("unlinking a: %d\n", MFS_Unlink(0, "a"));

    //printf("testing too many nbytes: %d\n", MFS_Write(2, "hi", 0, 30 * MFS_BLOCK_SIZE + 1));
    //printf("%d\n", MFS_Lookup(0, "osu"));

    //MFS_Creat(1, MFS_DIRECTORY, "osu_subdirectory");
    // printf("unlinking test_directory_empty: %d\n", MFS_Unlink(1, "osu_subdirectory"));
    // printf("unlinking non_empty_directory: %d\n", MFS_Unlink(0, "osu"));

    MFS_Shutdown();

    // printf("client:: send message [%s]\n", message);
    // int rc = UDP_Write(sd, &addrSnd, message, BUFFER_SIZE);
    // if (rc < 0)
    // {
    //     printf("client:: failed to send\n");
    //     exit(1);
    // }

    // printf("client:: wait for reply...\n");
    // rc = UDP_Read(sd, &addrRcv, message, BUFFER_SIZE);
    // printf("client:: got reply [size:%d contents:(%s)\n", rc, message);
    return 0;
}
