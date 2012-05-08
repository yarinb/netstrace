/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id$
 */

#include "defs.h"

#include "prg_cache.h"
#include "netstat-util.h"
#include "net-support.h"

#include <fcntl.h>
#if HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef HAVE_LONG_LONG_OFF_T
/*
 * Hacks for systems that have a long long off_t
 */

#define sys_pread64	sys_pread
#define sys_pwrite64	sys_pwrite
#endif

int
sys_read(struct tcb *tcp)
{
  struct socket_info sockinfo;
  if (entering(tcp)) {
    json_object_object_add(tcp->json, "fd", json_object_new_int((int)tcp->u_arg[0]));
    printfd(tcp, tcp->u_arg[0]);
    tprintf(", ");
    if (output_json) {
      if (get_socket_info(tcp->pid, (int) tcp->u_arg[0], &sockinfo) == 0) {
        append_to_json(tcp->json, &sockinfo);
      } else {
        json_object_object_add(tcp->json, "pid", json_object_new_int(tcp->pid));
      }
    }
  } else {
    /* exiting... */
    if (syserror(tcp)) {
      	tprintf("%#lx", tcp->u_arg[1]);
    } else {
      /* no error */
      if (output_json) {
        json_object_object_add(tcp->json, "content",
            json_object_new_string(readstr(tcp, tcp->u_arg[1], tcp->u_arg[2])));
        json_object_object_add(tcp->json, "length", json_object_new_int(tcp->u_arg[2]));
      } else {
        printstr(tcp, tcp->u_arg[1], tcp->u_rval);
      }
    }
    tprintf(", %lu", tcp->u_arg[2]);
  }
  return 0;
}

int
sys_write(struct tcb *tcp)
{
  struct socket_info sockinfo;
	if (entering(tcp)) {
    if (get_socket_info(tcp->pid, (int) tcp->u_arg[0], &sockinfo) == 0) {
      append_to_json(tcp->json, &sockinfo);
		} else {
			json_object_object_add(tcp->json, "pid", json_object_new_int(tcp->pid));
		}
    json_object_object_add(tcp->json, "fd", json_object_new_int(tcp->u_arg[0]));

    json_object_object_add(tcp->json, "content",
          json_object_new_string(readstr(tcp, tcp->u_arg[1], tcp->u_arg[2])));

    json_object_object_add(tcp->json, "length", json_object_new_int(tcp->u_arg[2]));
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printstr(tcp, tcp->u_arg[1], tcp->u_arg[2]);
		tprintf(", %lu", tcp->u_arg[2]);
	}
	return 0;
}

#if HAVE_SYS_UIO_H
int
readstr_iov(tcp, len, addr, result, len_sum)
struct tcb * tcp;
unsigned long len;
unsigned long addr;
char **result;
int *len_sum;
{

  char *strp;
	char **resp = result;
	int count = 0, leng = 0;

#if defined(LINUX) && SUPPORTED_PERSONALITIES > 1
	union {
		struct { u_int32_t base; u_int32_t len; } iov32;
		struct { u_int64_t base; u_int64_t len; } iov64;
	} iov;
#define sizeof_iov \
  (personality_wordsize[current_personality] == 4 \
   ? sizeof(iov.iov32) : sizeof(iov.iov64))
#define iov_iov_base \
  (personality_wordsize[current_personality] == 4 \
   ? (u_int64_t) iov.iov32.base : iov.iov64.base)
#define iov_iov_len \
  (personality_wordsize[current_personality] == 4 \
   ? (u_int64_t) iov.iov32.len : iov.iov64.len)
#else
	struct iovec iov;
#define sizeof_iov sizeof(iov)
#define iov_iov_base iov.iov_base
#define iov_iov_len iov.iov_len
#endif
	unsigned long size, cur, end, abbrev_end;
	int failed = 0;

	if (!len) {
		/* tprintf("[]"); */
		return 0;
	}
	size = len * sizeof_iov;
	end = addr + size;
	if (!verbose(tcp) || size / sizeof_iov != len || end < addr) {
		/* tprintf("%#lx", addr); */
		return 0;
	}
	if (abbrev(tcp)) {
		abbrev_end = addr + max_strlen * sizeof_iov;
		if (abbrev_end < addr)
			abbrev_end = end;
	} else {
		abbrev_end = end;
	}
  *len_sum = 0;
	/* tprintf("["); */
	/* resp = (char **) malloc(sizeof(char**) * (end-addr)/sizeof_iov + 1); */
	for (cur = addr; cur < end; cur += sizeof_iov) {
		/* if (cur > addr) */
			/* tprintf(", "); */
		if (cur >= abbrev_end) {
			/* tprintf("..."); */
			break;
		}
		if (umoven(tcp, cur, sizeof_iov, (char *) &iov) < 0) {
			/* tprintf("?"); */
			failed = 1;
			break;
		}
		/* tprintf("{"); */
		strp = readstr(tcp, (long) iov_iov_base, iov_iov_len);
    leng = strlen(strp);
    *resp = (char *) malloc(sizeof(char) * leng + 1);
    strncpy(*resp, strp, leng+1);

    *len_sum += iov_iov_len;
    /* increment result pointer to next array index */
    resp++;
		count++;
		/* tprintf(", %lu}", (unsigned long)iov_iov_len); */
	}

	return count;
	/* tprintf("]"); */
	/* if (failed) */
	/* 	tprintf(" %#lx", addr); */
#undef sizeof_iov
#undef iov_iov_base
#undef iov_iov_len
}
#endif

#if HAVE_SYS_UIO_H
void
tprint_iov(tcp, len, addr)
struct tcb * tcp;
unsigned long len;
unsigned long addr;
{
#if defined(LINUX) && SUPPORTED_PERSONALITIES > 1
	union {
		struct { u_int32_t base; u_int32_t len; } iov32;
		struct { u_int64_t base; u_int64_t len; } iov64;
	} iov;
#define sizeof_iov \
  (personality_wordsize[current_personality] == 4 \
   ? sizeof(iov.iov32) : sizeof(iov.iov64))
#define iov_iov_base \
  (personality_wordsize[current_personality] == 4 \
   ? (u_int64_t) iov.iov32.base : iov.iov64.base)
#define iov_iov_len \
  (personality_wordsize[current_personality] == 4 \
   ? (u_int64_t) iov.iov32.len : iov.iov64.len)
#else
	struct iovec iov;
#define sizeof_iov sizeof(iov)
#define iov_iov_base iov.iov_base
#define iov_iov_len iov.iov_len
#endif
	unsigned long size, cur, end, abbrev_end;
	int failed = 0;

	if (!len) {
		tprintf("[]");
		return;
	}
	size = len * sizeof_iov;
	end = addr + size;
	if (!verbose(tcp) || size / sizeof_iov != len || end < addr) {
		tprintf("%#lx", addr);
		return;
	}
	if (abbrev(tcp)) {
		abbrev_end = addr + max_strlen * sizeof_iov;
		if (abbrev_end < addr)
			abbrev_end = end;
	} else {
		abbrev_end = end;
	}
	tprintf("[");
	for (cur = addr; cur < end; cur += sizeof_iov) {
		if (cur > addr)
			tprintf(", ");
		if (cur >= abbrev_end) {
			tprintf("...");
			break;
		}
		if (umoven(tcp, cur, sizeof_iov, (char *) &iov) < 0) {
			tprintf("?");
			failed = 1;
			break;
		}
		tprintf("{");
		printstr(tcp, (long) iov_iov_base, iov_iov_len);
		tprintf(", %lu}", (unsigned long)iov_iov_len);
	}
	tprintf("]");
	if (failed)
		tprintf(" %#lx", addr);
#undef sizeof_iov
#undef iov_iov_base
#undef iov_iov_len
}

int
sys_readv(struct tcb *tcp)
{
  struct socket_info sockinfo;
  char *strings[100];
  char *buf = NULL;
  int i, size, buf_size = 0, write_bytes, len_sum;
  void *lastp = buf;

  if (entering(tcp)) {
    /* printfd(tcp, tcp->u_arg[0]); */
    /* tprintf(", "); */
    if (get_socket_info(tcp->pid, (int) tcp->u_arg[0], &sockinfo) == 0) {
      append_to_json(tcp->json, &sockinfo);
    } else {
      json_object_object_add(tcp->json, "pid", json_object_new_int(tcp->pid));
    }
  } else {
    if (syserror(tcp)) {
      tprintf("%#lx, %lu", tcp->u_arg[1], tcp->u_arg[2]);
      return 0;
    } else {
      tprint_iov(tcp, tcp->u_arg[2], tcp->u_arg[1]);
      if (output_json) {
          json_object_object_add(tcp->json, "fd", json_object_new_int(tcp->u_arg[0]));
        size = readstr_iov(tcp, tcp->u_arg[2], tcp->u_arg[1], strings, &len_sum);
        buf = (char *) malloc(sizeof(char) * max_strlen);
        for (i = 0; i< size; i++) {

          if (i == 0) {
            lastp = (void *) buf;
          }
          write_bytes = MIN(strlen(strings[i]), max_strlen - buf_size);
          lastp = mempcpy(lastp, strings[i], write_bytes);
          buf_size += write_bytes;
          free(strings[i]);
        }
        if (buf) {
          json_object_object_add(tcp->json, "content", json_object_new_string_len(buf, buf_size));
          json_object_object_add(tcp->json, "length", json_object_new_int(len_sum));
          free(buf);
        }
      }
    }
    tprint_iov(tcp, tcp->u_arg[2], tcp->u_arg[1]);
    tprintf(", %lu", tcp->u_arg[2]);
  }
  return 0;
}

int
sys_writev(struct tcb *tcp)
{

  struct socket_info sockinfo;
  char *strings[100];

  char *buf = NULL;
  void *lastp = (void*) buf;
  void *writep;
  int i, size, buf_size = 0, len_sum = 0, write_bytes, len;
  
  /* printf("entering writev"); */
  if (entering(tcp)) {
    printfd(tcp, tcp->u_arg[0]);
    tprintf(", ");
    tprint_iov(tcp, tcp->u_arg[2], tcp->u_arg[1]);
    tprintf(", %lu", tcp->u_arg[2]);
    if (output_json) {
      if (get_socket_info(tcp->pid, (int) tcp->u_arg[0], &sockinfo) == 0) {
        append_to_json(tcp->json, &sockinfo);
      } else {
        json_object_object_add(tcp->json, "pid", json_object_new_int(tcp->pid));
      }
      json_object_object_add(tcp->json, "fd", json_object_new_int(tcp->u_arg[0]));
      size = readstr_iov(tcp, tcp->u_arg[2], tcp->u_arg[1], strings, &len_sum);
      printf("readstr_iov size %d len_sum %d\n", size, len_sum); 

      buf = (char *) malloc(sizeof(char)*(max_strlen + suffix_strlen + 1));
      for (i = 0; i< size; i++) {
        if (i == 0) {
          lastp = (void *) buf;
        }
        if (buf_size < max_strlen) {
          len = strlen(strings[i]);
        printf("buf_size=%d, len=%d\n", buf_size, len);
          write_bytes = MIN(len, max_strlen - buf_size);
          lastp = mempcpy(lastp, strings[i], write_bytes);
          buf_size += write_bytes;
        } else if (i == size - 1) {
          // Append the last bytes of the write to the payload if the payload is larger then max_strlen
          len = strlen(strings[i]);
          write_bytes = MIN(len, suffix_strlen);
          writep = len < suffix_strlen ? strings[i] : strings[i] + (len - suffix_strlen);
          printf("adding to the end: %s\n", writep); 
          lastp = memcpy(lastp, writep, write_bytes);
          buf_size += write_bytes;
        }
        /* printf("lasp %d\n", &lastp); */
        free(strings[i]);
      }
      /* *((char *)lastp) = 0; */
      if (buf) {
        json_object_object_add(tcp->json, "content", json_object_new_string_len(buf, buf_size));
        json_object_object_add(tcp->json, "length", json_object_new_int(len_sum));
        free(buf);
      }
    }
    tprintf(", %lu", tcp->u_arg[2]);
  }
  return 0;
}
#endif

#if defined(SVR4)

int
sys_pread(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
	} else {
		if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[1]);
		else
			printstr(tcp, tcp->u_arg[1], tcp->u_rval);
#if UNIXWARE
		/* off_t is signed int */
		tprintf(", %lu, %ld", tcp->u_arg[2], tcp->u_arg[3]);
#else
		tprintf(", %lu, %llu", tcp->u_arg[2],
			LONG_LONG(tcp->u_arg[3], tcp->u_arg[4]));
#endif
	}
	return 0;
}

int
sys_pwrite(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printstr(tcp, tcp->u_arg[1], tcp->u_arg[2]);
#if UNIXWARE
		/* off_t is signed int */
		tprintf(", %lu, %ld", tcp->u_arg[2], tcp->u_arg[3]);
#else
		tprintf(", %lu, %llu", tcp->u_arg[2],
			LONG_LONG(tcp->u_arg[3], tcp->u_arg[4]));
#endif
	}
	return 0;
}
#endif /* SVR4 */

#ifdef FREEBSD
#include <sys/types.h>
#include <sys/socket.h>

int
sys_sendfile(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printfd(tcp, tcp->u_arg[1]);
		tprintf(", %llu, %lu",
			LONG_LONG(tcp->u_arg[2], tcp->u_arg[3]),
			tcp->u_arg[4]);
	} else {
		off_t offset;

		if (!tcp->u_arg[5])
			tprintf(", NULL");
		else {
			struct sf_hdtr hdtr;

			if (umove(tcp, tcp->u_arg[5], &hdtr) < 0)
				tprintf(", %#lx", tcp->u_arg[5]);
			else {
				tprintf(", { ");
				tprint_iov(tcp, hdtr.hdr_cnt, hdtr.headers);
				tprintf(", %u, ", hdtr.hdr_cnt);
				tprint_iov(tcp, hdtr.trl_cnt, hdtr.trailers);
				tprintf(", %u }", hdtr.hdr_cnt);
			}
		}
		if (!tcp->u_arg[6])
			tprintf(", NULL");
		else if (umove(tcp, tcp->u_arg[6], &offset) < 0)
			tprintf(", %#lx", tcp->u_arg[6]);
		else
			tprintf(", [%llu]", offset);
		tprintf(", %lu", tcp->u_arg[7]);
	}
	return 0;
}
#endif /* FREEBSD */

#ifdef LINUX

/* The SH4 ABI does allow long longs in odd-numbered registers, but
   does not allow them to be split between registers and memory - and
   there are only four argument registers for normal functions.  As a
   result pread takes an extra padding argument before the offset.  This
   was changed late in the 2.4 series (around 2.4.20).  */
#if defined(SH)
#define PREAD_OFFSET_ARG 4
#else
#define PREAD_OFFSET_ARG 3
#endif

int
sys_pread(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
	} else {
		if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[1]);
		else
			printstr(tcp, tcp->u_arg[1], tcp->u_rval);
		tprintf(", %lu, ", tcp->u_arg[2]);
		printllval(tcp, "%llu", PREAD_OFFSET_ARG);
	}
	return 0;
}

int
sys_pwrite(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printstr(tcp, tcp->u_arg[1], tcp->u_arg[2]);
		tprintf(", %lu, ", tcp->u_arg[2]);
		printllval(tcp, "%llu", PREAD_OFFSET_ARG);
	}
	return 0;
}

int
sys_sendfile(struct tcb *tcp)
{
	if (entering(tcp)) {
		off_t offset;

		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printfd(tcp, tcp->u_arg[1]);
		tprintf(", ");
		if (!tcp->u_arg[2])
			tprintf("NULL");
		else if (umove(tcp, tcp->u_arg[2], &offset) < 0)
			tprintf("%#lx", tcp->u_arg[2]);
		else
			tprintf("[%lu]", offset);
		tprintf(", %lu", tcp->u_arg[3]);
	}
	return 0;
}

int
sys_sendfile64(struct tcb *tcp)
{
	if (entering(tcp)) {
		loff_t offset;

		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printfd(tcp, tcp->u_arg[1]);
		tprintf(", ");
		if (!tcp->u_arg[2])
			tprintf("NULL");
		else if (umove(tcp, tcp->u_arg[2], &offset) < 0)
			tprintf("%#lx", tcp->u_arg[2]);
		else
			tprintf("[%llu]", (unsigned long long int) offset);
		tprintf(", %lu", tcp->u_arg[3]);
	}
	return 0;
}

#endif /* LINUX */

#if _LFS64_LARGEFILE || HAVE_LONG_LONG_OFF_T
int
sys_pread64(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
	} else {
		if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[1]);
		else
			printstr(tcp, tcp->u_arg[1], tcp->u_rval);
		tprintf(", %lu, ", tcp->u_arg[2]);
		printllval(tcp, "%#llx", 3);
	}
	return 0;
}

int
sys_pwrite64(struct tcb *tcp)
{
	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printstr(tcp, tcp->u_arg[1], tcp->u_arg[2]);
		tprintf(", %lu, ", tcp->u_arg[2]);
		printllval(tcp, "%#llx", 3);
	}
	return 0;
}
#endif

int
sys_ioctl(struct tcb *tcp)
{
	const struct ioctlent *iop;

	if (entering(tcp)) {
		printfd(tcp, tcp->u_arg[0]);
		tprintf(", ");
		iop = ioctl_lookup(tcp->u_arg[1]);
		if (iop) {
			tprintf("%s", iop->symbol);
			while ((iop = ioctl_next_match(iop)))
				tprintf(" or %s", iop->symbol);
		} else
			tprintf("%#lx", tcp->u_arg[1]);
		ioctl_decode(tcp, tcp->u_arg[1], tcp->u_arg[2]);
	}
	else {
		int ret;
		if (!(ret = ioctl_decode(tcp, tcp->u_arg[1], tcp->u_arg[2])))
			tprintf(", %#lx", tcp->u_arg[2]);
		else
			return ret - 1;
	}
	return 0;
}
