#!/bin/bash
git pull
make clean
make
make install
/etc/init.d/sprinklers_pi restart
