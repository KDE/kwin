#! /usr/bin/env bash
$EXTRACTRC `find . -name \*.ui` >> rc.cpp || exit 11
$XGETTEXT `find . -name \*.cpp -o -name \*.h -o -name \*.qml` -o $podir/kwin_effects.pot
rm -f rc.cpp
