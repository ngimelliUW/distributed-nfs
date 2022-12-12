#include "mfs.h"

typedef struct __msg_t {
    char * hostname;
    int port;
    int pinum;
    char * name;
    int inum;
    MFS_Stat_t * m;
    char * buffer;
    int offset;
    int nbytes;
    int type;
} msg_t;