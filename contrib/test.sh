#!/bin/sh

IOCAT=/Users/tat/work/cxh2/contrib/iocat 

$IOCAT -e < "$1" | $IOCAT -d | diff - "$1"
# $IOCAT < "$1" | diff - "$1"

if [ $? -eq 0 ]; then
    echo -n "."
else
    echo 
    echo "$1"
    echo
fi
