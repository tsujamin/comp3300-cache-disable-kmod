comp3300-cache-disable-kmod
===========================
small kernel module that allows toggling of x86/amd64 on die caches.

```bash
echo 0 > /proc/cpustate # disables cache
echo 1 > /proc/cpustate # enables cache
cat /proc/cpustate # prints cache status and cpu governor information
```

written for COMP3300 at the Australian National University

Installing
----------
just call `build.sh` and the module will be compiled and loaded. tested on linux 3.13.

you may need to edit build.sh depending on where your linux kerel src/headers are located
