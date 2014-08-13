#!/bin/bash
ctags `find . -name "*.[ch]"` `find /usr/src/kernels/$(uname -r)/ -name "*.[ch]"`

