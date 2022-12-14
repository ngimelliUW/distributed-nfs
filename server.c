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

char meta_blocks[3 * MFS_BLOCK_SIZE];
inode_t *inodes;

char meta_blocks[3 * MFS_BLOCK_SIZE];
inode_t *inodes;

res_t res;


void intHandler(int dummy)
{
    UDP_Close(sd);
    exit(130);
}

unsigned int get_bit(unsigned int *bitmap, int position)
{
    int index = position / 32;
    int offset = 31 - (position % 32);
    return (bitmap[index] >> offset) & 0x1;
}

void set_bit(unsigned int *bitmap, int position)
{
    int index = position / 32;
    int offset = 31 - (position % 32);
    bitmap[index] |= 0x1 << offset;
}

int server_init()
{
    inodes = (inode_t *)(long)superBlock.inode_region_addr;
    return -1;
}

int server_stat(int inum, MFS_Stat_t *m) {
    if (superBlock.num_inodes < inum || inum < 0){
        printf("This inum is not in the inode table");
    }
    else{
        if (!get_bit((unsigned int *) (long) superBlock.inode_bitmap_addr, inum)){ //If the bit for this inum is 0
            printf("There's no allocated file at this inode");
        }
        else{
            res.rc = 0;
            inode_t curr_inode = inodes[inum];
            m -> type = curr_inode.type;
            m -> size = curr_inode.size;
            return 0;
        }
    }
    return -1;
}

int server_lookup(int pinum, char *name)
{
    // inode_t in;
    // long inode_address = (long)(fileD + superBlock.inode_region_addr + pinum * 128);
    // memcpy(&in, (void *)inode_address, sizeof(inode_t));

    //inode_t in = inodes[pinum];

    return -1;
}

int server_creat(int pinum, int type, char *name)
{
    if (pinum < 0 || pinum > superBlock.num_inodes - 1)
        return -1;

    inode_t parent = inodes[pinum];
    // long inode_address = (long)(fileD + superBlock.inode_region_addr + pinum * 128);
    // memcpy(&parent, (void *)inode_address, sizeof(inode_t));

    char pblock[MFS_BLOCK_SIZE];
    // loop through parent inodes pointers:
    for (int i = 0; i < DIRECT_PTRS; i++)
    {
        // figure out why this:
        if (parent.direct[i] == ~0)
            continue;

        int offset;
        dir_ent_t *entry;
        for (offset = 0; offset < MFS_BLOCK_SIZE; offset += sizeof(dir_ent_t))
        {
            entry = (dir_ent_t *)(pblock + offset);
            if (entry->inum != -1)
                continue;

            lseek(fileD, parent.direct[i], SEEK_SET);
            read(fileD, pblock, MFS_BLOCK_SIZE);

            int next_idx = -1;
            // find first free inode:
            for (int j = 0; j < superBlock.inode_region_len; j++)
            {
                if (inodes[j].type == 0)
                    next_idx = j;
            }

            // no inodes available:
            if (next_idx == -1)
                return -1;

            // make new inode for file:
            inode_t *new_inode = &inodes[next_idx];
            new_inode->type = type;
            new_inode->size = 0;

            // get next free datablock
            int next_datablock_idx = -1;
            for (int j = 0; j < superBlock.num_data; j++)
            {
                int is_used = get_bit((unsigned int *)(long)superBlock.data_bitmap_addr, j);
                if (!is_used)
                {
                    next_datablock_idx = j;
                    break;
                }
            }

            if (next_datablock_idx == -1)
            {
                // no datablocks available:
                return -1;
            }

            new_inode->direct[0] = next_datablock_idx;

            // set block as used in bitmap:
            set_bit((unsigned int *)(long)superBlock.data_bitmap_addr, next_datablock_idx);

            pwrite(fileD, meta_blocks, 3 * MFS_BLOCK_SIZE, MFS_BLOCK_SIZE);

            // TODO: deal with making directories:

            entry->inum = next_idx;
            strncpy(entry->name, name, 60);

            lseek(fileD, parent.direct[i], SEEK_SET);
            write(fileD, pblock, MFS_BLOCK_SIZE);

            return 0;
        }

        // grow if needed:
        return -1;
    }

    return -1;
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

    if (argc != 3)
    {
        exit(1);
    }

    int portnum = portnum = strtol(argv[1], NULL, 10);
    char *fsi = argv[2];

    fileD = open(fsi, O_RDWR | O_CREAT, S_IRWXU);
    read(fileD, &superBlock, sizeof(super_t));

    server_init();

    res.rc = -1; // default return val

    signal(SIGINT, intHandler);

    int sd = UDP_Open(portnum);
    if (sd == -1)
    {
        fsync(sd);
    }
    else
    {
        pread(sd, meta_blocks, 3 * MFS_BLOCK_SIZE, MFS_BLOCK_SIZE);
    }

    assert(sd > -1);
    while (1)
    {
        printf("server:: waiting...\n");
        struct sockaddr_in addr;

        msg_t msg;
        printf("got to read\n");
        UDP_Read(sd, &addr, (char *)&msg, sizeof(msg));
        printf("got past read\n");

        if (msg.func == STAT){
            server_stat(msg.inum, msg.m);
        }

        if (msg.func == SHUTDOWN)
        {
            server_shutdown();
            res.rc = 0;
        }

        printf("got to write\n");
        // after reqs:
        // ACKNOWLEDGEMENT
        UDP_Write(sd, &addr, (char *)&res, sizeof(res));
        printf("got past write\n");

        if (msg.func == SHUTDOWN)
        {
            printf("shuttin the ufck down\n");
            UDP_Close(sd);
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
