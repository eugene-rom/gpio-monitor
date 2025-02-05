#!/bin/bash

export PATH=/opt/FriendlyARM/toolschain/4.4.3/bin:$PATH
export CC=arm-linux-gcc
export CFLAGS="-march=armv4t -mtune=arm920t"

make clean
make
