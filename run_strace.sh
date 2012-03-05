#!/bin/bash
./strace -s 1024 -f -e trace=$@ curl -s http://google.co.il
