#!/bin/bash

[ -z "$CC" ] && {
    KERNEL_R=$(uname -r | cut -d '-' -f1)
    if [ "$KERNEL_R" = "2.6.9" ]; then
	export CC=gcc-3.4.6
    else
	export CC=gcc-4.1.2
    fi
}
echo "kernel CC=$CC (must be compiler a current kernel is compiled by)"

make CC=$CC
make CC=$CC modules_install
depmod
