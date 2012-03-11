#!/bin/bash
./strace -f -e trace=$@ curl -s http://google.co.il
