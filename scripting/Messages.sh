#! /usr/bin/env bash
$XGETTEXT `find . -name \*.h -o -name \*.cpp` -o $podir/kwin_scripting.pot
