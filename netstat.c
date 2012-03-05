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

#include "net-support.h"
#include "netstat-util.h"
#include "netstat_config.h"

/* yarinb - compat def for unsupported i18n 
 * no gettext() */
#define _(str) (str)
#define PROGNAME_WIDTH 20
#define PRG_HASH_SIZE 211
static struct prg_node {
    struct prg_node *next;
    unsigned long inode;
    char name[PROGNAME_WIDTH];
    pid_t pid;
} *prg_hash[PRG_HASH_SIZE];

static char prg_cache_loaded = 0;

#define PRG_HASHIT(x) ((x) % PRG_HASH_SIZE)

#define PRG_LOCAL_ADDRESS "local_address"
#define PRG_INODE	 "inode"
#define PRG_SOCKET_PFX    "socket:["
#define PRG_SOCKET_PFXl (strlen(PRG_SOCKET_PFX))
#define PRG_SOCKET_PFX2   "[0000]:"
#define PRG_SOCKET_PFX2l  (strlen(PRG_SOCKET_PFX2))

#ifndef LINE_MAX
#define LINE_MAX 4096
#endif

#define PATH_PROC	   "/proc"
#define PATH_FD_SUFF	"fd"
#define PATH_FD_SUFFl       strlen(PATH_FD_SUFF)
#define PATH_PROC_X_FD      PATH_PROC "/%s/" PATH_FD_SUFF

#define _PATH_PROCNET_TCP		"/proc/net/tcp"
#define _PATH_PROCNET_TCP6		"/proc/net/tcp6"
#define _PATH_PROCNET_UDP		"/proc/net/udp"
#define _PATH_PROCNET_UDP6		"/proc/net/udp6"
#define _PATH_PROCNET_RAW		"/proc/net/raw"
#define _PATH_PROCNET_RAW6		"/proc/net/raw6"

#define PATH_CMDLINE	"cmdline"
#define PATH_CMDLINEl       strlen(PATH_CMDLINE)

int flag_not = FLAG_NUM;

FILE *procinfo;

#define INFO_GUTS1(file,name,proc)			\
  procinfo = proc_fopen((file));			\
  if (procinfo == NULL) {				\
    if (errno != ENOENT) {				\
      perror((file));					\
      return -1;					\
    }							\
    if (flag_arg || flag_ver)				\
      ESYSNOT("netstat", (name));			\
    if (flag_arg)					\
      rc = 1;						\
  } else {						\
    do {						\
      if (fgets(buffer, sizeof(buffer), procinfo))	\
        (proc)(lnr++, buffer);				\
    } while (!feof(procinfo));				\
    fclose(procinfo);					\
  }

#if HAVE_AFINET6
#define INFO_GUTS2(file,proc)				\
  lnr = 0;						\
  procinfo = proc_fopen((file));		       	\
  if (procinfo != NULL) {				\
    do {						\
      if (fgets(buffer, sizeof(buffer), procinfo))	\
	(proc)(lnr++, buffer);				\
    } while (!feof(procinfo));				\
    fclose(procinfo);					\
  }
#else
#define INFO_GUTS2(file,proc)
#endif

#define INFO_GUTS3					\
 return rc;

#define INFO_GUTS6(file,file6,name,proc)		\

#define INFO_GUTS(file,name,proc)			\
 char buffer[8192];					\
 int rc = 0;						\
 int lnr = 0;						\
 INFO_GUTS1(file,name,proc)				\
 INFO_GUTS3


FILE *proc_fopen(const char *name)
{
    static char *buffer;
    static size_t pagesz;
    FILE *fd = fopen(name, "r");

    if (fd == NULL)
      return NULL;
      
    if (!buffer) {
      pagesz = getpagesize();
      buffer = malloc(pagesz);
    }
    
    setvbuf(fd, buffer, _IOFBF, pagesz);
    return fd;
}

void prg_cache_add(pid_t pid, unsigned long inode, char *name)
{
  unsigned hi = PRG_HASHIT(inode);
  struct prg_node **pnp,*pn;

  prg_cache_loaded=2;
  for (pnp=prg_hash+hi;(pn=*pnp);pnp=&pn->next) {
    if (pn->inode==inode) {
      /* Some warning should be appropriate here
         as we got multiple processes for one i-node */
      return;
    }
  }
  if (!(*pnp=malloc(sizeof(**pnp)))) 
    return;
  pn=*pnp;
  pn->next=NULL;
  pn->inode=inode;
  pn->pid=pid;
  if (strlen(name)>sizeof(pn->name)-1) 
    name[sizeof(pn->name)-1]='\0';
  strcpy(pn->name,name);
}

struct prg_node *_cache_get_prg(unsigned long inode)
{
  unsigned hi=PRG_HASHIT(inode);
  struct prg_node *pn;

  for (pn=prg_hash[hi];pn;pn=pn->next)
    if (pn->inode==inode) {
#if DEBUG
      printf("found matching pid for inode %d: [%d]", inode, pn->pid );
#endif
      return(pn);
    }

  return NULL;
}

const char *prg_cache_get_name(unsigned long inode)
{

  struct prg_node *pn;
  pn = _cache_get_prg(inode);
  if (pn == NULL)
    return("-");

  return pn->name;
}

pid_t prg_cache_get_pid(unsigned long inode)
{

  struct prg_node *pn;
  pn = _cache_get_prg(inode);
  if (pn == NULL)
    return -1;

  return pn->pid;
}

void prg_cache_clear(void)
{
    struct prg_node **pnp,*pn;

    if (prg_cache_loaded == 2)
	for (pnp=prg_hash;pnp<prg_hash+PRG_HASH_SIZE;pnp++)
	    while ((pn=*pnp)) {
		*pnp=pn->next;
		free(pn);
	    }
    prg_cache_loaded=0;
}

static int extract_type_1_socket_inode(const char lname[], unsigned long * inode_p) {

    /* If lname is of the form "socket:[12345]", extract the "12345"
       as *inode_p.  Otherwise, return -1 as *inode_p.
       */

    if (strlen(lname) < PRG_SOCKET_PFXl+3) return(-1);
    
    if (memcmp(lname, PRG_SOCKET_PFX, PRG_SOCKET_PFXl)) return(-1);
    if (lname[strlen(lname)-1] != ']') return(-1);

    {
        char inode_str[strlen(lname + 1)];  /* e.g. "12345" */
        const int inode_str_len = strlen(lname) - PRG_SOCKET_PFXl - 1;
        char *serr;

        strncpy(inode_str, lname+PRG_SOCKET_PFXl, inode_str_len);
        inode_str[inode_str_len] = '\0';
        *inode_p = strtoul(inode_str,&serr,0);
        if (!serr || *serr)
            return(-1);
    }
    return(0);
}



static int extract_type_2_socket_inode(const char lname[], unsigned long * inode_p) {

    /* If lname is of the form "[0000]:12345", extract the "12345"
       as *inode_p.  Otherwise, return -1 as *inode_p.
       */

    if (strlen(lname) < PRG_SOCKET_PFX2l+1) return(-1);
    if (memcmp(lname, PRG_SOCKET_PFX2, PRG_SOCKET_PFX2l)) return(-1);

    {
        char *serr;

        *inode_p=strtoul(lname + PRG_SOCKET_PFX2l,&serr,0);
        if (!serr || *serr)
            return(-1);
    }
    return(0);
}



void prg_cache_load(void)
{
    char line[LINE_MAX],eacces=0;
    int procfdlen,fd,cmdllen,lnamelen;
    char lname[30],cmdlbuf[512],finbuf[PROGNAME_WIDTH];
    unsigned long inode;
    const char *cs,*cmdlp;
    DIR *dirproc=NULL,*dirfd=NULL;
    struct dirent *direproc,*direfd;
    pid_t pid;

    if (prg_cache_loaded) return;
    prg_cache_loaded=1;
    cmdlbuf[sizeof(cmdlbuf)-1]='\0';
    if (!(dirproc=opendir(PATH_PROC))) goto fail;
    while (errno=0,direproc=readdir(dirproc)) {
	for (cs=direproc->d_name;*cs;cs++)
	    if (!isdigit(*cs)) 
		break;
	if (*cs) 
	    continue;
  /* yarinb: extract pid from direproc->d_name */
  pid = atoi(direproc->d_name);
	procfdlen=snprintf(line,sizeof(line),PATH_PROC_X_FD,direproc->d_name);
	if (procfdlen<=0 || procfdlen>=sizeof(line)-5) 
	    continue;
	errno=0;
	dirfd=opendir(line);
	if (! dirfd) {
	    if (errno==EACCES) 
		eacces=1;
	    continue;
	}
	line[procfdlen] = '/';
	cmdlp = NULL;
	while ((direfd = readdir(dirfd))) {
	    /* Skip . and .. */
	    if (!isdigit(direfd->d_name[0]))
		continue;
	    if (procfdlen+1+strlen(direfd->d_name)+1>sizeof(line)) 
		continue;
	    memcpy(line + procfdlen - PATH_FD_SUFFl, PATH_FD_SUFF "/",
		   PATH_FD_SUFFl+1);
	    strcpy(line + procfdlen + 1, direfd->d_name);
	    lnamelen=readlink(line,lname,sizeof(lname)-1);
            lname[lnamelen] = '\0';  /*make it a null-terminated string*/

            if (extract_type_1_socket_inode(lname, &inode) < 0)
              if (extract_type_2_socket_inode(lname, &inode) < 0)
                continue;

	    if (!cmdlp) {
		if (procfdlen - PATH_FD_SUFFl + PATH_CMDLINEl >= 
		    sizeof(line) - 5) 
		    continue;
		strcpy(line + procfdlen-PATH_FD_SUFFl, PATH_CMDLINE);
		fd = open(line, O_RDONLY);
		if (fd < 0) 
		    continue;
		cmdllen = read(fd, cmdlbuf, sizeof(cmdlbuf) - 1);
		if (close(fd)) 
		    continue;
		if (cmdllen == -1) 
		    continue;
		if (cmdllen < sizeof(cmdlbuf) - 1) 
		    cmdlbuf[cmdllen]='\0';
		if ((cmdlp = strrchr(cmdlbuf, '/'))) 
		    cmdlp++;
		else 
		    cmdlp = cmdlbuf;
	    }

	    snprintf(finbuf, sizeof(finbuf), "%s/%s", direproc->d_name, cmdlp);
	    prg_cache_add(pid, inode, finbuf);
	}
	closedir(dirfd); 
	dirfd = NULL;
    }
    if (dirproc) 
	closedir(dirproc);
    if (dirfd) 
	closedir(dirfd);
    if (!eacces) 
	return;
    if (prg_cache_loaded == 1) {
    fail:
	fprintf(stderr,_("(No info could be read for \"-p\": geteuid()=%d but you should be root.)\n"),
		geteuid());
    }
    else
	fprintf(stderr, _("(Not all processes could be identified, non-owned process info\n"
			 " will not be shown, you would have to be root to see it all.)\n"));
}
static int tcp_do_one(int lnr, const char *line, unsigned long inode, struct socket_info *sock_info)
{
  unsigned long rxq, txq, time_len, retr, _inode;
  int num, local_port, rem_port, d, state, uid, timer_run, timeout;
  char rem_addr[128], local_addr[128], buffer[1024], more[512];
  const char *protname;
  struct aftype *ap;
#if HAVE_AFINET6
  struct sockaddr_in6 localaddr, remaddr;
  char addr6[INET6_ADDRSTRLEN];
  struct in6_addr in6;
  extern struct aftype inet6_aftype;
#else
  struct sockaddr_in localaddr, remaddr;
#endif

  if (lnr == 0)
    return 1;

  num = sscanf(line,
      "%d: %64[0-9A-Fa-f]:%X %64[0-9A-Fa-f]:%X %X %lX:%lX %X:%lX %lX %d %d %lu %512s\n",
      &d, local_addr, &local_port, rem_addr, &rem_port, &state,
      &txq, &rxq, &timer_run, &time_len, &retr, &uid, &timeout, &_inode, more);

  /* yarinb: match only lines for a give inode */
  if (_inode != inode)
    return 1;
  if (strlen(local_addr) > 8) {
#if HAVE_AFINET6
    protname = "tcp6";
    /* Demangle what the kernel gives us */
    sscanf(local_addr, "%08X%08X%08X%08X",
        &in6.s6_addr32[0], &in6.s6_addr32[1],
        &in6.s6_addr32[2], &in6.s6_addr32[3]);
    inet_ntop(AF_INET6, &in6, addr6, sizeof(addr6));
    inet6_aftype.input(1, addr6, (struct sockaddr *) &localaddr);
    sscanf(rem_addr, "%08X%08X%08X%08X",
        &in6.s6_addr32[0], &in6.s6_addr32[1],
        &in6.s6_addr32[2], &in6.s6_addr32[3]);
    inet_ntop(AF_INET6, &in6, addr6, sizeof(addr6));
    inet6_aftype.input(1, addr6, (struct sockaddr *) &remaddr);
    localaddr.sin6_family = AF_INET6;
    remaddr.sin6_family = AF_INET6;
#endif
  } else {
    protname = "tcp";
    sscanf(local_addr, "%X",
        &((struct sockaddr_in *) &localaddr)->sin_addr.s_addr);
    sscanf(rem_addr, "%X",
        &((struct sockaddr_in *) &remaddr)->sin_addr.s_addr);
    ((struct sockaddr *) &localaddr)->sa_family = AF_INET;
    ((struct sockaddr *) &remaddr)->sa_family = AF_INET;
  }

  if (num < 11) {
    fprintf(stderr, _("warning, got bogus tcp line.\n"));
    return 1;
  }
    if ((ap = get_afntype(((struct sockaddr *) &localaddr)->sa_family)) == NULL) {
    fprintf(stderr, _("netstat: unsupported address family %d !\n"),
        ((struct sockaddr *) &localaddr)->sa_family);
    return 1;
  }

  safe_strncpy(local_addr, ap->sprint((struct sockaddr *) &localaddr, flag_not), sizeof(local_addr));
  safe_strncpy(rem_addr, ap->sprint((struct sockaddr *) &remaddr, flag_not), sizeof(rem_addr));
  if (/*rem_port*/1) {
    snprintf(buffer, sizeof(buffer), "%s", get_sname(htons(local_port), "tcp", flag_not & FLAG_NUM_PORT));

    /* yarinb - don't truncate ip addresses */
    if (0) {
      if ((strlen(local_addr) + strlen(buffer)) > 22)
        local_addr[22 - strlen(buffer)] = '\0';
    }

    strcat(local_addr, ":");
    strcat(local_addr, buffer);
    snprintf(buffer, sizeof(buffer), "%s",
        get_sname(htons(rem_port), "tcp", flag_not & FLAG_NUM_PORT));

    /* yarinb - don't truncate ip addresses */
    if (0) {
      if ((strlen(rem_addr) + strlen(buffer)) > 22)
        rem_addr[22 - strlen(buffer)] = '\0';
    }

    strcat(rem_addr, ":");
    strcat(rem_addr, buffer);

    sock_info->inode = inode;
    sock_info->localaddr = &localaddr;
    sock_info->remaddr = &remaddr;
    sock_info->pid = prg_cache_get_pid(inode);
    printf("prefix: %-4s  %-*s %-*s\n",
        protname, netmax(23,strlen(local_addr)), local_addr, netmax(23,strlen(rem_addr)), rem_addr);

    return 0;
  }
}

int get_tcp_info(unsigned long inode, struct socket_info *sock_info)
{
 char buffer[8192];					
 int lnr = 0;						

 procinfo = proc_fopen(_PATH_PROCNET_TCP);
 if (procinfo == NULL) {
  if (errno != ENOENT) {
    perror(_PATH_PROCNET_TCP);
    return 1;
  }
 }
  

  do {
    if (fgets(buffer, sizeof(buffer), procinfo)) {
      if (tcp_do_one(lnr++, buffer, inode, sock_info) == 0)
        return 0;

    }
  } while (!feof(procinfo));
 fclose(procinfo);

 lnr = 0;
 procinfo = proc_fopen(_PATH_PROCNET_TCP6);
 if (procinfo == NULL) {
  if (errno != ENOENT) {
    perror(_PATH_PROCNET_TCP6);
    return 1;
  }
 }

  do {
    if (fgets(buffer, sizeof(buffer), procinfo)) {
      if (tcp_do_one(lnr++, buffer, inode, sock_info) == 0)
        return 0;

    }
  } while (!feof(procinfo));
 fclose(procinfo);

 return -1;
}

