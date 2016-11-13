#!/bin/bash

sudo rm -rf /dev/pimbt
sudo rmmod ./test-driver.ko
make clean
make
gcc -o user_code user_code.c 
sudo mknod /dev/pimbt c 70 0
sudo insmod ./test-driver.ko
sudo chmod 666 /dev/pimbt


