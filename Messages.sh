#! /usr/bin/env bash
$EXTRACTRC *.rc *.ui *.kcfg > rc.cpp
$XGETTEXT *.h *.cpp killer/*.cpp lib/*.cpp -o $podir/kwin.pot
