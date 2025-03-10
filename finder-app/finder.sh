#!/bin/sh

if [ $# -lt 2 ]; then
	echo "finder require 2 arguments"
	exit 1
fi

if [ ! -d $1 ]; then
	echo "first argument should be a valid directory"
	exit 1
fi

nlines=$(grep -r $2 $1 | wc -l)
nfiles=$(grep -rl $2 $1 | wc -l)

echo "The number of files are $nfiles and the number of matching lines are $nlines"
