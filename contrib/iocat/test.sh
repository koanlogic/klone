#!/bin/sh

IOCAT=iocat
CODECS="-z -c"

export FQN="$1"

$IOCAT -e $CODECS < "$1" | $IOCAT -d $CODECS | diff - "$1"

if [ $? -eq 0 ]; then
    echo -n "."
else
    echo 
    echo "$1"
    echo
fi
