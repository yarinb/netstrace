#include <sys/types.h>
#include "netstat_config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "net-support.h"
#include "netstat-util.h"

int
main(int argc, char *argv[])
{
  struct socket_info sock;
  int inode = atoi(argv[1]);
  printf("Recieved inode %d\n", inode);


  int ret = get_tcp_info(inode, &sock);
  if (!ret && sock.pid != -1) {
    printf("Detailed information on socket number %d:\n", sock.pid);
    printf("local address: %s:%d\n", sock.laddress, sock.lport);
    printf("remote address: %s:%d\n", sock.raddress, sock.rport);
  } else {
    printf("ERROR");
  }

  return 0;
}
