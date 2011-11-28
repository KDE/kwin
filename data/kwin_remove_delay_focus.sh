#!/bin/sh
if ! `kreadconfig --file kwinrc --group Windows --key DelayFocus --default false` && [ `kreadconfig --file kwinrc --group Windows --key DelayFocusInterval --default 0` != 0 ]; then
    kwriteconfig --file kwinrc --group Windows --key DelayFocusInterval 0
fi