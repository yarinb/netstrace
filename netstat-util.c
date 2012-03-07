#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/dir.h>

#include "netstat-util.h"

char *safe_strncpy(char *dst, const char *src, size_t size)
{   
    dst[size-1] = '\0';
    return strncpy(dst,src,size-1);   
}


/* like strcmp(), but knows about numbers */
int nstrcmp(const char *astr, const char *b)
{
    const char *a = astr;

    while (*a == *b) {
	if (*a == '\0')
	    return 0;
	a++;
	b++;
    }
    if (isdigit(*a)) {
	if (!isdigit(*b))
	    return -1;
	while (a > astr) {
	    a--;
	    if (!isdigit(*a)) {
		a++;
		break;
	    }
	    if (!isdigit(*b))
		return -1;
	    b--;
	}
	return atoi(a) > atoi(b) ? 1 : -1;
    }
    return *a - *b;
}

#define PRG_SOCKET_PFX    "socket:["
#define PRG_SOCKET_PFXl (strlen(PRG_SOCKET_PFX))
#define PRG_SOCKET_PFX2   "[0000]:"
#define PRG_SOCKET_PFX2l  (strlen(PRG_SOCKET_PFX2))
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
int resolve_inode(pid_t pid, int fd, unsigned long *inode)
{
	
    int lnamelen;
    char lname[30];


		char fdfile[20];
		snprintf(fdfile, sizeof(fdfile), "/proc/%d/fd/%d", pid, fd);
		lnamelen = readlink(fdfile, lname, sizeof(lname)-1);
		if (lnamelen == -1) {
			fprintf(stderr, "Failed to read link of %s\n", fdfile);
			return 1;
		}
		lname[lnamelen] = '\0';

		if (extract_type_1_socket_inode(lname, inode) < 0)
			if (extract_type_2_socket_inode(lname, inode) < 0) {
				fprintf(stderr, "Failed to extract inode from %s\n", fdfile);
				return 1;
			}

		return 0;

}
