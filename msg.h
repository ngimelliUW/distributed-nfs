#include "mfs.h"

#define INIT (0)
#define LOOKUP (1)
#define STAT (2)
#define WRITE (3)
#define READ (4)
#define CREAT (5)
#define UNLINK (6)
#define SHUTDOWN (7)

typedef struct __msg_t
{
    char *hostname;
    int port;
    int pinum;
    char *name;
    int inum;
    MFS_Stat_t *m;
    char *buffer;
    int offset;
    int nbytes;
    int type;
    int func;
} msg_t;

typedef struct __res_t
{
    int rc;
    int inum;
    char msg[16];
    MFS_Stat_t mfs_stat;
    char buffer[MFS_BLOCK_SIZE];
} res_t;