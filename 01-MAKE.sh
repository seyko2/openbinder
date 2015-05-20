#!/bin/sh

# g++-3.4.6 was used to compile the sources of the openbinder
make all docs nomodules
[ $? -ne 0 ] && {
    echo FAIL
    exit
}

cd modules/binder
./01-MAKE.sh

[ $? -ne 0 ] && {
    echo FAIL
    exit
}

echo OK
