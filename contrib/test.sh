#!/bin/sh

IOCAT=/home/tat/work/KL/klone/contrib/iocat

export FQN="$1"

$IOCAT -e < "$1" | $IOCAT -d | diff - "$1"

if [ $? -eq 0 ]; then
    echo -n "."
else
    echo 
    echo "$1"
    echo
fi
