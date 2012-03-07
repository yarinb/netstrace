#!/bin/sh
rm -f netstat-util.o prg_cache.o af.o unix.o inet4.o inet6.o netstat.o test_netstat.o ttt
gcc -Wall -g -O netstat-util.c prg_cache.c af.c unix.c inet4.c inet6.c netstat.c test_netstat.c -o ttt -DDEBUG
