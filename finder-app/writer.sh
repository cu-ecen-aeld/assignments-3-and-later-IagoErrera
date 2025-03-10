#!/bin/sh

if [ $# -lt 2 ]; then
	echo "writer require 2 arguments"
	exit 1
fi

writefile=$1
writestr=$2

writefolder=$(dirname $writefile)

if [ ! -d $writefolder ]; then
	mkdir -p $writefolder
fi

touch $writefile

if [ ! $? -eq 0 ]; then
	exit 1
fi

echo $writestr > $writefile

if [ ! $? -eq 0 ]; then
	exit 1
fi

