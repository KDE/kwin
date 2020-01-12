#! /usr/bin/env bash
$EXTRACTRC *.kcfg *.ui >> rc.cpp
$XGETTEXT *.h *.cpp colorcorrection/*.cpp helpers/killer/*.cpp plugins/scenes/opengl/*.cpp tabbox/*.cpp scripting/*.cpp -o $podir/kwin.pot
