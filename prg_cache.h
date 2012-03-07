#ifndef _PRG_CACHE_H_
/* #define _PRG_CACHE_H_ */

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>

#define _(str) (str)

#define PROGNAME_WIDTH 20
#define PRG_HASH_SIZE 211
struct prg_node {
    struct prg_node *next;
    unsigned long inode;
    char name[PROGNAME_WIDTH];
    pid_t pid;
    int fd;
};/* *prg_hash[PRG_HASH_SIZE]; */

#ifndef LINE_MAX
#define LINE_MAX 4096
#endif
#define PRG_HASHIT(x) ((x) % PRG_HASH_SIZE)

#define PRG_LOCAL_ADDRESS "local_address"
#define PRG_INODE	 "inode"
#define PRG_SOCKET_PFX    "socket:["
#define PRG_SOCKET_PFXl (strlen(PRG_SOCKET_PFX))
#define PRG_SOCKET_PFX2   "[0000]:"
#define PRG_SOCKET_PFX2l  (strlen(PRG_SOCKET_PFX2))

#define PATH_PROC	   "/proc"
#define PATH_FD_SUFF	"fd"
#define PATH_FD_SUFFl       strlen(PATH_FD_SUFF)
#define PATH_PROC_X_FD      PATH_PROC "/%s/" PATH_FD_SUFF

#define PATH_CMDLINE	"cmdline"
#define PATH_CMDLINEl       strlen(PATH_CMDLINE)
void prg_cache_add(pid_t pid, unsigned long inode, char *name);
const char *prg_cache_get_name(unsigned long inode);
pid_t prg_cache_get_pid(unsigned long inode);
void prg_cache_clear(void);
void prg_cache_load(void);

#endif
