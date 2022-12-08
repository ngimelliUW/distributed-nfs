#include "udp.h"
#include "mfs.h"

int fd;
struct sockaddr_in server_addr;

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