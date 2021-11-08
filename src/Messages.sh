#! /usr/bin/env bash
$EXTRACTRC *.kcfg *.ui >> rc.cpp
$XGETTEXT *.h *.cpp plugins/nightcolor/*.cpp helpers/killer/*.cpp scenes/opengl/*.cpp tabbox/*.cpp scripting/*.cpp plugins/krunner-integration/*.cpp -o $podir/kwin.pot
