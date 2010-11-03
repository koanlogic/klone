#!/bin/sh

KLONE_VER="`cat VERSION`"

E ()
{
    echo "\033[1;37m\033[42m $@ \033[0m"
    tput sgr0
}

err ()
{
    E "####################################################################"
    E "# Error: $@"
    E "####################################################################"

    exit 1
}

wrap ()
{
    msg=$1 ; shift ; cmd=$@

    E "$ "$cmd""
    eval ${cmd} 2>/dev/null || err $msg
}

DISTDIR="/tmp/klone-2.3.0-dist"

wrap "deleting dist directory $DISTDIR" \
    rm -rf $DISTDIR
wrap "creating dist directory $DISTDIR" \
    mkdir $DISTDIR 
wrap "changing directory to $DISTDIR" \
    cd $DISTDIR

# clone fresh repos
wrap "cloning makl git repository" \
    git clone file:///${HOME}/work/GIT/makl
wrap "cloning libu git repository" \
    git clone file:///${HOME}/work/GIT/libu
wrap "cloning klone git repository" \
    git clone file:///${HOME}/work/GIT/klone

wrap "moving into to klone subdirectory" \
    cd klone
wrap "pull in libu" \
    ln -s ../libu 
wrap "pull in makl" \
    ln -s ../makl

wrap "moving to dist directory" \
    cd skin/default 
wrap "launching dist make instance" \
    make -f Makefile.dist

echo
E "============================================================================"
E "klone tarball ready:                                                        "
E "     ${DISTDIR}/klone/skin/default/klone-${KLONE_VER}.tar.gz"
E "============================================================================"
echo

exit 0
