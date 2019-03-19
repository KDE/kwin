#! /usr/bin/env bash
$EXTRACTRC `find . -name \*.ui` >> rc.cpp || exit 11
$XGETTEXT *.h *.cpp -o $podir/kcmkwincompositing.pot
