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

#include "prg_cache.h"
#include "net-support.h"
#include "netstat-util.h"
#include "netstat_config.h"

/* yarinb - compat def for unsupported i18n 
 * no gettext() */
#define _(str) (str)

#define _PATH_PROCNET_TCP		"/proc/net/tcp"
#define _PATH_PROCNET_TCP6		"/proc/net/tcp6"
#define _PATH_PROCNET_UDP		"/proc/net/udp"
#define _PATH_PROCNET_UDP6		"/proc/net/udp6"
#define _PATH_PROCNET_RAW		"/proc/net/raw"
#define _PATH_PROCNET_RAW6		"/proc/net/raw6"


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

    /* sock_info->laddress = local_addr; */
		strncpy(sock_info->laddress, local_addr, sizeof(char) * strlen(local_addr) + 1);
    sock_info->lport = local_port;
    /* strcat(local_addr, ":"); */
    /* strcat(local_addr, buffer); */
    /* snprintf(buffer, sizeof(buffer), "%s", */
    /*     get_sname(htons(rem_port), "tcp", flag_not & FLAG_NUM_PORT)); */

    /* yarinb - don't truncate ip addresses */
    if (0) {
      if ((strlen(rem_addr) + strlen(buffer)) > 22)
        rem_addr[22 - strlen(buffer)] = '\0';
    }

    /* strcat(rem_addr, ":"); */
    /* strcat(rem_addr, buffer); */

    sock_info->inode = inode;
    /* sock_info->raddress = rem_addr; */
		strncpy(sock_info->raddress, rem_addr, sizeof(char) * strlen(rem_addr)+1);
    sock_info->rport = rem_port;
    sock_info->pid = prg_cache_get_pid(inode);
    sock_info->sa_family = ((struct sockaddr *)&localaddr)->sa_family;
    printf("prefix: %-4s  %-*s:%d %-*s:%d\n",
        protname, netmax(23,strlen(local_addr)), local_addr, local_port, netmax(23,strlen(rem_addr)), rem_addr, rem_port);

    return 0;
  }
}

int get_tcp_info(unsigned long inode, struct socket_info *sock_info)
{
 char buffer[8192];					
 int lnr = 0;						

 prg_cache_load();
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

