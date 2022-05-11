#!/bin/bash
for d in "kern boot inc lib user conf"
do
    cd ~/lab/$d
    rm *.orig *~ *.pyc
done
