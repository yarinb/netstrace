#include <sys/socket.h>

#define FLAG_EXT       3		/* AND-Mask */
#define FLAG_NUM_HOST  4
#define FLAG_NUM_PORT  8
#define FLAG_NUM_USER 16
#define FLAG_NUM     (FLAG_NUM_HOST|FLAG_NUM_PORT|FLAG_NUM_USER)
#define FLAG_SYM      32
#define FLAG_CACHE    64
#define FLAG_FIB     128
#define FLAG_VERBOSE 256

struct socket_info {
	pid_t pid;
	int fd;
  char laddress[128];     /* local socket address */
  int lport;          /* local socket port */
  char raddress[128];     /* remote socket address */
  int rport;          /* remote socket port */
  int sa_family;
  unsigned long inode;
};

/* This structure defines protocol families and their handlers. */
struct aftype {
    const char *name;
    char *title;
    int af;
    int alen;
    char *(*print) (unsigned char *);
    char *(*sprint) (struct sockaddr *, int numeric);
    int (*input) (int type, char *bufp, struct sockaddr *);
    void (*herror) (char *text);

    /* may modify src */
    int (*getmask) (char *src, struct sockaddr * mask, char *name);

    int fd;
    const char *flag_file;
};

extern struct aftype *aftypes[];

extern struct aftype *get_aftype(const char *name);
extern struct aftype *get_afntype(int type);

extern int get_socket_for_af(int af);

extern int INET_rinput(int action, int flags, char **argv);
extern int INET6_rinput(int action, int flags, char **argv);


extern int aftrans_opt(const char *arg);
extern void aftrans_def(char *tool, char *argv0, char *dflt);

extern const char *get_sname(int socknumber, const char *proto, int numeric);


extern int flag_unx;
extern int flag_inet;
extern int flag_inet6;

extern char afname[];
extern int get_tcp_info(unsigned long inode, struct socket_info *sock_info);
