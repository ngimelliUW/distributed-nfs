#include "udp.h"
#include "mfs.h"
#include "ufs.h"
#include "msg.h"

int fd;
struct sockaddr_in server_addr;

msg_t msg;  // message to send to server
res_t sres; // message to receive from server

void send_req()
{
    struct timeval tv;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    tv.tv_sec = 3;
    tv.tv_usec = 0;

    UDP_Write(fd, &server_addr, (char *)&msg, sizeof(msg_t));

    int is_ready = select(fd + 1, &fds, NULL, NULL, &tv);

    if (is_ready)
    {
        UDP_Read(fd, &server_addr, (char *)&sres, sizeof(res_t));
        if (sres.rc)
        {
            // handle req failure
            printf("req failed\n");
        }
        else
        {
            // handle req success
            printf("req successful\n");
        }
    }
    else
    {
        // timeout occured:
        printf("timed out. trying req again\n");
        send_req();
    }
    UDP_Write(fd, &server_addr, (char *)&msg, sizeof(msg_t));
}

//  takes a host name and port number and uses those to find the server exporting the file system
int MFS_Init(char *hostname, int port)
{
    int rc = UDP_FillSockAddr(&server_addr, hostname, port);

    if (rc != 0)
    {
        printf("Failed to set up server address\n");
        return rc;
    }

    fd = UDP_Open(0);
    return 0;
}

/**
 * takes the parent inode number (which should be the inode number of a directory) and looks up the entry name in it.
 * The inode number of name is returned. Success: return inode number of name; failure: return -1.
 * Failure modes: invalid pinum, name does not exist in pinum.
*/
int MFS_Lookup(int pinum, char *name)
{
    msg.func = LOOKUP;
    msg.pinum = pinum;
    strcpy(msg.name, name);

    send_req();

    return -1;
}

/*
 * returns some information about the file specified by inum.
 * Upon success, return 0, otherwise -1.
 * The exact info returned is defined by MFS_Stat_t.
 * Failure modes: inum does not exist.
 * File and directory sizes are described below.
 */
int MFS_Stat(int inum, MFS_Stat_t *m)
{
    msg.inum = inum;
    msg.m = m;
    send_req();

    return -1;
}

/*
 * writes a buffer of size nbytes (max size: 4096 bytes) at the byte offset specified by offset.
 * Returns 0 on success, -1 on failure.
 * Failure modes: invalid inum, invalid nbytes, invalid offset, not a regular file (because you can't write to directories).
*/
int MFS_Write(int inum, char *buffer, int offset, int nbytes)
{
    msg.inum = inum;
    strcpy(msg.buffer, buffer);
    msg.offset = offset;
    msg.nbytes = nbytes;
    send_req();
    return -1;
}

/**
 * reads nbytes of data (max size 4096 bytes) specified by the byte offset offset into the buffer from file specified by inum.
 * The routine should work for either a file or directory; directories should return data in the format specified by MFS_DirEnt_t.
 * Success: 0, failure: -1. Failure modes: invalid inum, invalid offset, invalid nbytes.
*/
int MFS_Read(int inum, char *buffer, int offset, int nbytes)
{
    msg.inum = inum;
    strcpy(msg.buffer, buffer);
    msg.offset = offset;
    msg.nbytes = nbytes;
    send_req();
    return -1;
}

/**
 * makes a file (type == MFS_REGULAR_FILE) or directory (type == MFS_DIRECTORY)
 * in the parent directory specified by pinum of name name.
 * Returns 0 on success, -1 on failure.
 * Failure modes: pinum does not exist, or name is too long. If name already exists, return success.
*/
int MFS_Creat(int pinum, int type, char *name)
{
    msg.pinum = pinum;
    msg.type = type;
    strcpy(msg.name, name);
    send_req();
    return -1;
}

/**
 * removes the file or directory name from the directory specified by pinum.
 * 0 on success, -1 on failure.
 * Failure modes: pinum does not exist, directory is NOT empty.
 * Note that the name not existing is NOT a failure by our definition (think about why this might be).
*/
int MFS_Unlink(int pinum, char *name)
{
    msg.pinum = pinum;
    strcpy(msg.name, name);
    send_req();
    return -1;
}

/**
 * ust tells the server to force all of its data structures to disk and shutdown by calling exit(0).
 * This interface will mostly be used for testing purposes.
*/
int MFS_Shutdown()
{
    msg.func = SHUTDOWN;
    send_req();

    UDP_Close(fd);
    return 0;
}