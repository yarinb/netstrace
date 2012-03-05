#!/bin/sh

for file in `cat .gitignore`; do rm -rf $file; done 
