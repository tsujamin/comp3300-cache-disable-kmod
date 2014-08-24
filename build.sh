sudo rmmod mod
make -C /usr/src/kernels/`uname -r`/ SUBDIRS=`pwd` modules
sudo insmod mod.ko
