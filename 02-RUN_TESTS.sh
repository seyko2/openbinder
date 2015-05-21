#!/bin/bash

BINDER_PRESENT=$(lsmod | grep binderdev)
[ -z "$BINDER_PRESENT" ] && {
    modprobe binderdev
    BINDER_PRESENT=$(lsmod | grep binderdev)
}

[ -z "$BINDER_PRESENT" ] && {
    echo "A binderdev kernel module not found."
    echo "Please go to the modules/binder and make it."
    exit
}

make test
