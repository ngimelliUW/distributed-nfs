#include <stdio.h>
#include "udp.h"
#include "ufs.h"
#include "msg.h"
#include <sys/stat.h>
#include <signal.h>

#define BUFFER_SIZE (1000)
//REMINDER Amount of directory entries = directory inode's size / 32

int sd;    // connection fd
int fileD; // from file system image
super_t superBlock;
res_t res;
inode_t **inodes;
void *data_blocks;
void *inode_bitmap;
void *data_bitmap;

void intHandler(int dummy)
{
    UDP_Close(sd);
    exit(130);
}

// These two functions take the pointer to the beginning of the inode or data block bitmap region and an integer that is the inode or data block number.
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

void clear_bit(unsigned int *bitmap, int position)
{
    int index = position / 32;
    int offset = 31 - (position % 32);
    bitmap[index] &= ~(0x1 << offset);
}

int server_init()
{
    inodes = malloc(superBlock.num_inodes * sizeof(inode_t *));

    for (int i = 1; i < superBlock.num_inodes; i++)
    {
        inodes[i] = malloc(sizeof(inode_t));
        inodes[i]->size = 0;
        inodes[i]->type = 0;

        for (int j = 0; j < DIRECT_PTRS; j++)
        {
            inodes[i]->direct[j] = ~0;
        }
    }
    // Set bits in the inode bitmap to 0
    inode_bitmap = malloc(superBlock.inode_bitmap_len * MFS_BLOCK_SIZE);
    memset(inode_bitmap, 0, MFS_BLOCK_SIZE * superBlock.inode_bitmap_len);

    // set bits in data bitmap to 0
    data_bitmap = malloc(superBlock.data_bitmap_len * MFS_BLOCK_SIZE);
    memset(data_bitmap, 0, MFS_BLOCK_SIZE * superBlock.data_bitmap_len);

    // Set up root directory
    inodes[0] = malloc(sizeof(inode_t));
    inodes[0]->size = 2 * sizeof(dir_ent_t); //Two dot entries
    inodes[0]->type = MFS_DIRECTORY;
    inodes[0]->direct[0] = 0;

    // Update bitmaps
    set_bit(inode_bitmap, 0);
    set_bit(data_bitmap, 0);

    // TODO: malloc datablocks
    data_blocks = malloc(superBlock.num_data * MFS_BLOCK_SIZE);

    //curr = ., parent = ..
    MFS_DirEnt_t *curr = data_blocks;
    strncpy(curr->name, ".", 28);
    curr->inum = 0;

    MFS_DirEnt_t *parent = data_blocks + sizeof(MFS_DirEnt_t);
    strncpy(parent->name, "..", 28);
    parent->inum = 0;

    // Fill in unused entries with inode -1, in case of remove
    for (int i = 2; i < MFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t); i++)
    {
        MFS_DirEnt_t *unused_dir = (data_blocks + i * sizeof(MFS_DirEnt_t));
        unused_dir->inum = -1;
        strncpy(unused_dir->name, "", 28);
    }

    fsync(fileD);
    return 0;
}

int server_lookup(int pinum, char *name)
{
    if (pinum < 0 || pinum > superBlock.num_inodes)
    {
        return -1;
    }

    if (!get_bit(inode_bitmap, pinum))
    {
        return -1;
    }

    inode_t *parent = inodes[pinum];

    for (int i = 0; i < DIRECT_PTRS; i++)
    {
        if (parent->direct[i] == ~0)
            continue;

        for (int j = 0; j < parent->size; j += sizeof(dir_ent_t))
        {
            dir_ent_t *curr_dir_ent = data_blocks + MFS_BLOCK_SIZE * parent->direct[i] + j;

            if (!strncmp(curr_dir_ent->name, name, 28))
            {
                res.rc = 0;
                return curr_dir_ent->inum;
            }
        }
    }
    return -1;
}

int server_stat(int inum, MFS_Stat_t *m)
{
    // if (superBlock.num_inodes < inum || inum < 0)
    // {
    //     printf("This inum is not in the inode table");
    //     return -1;
    // }
    // if (!get_bit((unsigned int *)(long)superBlock.inode_bitmap_addr, inum))
    // { //If the bit for this inum is 0
    //     printf("There's no allocated file at this inode");
    //     return -1;
    // }
    // res.rc = 0;
    // inode_t curr_inode = inodes[inum];
    // m->type = curr_inode.type;
    // m->size = curr_inode.size;
    return 0;
}

// int server_creat(int pinum, int type, char *name)
// {
//     if (pinum < 0 || pinum > superBlock.num_inodes)
//     {
//         printf("Parent inum is out of range");
//         return -1;
//     }

//     if (!get_bit((unsigned int *)(long)superBlock.inode_bitmap_addr, pinum))
//     {
//         printf("Parent inum is unallocated");
//         return -1;
//     }

//     inode_t parent = inodes[pinum];
//     // long inode_address = (long)(fileD + superBlock.inode_region_addr + pinum * 128);
//     // memcpy(&parent, (void *)inode_address, sizeof(inode_t));

//     char pblock[MFS_BLOCK_SIZE];
//     // loop through parent inodes pointers:
//     for (int i = 0; i < DIRECT_PTRS; i++)
//     {
//         if (parent.direct[i] == ~0) //Unsigned -1 = 32 1s. ~0 = 32 1s. Unused directories are initialized to -1 in his initfs(). REMINDER
//             continue;

//         int offset;
//         dir_ent_t *entry;
//         for (offset = 0; offset < MFS_BLOCK_SIZE; offset += sizeof(dir_ent_t))
//         {
//             entry = (dir_ent_t *)(pblock + offset);
//             if (entry->inum != -1)
//                 continue;

//             lseek(fileD, parent.direct[i], SEEK_SET);
//             read(fileD, pblock, MFS_BLOCK_SIZE);

//             int next_idx = -1;
//             // find first free inode:
//             for (int j = 0; j < superBlock.inode_region_len; j++)
//             {
//                 if (inodes[j].type == 0)
//                     next_idx = j;
//             }

//             // no inodes available:
//             if (next_idx == -1)
//                 return -1;

//             // make new inode for file:
//             inode_t *new_inode = &inodes[next_idx];
//             new_inode->type = type;
//             new_inode->size = 0;

//             // get next free datablock
//             int next_datablock_idx = -1;
//             for (int j = 0; j < superBlock.num_data; j++)
//             {
//                 int is_used = get_bit((unsigned int *)(long)superBlock.data_bitmap_addr, j);
//                 if (!is_used)
//                 {
//                     next_datablock_idx = j;
//                     break;
//                 }
//             }

//             if (next_datablock_idx == -1)
//             {
//                 // no datablocks available:
//                 return -1;
//             }

//             new_inode->direct[0] = next_datablock_idx;

//             // set block as used in bitmap:
//             set_bit((unsigned int *)(long)superBlock.data_bitmap_addr, next_datablock_idx);

//             // pwrite(fileD, meta_blocks, 3 * MFS_BLOCK_SIZE, MFS_BLOCK_SIZE);

//             // TODO: deal with making directories:

//             entry->inum = next_idx;
//             strncpy(entry->name, name, 60);

//             lseek(fileD, parent.direct[i], SEEK_SET);
//             write(fileD, pblock, MFS_BLOCK_SIZE);

//             return 0;
//         }

//         // grow if needed:
//         return -1;
//     }

//     return -1;
// }

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

    int portnum = strtol(argv[1], NULL, 10);
    char *fsi = argv[2];

    fileD = open(fsi, O_RDWR | O_CREAT, S_IRWXU);
    read(fileD, &superBlock, sizeof(super_t));

    server_init();
    printf("got past server init\n");

    signal(SIGINT, intHandler);

    int sd = UDP_Open(portnum);
    //REMINDER
    //Inits super block
    // if (sd == -1)
    // {
    //     fsync(sd);
    // }
    // else
    // {
    // pread(sd, meta_blocks, 3 * MFS_BLOCK_SIZE, MFS_BLOCK_SIZE);
    // }

    assert(sd > -1);
    while (1)
    {
        res.rc = -1; // default return val REMINDER moved inside while loop
        // printf("server:: waiting...\n");
        struct sockaddr_in addr;

        msg_t msg;
        UDP_Read(sd, &addr, (char *)&msg, sizeof(msg_t));

        // printf("msg.func is %d\n", msg.func);
        if (msg.func == LOOKUP)
        {
            res.rc = server_lookup(msg.pinum, msg.name);
            printf("Server lookup returned %d\n", res.rc);
        }

        if (msg.func == STAT)
        {
            server_stat(msg.inum, msg.m);
        }

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
            UDP_Close(sd);
            exit(0);
        }

        // char message[BUFFER_SIZE];
        // printf("server:: waiting...\n");
        // int rc = UDP_Read(sd, &addr, message, BUFFER_SIZE);
        // printf("server:: read message [size:%d contents:(%s)]\n", rc, message);
        // if (rc > 0)
        // {
        //     char reply[BUFFER_SIZE];
        //     sprintf(reply, "goodbye world");
        //     rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
        //     printf("server:: reply\n");
        // }
    }

    return 0;
}
