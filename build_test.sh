#!/bin/sh
gcc -Wall -g -O netstat-util.c af.c unix.c inet4.c inet6.c netstat.c test_netstat.c -o ttt -DDEBUG
