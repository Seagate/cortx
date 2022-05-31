#!/bin/bash

#########################################################
##  4 Partitions of +8GB will be created for sdb disk  ##
#########################################################

echo "p
n
p
1

+8GB
n
p
2

+8GB
n
p
3

+8GB
n
p
4

+8GB

w
" | fdisk /dev/sdb

#########################################################
## 4 Partitions of +8GB will be created for sdc disk   ##
#########################################################

echo "p
n
p
1

+8GB
n
p
2

+8GB
n
p
3

+8GB
n
p
4

+8GB

w
" | fdisk /dev/sdc
