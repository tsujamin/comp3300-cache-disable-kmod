sudo rmmod mod
make -C /usr/src/linux-headers-`uname -r`/ SUBDIRS=`pwd` modules
sudo insmod mod.ko
