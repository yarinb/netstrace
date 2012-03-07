#include "prg_cache.h"

struct prg_node *prg_hash[PRG_HASH_SIZE];

static char prg_cache_loaded = 0;
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

static struct prg_node *_cache_get_prg(unsigned long inode)
{
  unsigned hi=PRG_HASHIT(inode);
  struct prg_node *pn;

  for (pn=prg_hash[hi];pn;pn=pn->next)
    if (pn->inode==inode) {
#if DEBUG
      printf("found matching pid for inode %lu: [%d]", inode, pn->pid );
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
  /* yarinb - handle permission issues in strace main */
  /* else */
    /* fprintf(stderr, _("(Not all processes could be identified, non-owned process info\n" */
    /*       " will not be shown, you would have to be root to see it all.)\n")); */
}
