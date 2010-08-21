#! /usr/bin/env bash
$EXTRACTRC *.kcfg >> rc.cpp
$XGETTEXT *.h *.cpp effects/*.cpp killer/*.cpp lib/*.cpp tabbox/*.cpp -o $podir/kwin.pot
