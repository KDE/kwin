#! /usr/bin/env bash
$EXTRACTRC `find . -not -path "./kcms/*" \( -name \*.kcfg -o -name \*.ui \)` >> rc.cpp || exit 11
$XGETTEXT `find . -not -path "./kcms/*" \( -name \*.cpp -o -name \*.qml \)` -o $podir/kwin.pot
rm -f rc.cpp
