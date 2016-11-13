#!/bin/bash

sudo mknod /dev/pimbt c 70 0
sudo insmod ./test-driver.ko
sudo chmod 666 /dev/pimbt

