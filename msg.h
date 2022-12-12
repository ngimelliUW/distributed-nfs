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