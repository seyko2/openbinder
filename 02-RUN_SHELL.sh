#!/bin/sh

BINDER_PRESENT=$(lsmod | grep binderdev)

[ -z "$BINDER_PRESENT" ] && {
    echo "Warning: a binderdev kernel module not found."
    sleep 2
}

# make LOCK_DEBUG=-1 runshell
make LOCK_DEBUG=0 runshell
