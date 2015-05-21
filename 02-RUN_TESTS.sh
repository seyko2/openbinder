#!/bin/bash

BINDER_PRESENT=$(lsmod | grep binderdev)

[ -z "$BINDER_PRESENT" ] && {
    echo "Warning: a binderdev kernel module not found."
    sleep 2
}

make test
