#!/bin/sh

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

yesno ()
{
    /bin/echo -n "$1 ([yY] or [nN]) "
    
    while [ true ] 
    do 
        read answer
        case ${answer} in
            [Yy]) return 0 ;;
            [nN]) return 1 ;;
            *) /bin/echo -n "please say [yY] or [nN]: " ;; 
        esac
    done 
}

DISTDIR="/tmp/klone-dist"

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

TARBALL="${DISTDIR}/klone/skin/default/klone-`cat VERSION`.tar.gz"

E "============================================================================"
E "klone tarball ready:                                                        "
E "     ${TARBALL}"
E "============================================================================"

yesno "===> upload tarball to kl.com ?" || exit 0

wrap "uploading klone tarball"  \
    scp ${TARBALL}* root@koanlogic.com:/var/www-anemic/www/download/klone/

exit 0
