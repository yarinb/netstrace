#ifndef _NETSTAT_UTIL_H_
#define _NETSTAT_UTIL_H_

#include <stddef.h>

#define _(str) (str)

int nstrcmp(const char *, const char *);

char *safe_strncpy(char *dst, const char *src, size_t size); 

#define netmin(a,b) ((a)<(b) ? (a) : (b))
#define netmax(a,b) ((a)>(b) ? (a) : (b))
#endif
