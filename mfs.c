#include "udp.h"
#include "mfs.h"

int fd;
struct sockaddr_in server_addr;

//  takes a host name and port number and uses those to find the server exporting the file system
int MFS_Init(char *hostname, int port)
{
    int rc = UDP_FillSockAddr(&server_addr, hostname, port);

    if (rc != 0)
    {
        printf("Failed to set up server address\n");
        return rc;
    }

    fd = UDP_Open(port);
    return 0;
}

// returns some information about the file specified by inum. Upon success, return 0, otherwise -1. The exact info returned is defined by MFS_Stat_t. Failure modes: inum does not exist. File and directory sizes are described below.
int MFS_Stat(int inum, MFS_Stat_t *m)
{
}