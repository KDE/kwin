#!/bin/sh

# KWin - the KDE window manager
# This file is part of the KDE project.

# SPDX-FileCopyrightText: 2022 Natalie Clarius <natalie_clarius@yahoo.de>

# SPDX-License-Identifier: GPL-2.0-or-later

while read -r line; do
    if [ "$line" = "Placement=Cascaded" ]; then
        echo "Placement=ZeroCornered"
    else
        echo "$line"
    fi
done
