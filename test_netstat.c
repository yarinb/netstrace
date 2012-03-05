#include "netstat_config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "net-support.h"
#include "netstat-util.h"

int
main(int argc, char *argv[])
{
  struct socket_info sock;
  int inode = atoi(argv[1]);
  printf("Recieved inode %d\n", inode);

  prg_cache_load();

  int ret = get_tcp_info(inode, &sock);
  if (!ret) {
    printf("sock->pid: %d", sock.pid);
  } else {
    printf("ERROR");
  }

  return 0;
}
