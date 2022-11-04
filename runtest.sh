#!/bin/bash
cd ~/kde/build/kwin
if make -j16 ; then
    echo "make successful"
else
    echo "make failed"
    cd ~/kde/src/kwin
    exit -1
fi
cd bin
if dbus-run-session ./$1 ; then
    echo "test successful"
else
    echo "test failed"
    cd ~/kde/src/kwin
    exit 1
fi
exit 0
