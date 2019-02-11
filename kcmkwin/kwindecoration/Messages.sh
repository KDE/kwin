#!bin/sh
$EXTRACTRC `find . -name \*.rc -o -name \*.ui -o -name \*.kcfg` >> rc.cpp
$XGETTEXT `find . -name \*.qml -o -name \*.cpp -o -name \*.h` -o $podir/kcm_kwindecoration.pot
rm -f rc.cpp
