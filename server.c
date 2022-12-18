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

    for (int i = 0; i < superBlock.num_inodes; i++)
    {
        inodes[i] = malloc(sizeof(inode_t));
        inodes[i]->size = 0;
        inodes[i]->type = 0;

        for (int j = 0; j < DIRECT_PTRS; j++)
        {
            inodes[i]->direct[j] = -1;
        }
    }
    // Set bits in the inode bitmap to 0
    inode_bitmap = malloc(superBlock.inode_bitmap_len * MFS_BLOCK_SIZE);
    memset(inode_bitmap, 0, MFS_BLOCK_SIZE * superBlock.inode_bitmap_len);

    // set bits in data bitmap to 0
    data_bitmap = malloc(superBlock.data_bitmap_len * MFS_BLOCK_SIZE);
    memset(data_bitmap, 0, MFS_BLOCK_SIZE * superBlock.data_bitmap_len);

    // Set up root directory
    inodes[0]->size = 2 * sizeof(dir_ent_t); //Two dot entries
    inodes[0]->type = MFS_DIRECTORY;
    inodes[0]->direct[0] = 0;

    for (int i = 1; i < DIRECT_PTRS; i++)
    {
        inodes[0]->direct[i] = -1;
    }

    // Update bitmaps
    set_bit(inode_bitmap, 0);
    set_bit(data_bitmap, 0);

    // TODO: malloc datablocks
    data_blocks = malloc(superBlock.num_data * MFS_BLOCK_SIZE);

    //curr = ., parent = ..
    dir_ent_t *curr = data_blocks;
    strncpy(curr->name, ".", 28);
    curr->inum = 0;

    dir_ent_t *parent = data_blocks + sizeof(dir_ent_t);
    strncpy(parent->name, "..", 28);
    parent->inum = 0;

    // Fill in unused entries with inode -1, in case of remove
    for (int i = 2; i < MFS_BLOCK_SIZE / sizeof(dir_ent_t); i++)
    {
        dir_ent_t *unused_dir = (data_blocks + i * sizeof(dir_ent_t));
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

    //printf("looking for file %s...\n", name);
    for (int i = 0; i < DIRECT_PTRS; i++)
    {
        //printf("looking at parent->direct[%d] = %d\n", i, parent->direct[i]);
        if (parent->direct[i] == -1)
            continue;

        for (int j = 0; j < parent->size; j += sizeof(dir_ent_t))
        {
            dir_ent_t *curr_dir_ent = data_blocks + MFS_BLOCK_SIZE * parent->direct[i] + j;
            // printf("%s\n", curr_dir_ent->name);

            if (!strncmp(curr_dir_ent->name, name, 28))
            {
                //printf("found file with name: %s\n", curr_dir_ent->name);
                return curr_dir_ent->inum;
            }
        }
    }
    //printf("could not find file %s\n", name);
    return -1;
}

int server_stat(int inum, MFS_Stat_t *m)
{
    if (superBlock.num_inodes < inum || inum < 0)
    {
        printf("This inum is not in the inode table");
        return -1;
    }
    if (!get_bit(inode_bitmap, inum))
    { //If the bit for this inum is 0
        printf("There's no allocated file at this inode");
        return -1;
    }
    inode_t *curr_inode = inodes[inum];
    printf("curr_inode type: %d, curr_inode size: %d", curr_inode->type, curr_inode->size);
    res.mfs_stat.size = curr_inode->size;
    res.mfs_stat.type = curr_inode->type;
    return 0;
}

//Failure modes: invalid inum, invalid nbytes, invalid offset, not a regular file (because you can't write to directories).
int server_write(int inum, char *buffer, int offset, int nbytes)
{
    //invalid nbytes
    if (nbytes < 0 || nbytes > MFS_BLOCK_SIZE)
    {
        printf("Invalid offset\n");
        return -1;
    }

    // invalid inum:
    if (inum < 0 || inum > superBlock.num_inodes)
    {
        printf("Parent inum is out of range\n");
        return -1;
    }

    // unallocated nbytes
    if (!get_bit(inode_bitmap, inum))
    {
        printf("Parent inum is unallocated\n");
        return -1;
    }

    inode_t *file = inodes[inum];

    // invalid: directory
    if (file->type == MFS_DIRECTORY)
    {
        printf("File type is directory\n");
        return -1;
    }

    // invalid offset
    if (offset < 0 || file->size < offset)
    {
        printf("Invalid offset\n");
        return -1;
    }

    if (offset + nbytes > 30 * MFS_BLOCK_SIZE)
    {
        printf("Invalid offset\n");
        return -1;
    }

    int relative_offset = offset % MFS_BLOCK_SIZE;

    int direct_index = offset / MFS_BLOCK_SIZE;

    int data_block_num = file->direct[direct_index];

    if (data_block_num == -1)
    {
        //Init a data block
        for (int i = 0; i < superBlock.num_data; i++)
        {
            if (get_bit(data_bitmap, i) != 0)
            {
                continue;
            }
            else
            {
                set_bit(data_bitmap, i);
                file->direct[direct_index] = i;
                data_block_num = i;
                break;
            }
        }
    }

    int last_byte = direct_index * MFS_BLOCK_SIZE + relative_offset + nbytes;
    if (last_byte > file->size)
        file->size = last_byte;

    if (nbytes + relative_offset <= MFS_BLOCK_SIZE)
    {
        // strncpy(data_blocks + (data_block_num * MFS_BLOCK_SIZE) + relative_offset, buffer, nbytes);
        memcpy(data_blocks + (data_block_num * MFS_BLOCK_SIZE) + relative_offset, buffer, nbytes);
        return 0;
    }
    else
    {
        // strncpy(data_blocks + (data_block_num * MFS_BLOCK_SIZE) + relative_offset, buffer, MFS_BLOCK_SIZE - relative_offset);
        memcpy(data_blocks + (data_block_num * MFS_BLOCK_SIZE) + relative_offset, buffer, MFS_BLOCK_SIZE - relative_offset);
    }

    int data_block_num2 = file->direct[direct_index + 1];

    if (data_block_num2 == -1)
    {
        //Init a data block
        for (int i = 0; i < superBlock.num_data; i++)
        {
            if (get_bit(data_bitmap, i) != 0)
            {
                continue;
            }
            else
            {
                set_bit(data_bitmap, i);
                file->direct[direct_index + 1] = i;
                data_block_num2 = i;
                break;
            }
        }
    }
    // strncpy(data_blocks + (data_block_num2 * MFS_BLOCK_SIZE), buffer, nbytes - (MFS_BLOCK_SIZE - relative_offset));
    memcpy(data_blocks + (data_block_num2 * MFS_BLOCK_SIZE), buffer + MFS_BLOCK_SIZE - relative_offset, nbytes - (MFS_BLOCK_SIZE - relative_offset));

    return 0;
}

int server_read(int inum, char *buffer, int offset, int nbytes)
{
    //invalid nbytes
    if (nbytes < 0 || nbytes > MFS_BLOCK_SIZE)
    {
        printf("Invalid offset\n");
        return -1;
    }

    // invalid inum:
    if (inum < 0 || inum > superBlock.num_inodes)
    {
        printf("Parent inum is out of range\n");
        return -1;
    }

    // unallocated nbytes
    if (!get_bit(inode_bitmap, inum))
    {
        printf("Parent inum is unallocated\n");
        return -1;
    }

    inode_t *file = inodes[inum];

    // // if reading directory
    // if (file->type == MFS_DIRECTORY)
    // {
    //     dir_ent_t *temp = (dir_ent_t *)buffer;
    //     for (int i = 0; i < DIRECT_PTRS; i++)
    //     {
    //         if (file->direct[i] == -1)
    //             continue;

    //         dir_ent_t *curr;
    //         for (int j = 0; j < MFS_BLOCK_SIZE; j += sizeof(dir_ent_t))
    //         {
    //             curr = data_blocks + MFS_BLOCK_SIZE * file->direct[i] + j;
    //             if (curr->inum == -1)
    //                 continue;
    //             memcpy(temp, curr, sizeof(dir_ent_t));
    //             temp += sizeof(dir_ent_t);
    //         }
    //     }
    //     return 0;
    // }

    // invalid offset
    if (offset < 0 || file->size < offset)
    {
        printf("Invalid offset\n");
        return -1;
    }

    if (offset + nbytes > file->size)
    {
        printf("Invalid offset\n");
        return -1;
    }

    int relative_offset = offset % MFS_BLOCK_SIZE;

    int direct_index = offset / MFS_BLOCK_SIZE;

    int data_block_num = file->direct[direct_index];

    if (nbytes + relative_offset <= MFS_BLOCK_SIZE)
    {
        memcpy(buffer, data_blocks + (data_block_num * MFS_BLOCK_SIZE) + relative_offset, nbytes);
        // char * temp = data_blocks + (data_block_num * MFS_BLOCK_SIZE) + relative_offset;
        // printf("Read %s at address in write\n", temp);
        // printf("Read %s to buffer in write\n", buffer);
        // memcpy(buffer, data_blocks + (data_block_num * MFS_BLOCK_SIZE) + relative_offset, nbytes);
        //printf("RETURNING 0\n");
        return 0;
    }
    else
    {
        //printf("NOT RETURNING\n");
        memcpy(buffer, data_blocks + (data_block_num * MFS_BLOCK_SIZE) + relative_offset, MFS_BLOCK_SIZE - relative_offset);
    }

    int data_block_num2 = file->direct[direct_index + 1];

    memcpy(buffer + MFS_BLOCK_SIZE - relative_offset, data_blocks + (data_block_num2 * MFS_BLOCK_SIZE), nbytes - (MFS_BLOCK_SIZE - relative_offset));

    return 0;
}
int server_creat(int pinum, int type, char *name)
{
    if (pinum < 0 || pinum > superBlock.num_inodes)
    {
        printf("Parent inum is out of range");
        return -1;
    }

    if (!get_bit(inode_bitmap, pinum))
    {
        printf("Parent inum is unallocated");
        return -1;
    }

    inode_t *parent = inodes[pinum];
    if (parent->type != MFS_DIRECTORY)
    {
        printf("passed a parent that is node a directory\n");
        return -1;
    }

    dir_ent_t *new_entry;
    // printf("got to for loop\n");
    for (int i = 0; i < DIRECT_PTRS; i++)
    {
        if (parent->direct[i] != -1)
        {
            for (int j = 0; j < MFS_BLOCK_SIZE; j += sizeof(dir_ent_t))
            {
                dir_ent_t *temp = data_blocks + MFS_BLOCK_SIZE * parent->direct[i] + j;
                if (temp->inum == -1)
                {
                    new_entry = temp;
                    i = DIRECT_PTRS; // exit out of outer-for loop
                    break;
                }
            }
        }
        else
        {
            //Init a data block
            for (int j = 0; j < superBlock.num_data; j++)
            {
                if (get_bit(data_bitmap, j) != 0)
                {
                    continue;
                }
                else
                {
                    set_bit(data_bitmap, j);
                    new_entry = data_blocks + MFS_BLOCK_SIZE * j;

                    // iterate through data block and init blank entries:
                    for (int k = 0; k < MFS_BLOCK_SIZE / sizeof(dir_ent_t); k++)
                    {
                        dir_ent_t *unused_dir = (new_entry + k * sizeof(dir_ent_t));
                        unused_dir->inum = -1;
                        strncpy(unused_dir->name, "", 28);
                    }
                    parent->direct[i] = j;
                    i = DIRECT_PTRS;
                    break;
                }
            }
        }
    }

    // printf("got past for loop\n");

    strncpy(new_entry->name, name, 28);

    // find next free inode
    // printf("got to next free inode section\n");
    int next_free_inode = -1;
    for (int i = 0; i < superBlock.num_inodes; i++)
    {
        if (get_bit(inode_bitmap, i) == 0)
        {
            next_free_inode = i;
            set_bit(inode_bitmap, i);
            break;
        }
    }

    new_entry->inum = next_free_inode;
    inodes[next_free_inode]->type = type;
    // handle file if its a directory:
    // printf("got to directory section\n");
    if (type == MFS_DIRECTORY)
    {
        // find next free datablock:
        for (int i = 0; i < superBlock.num_data; i++)
        {
            if (get_bit(data_bitmap, i) == 1)
            {
                continue;
            }
            else
            {
                set_bit(data_bitmap, i);
                inodes[next_free_inode]->direct[0] = i;
                inodes[next_free_inode]->size = 2 * sizeof(dir_ent_t);

                dir_ent_t *temp = data_blocks + MFS_BLOCK_SIZE * i;
                temp->inum = next_free_inode;
                strncpy(temp->name, ".", 28);

                dir_ent_t *temp2 = data_blocks + MFS_BLOCK_SIZE * i + sizeof(dir_ent_t);
                temp2->inum = pinum;
                strncpy(temp2->name, "..", 28);

                for (int j = 2; j < MFS_BLOCK_SIZE / sizeof(dir_ent_t); j++)
                {
                    temp = data_blocks + MFS_BLOCK_SIZE * i + j * sizeof(dir_ent_t);
                    temp->inum = -1;
                    strncpy(temp->name, "", 28);
                }

                break;
            }
        }
    }

    parent->size += sizeof(dir_ent_t);
    return 0;
}

int server_unlink(int pinum, char *name)
{
    if (pinum < 0 || pinum > superBlock.num_inodes)
    {
        printf("Parent inum is out of range");
        return -1;
    }

    if (!get_bit(inode_bitmap, pinum))
    {
        printf("Parent inum is unallocated");
        return -1;
    }
    inode_t *parent = inodes[pinum];
    // if (parent->type == MFS_DIRECTORY && parent->size > 2 * sizeof(dir_ent_t))
    // {
    //     return -1;
    // }

    dir_ent_t *entry;
    for (int i = 0; i < DIRECT_PTRS; i++)
    { //Looking for the entry
        if (parent->direct[i] == -1)
            continue;

        for (int j = 0; j < MFS_BLOCK_SIZE; j += sizeof(dir_ent_t))
        {
            dir_ent_t *curr = data_blocks + MFS_BLOCK_SIZE * parent->direct[i] + j;
            if (strncmp(curr->name, name, 28) == 0)
            {
                entry = curr;
                i = DIRECT_PTRS; //Exit outer loop
                break;
            }
        }
    }

    if (entry->inum == -1)
    {
        printf("File not found\n");
        return -1;
    }

    inode_t *entry_inode = inodes[entry->inum];

    if (entry_inode->type == MFS_DIRECTORY)
    {
        //Recursively remove everything in the directory
        for (int i = 0; i < DIRECT_PTRS; i++)
        {
            if (entry_inode->direct[i] == -1)
            {
                continue;
            }

            for (int j = 0; j < MFS_BLOCK_SIZE; j += sizeof(dir_ent_t))
            {
                dir_ent_t *curr = data_blocks + MFS_BLOCK_SIZE * entry_inode->direct[i] + j;
                if (strncmp(curr->name, ".", 28) == 0 || strncmp(curr->name, "..", 28) == 0)
                {
                    continue;
                }

                if (curr->inum == -1)
                {
                    continue;
                }
                return -1;
            }
        }
    }

    clear_bit(inode_bitmap, entry->inum);
    for (int i = 0; i < DIRECT_PTRS; i++)
    {
        clear_bit(data_bitmap, entry_inode->direct[i]);
        entry_inode->direct[i] = -1;
    }

    entry->inum = -1;
    strncpy(entry->name, "", 28);
    entry_inode->type = 0;
    entry_inode->size = 0;
    return 0;
}

int server_shutdown()
{
    fsync(fileD);
    close(fileD);
    //Free mallocs
    for (int i = 0; i < superBlock.num_inodes; i++)
        free(inodes[i]);
    free(inodes);
    free(inode_bitmap);
    free(data_bitmap);
    free(data_blocks);
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

    server_init();

    signal(SIGINT, intHandler);

    int sd = UDP_Open(portnum);

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
        }

        if (msg.func == WRITE)
        {
            res.rc = server_write(msg.inum, msg.buffer, msg.offset, msg.nbytes);
        }

        if (msg.func == READ)
        {
            res.rc = server_read(msg.inum, res.buffer, msg.offset, msg.nbytes);
            // printf("Read %s at msg.buffer in write\n", msg.buffer);
        }

        if (msg.func == CREAT)
        {
            res.rc = server_creat(msg.pinum, msg.type, msg.name);
        }

        if (msg.func == STAT)
        {
            res.rc = server_stat(msg.inum, msg.m);
        }

        if (msg.func == UNLINK)
        {
            res.rc = server_unlink(msg.pinum, msg.name);
        }

        // void *bff = inode_bitmap - (superBlock.inode_bitmap_addr * MFS_BLOCK_SIZE);
        // write(fileD, bff, (superBlock.data_region_addr + superBlock.data_region_len) * MFS_BLOCK_SIZE);

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
