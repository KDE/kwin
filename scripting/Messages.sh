#! /usr/bin/env bash
$EXTRACTRC `find . -name \*.rc -o -name \*.ui -o -name \*.kcfg` >> rc.cpp
$XGETTEXT `find . -name \*.h -o -name \*.cpp` -o $podir/kwin_scripting.pot
rm -f rc.cpp
