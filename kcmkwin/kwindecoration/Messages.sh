#! /usr/bin/env bash
$EXTRACTRC `find . -name \*.ui` >> rc.cpp || exit 11
$XGETTEXT *.cpp -o $podir/kcmkwindecoration.pot
rm -f rc.cpp
