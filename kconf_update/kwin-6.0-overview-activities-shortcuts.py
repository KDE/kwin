#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Niccolò Venerandi <niccolo.venerandi@kde.org>
# SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

import fileinput

for line in fileinput.input():
    if line.startswith('next activity'):
        print(line.replace('Meta+Tab', 'Meta+A'))
    elif line.startswith('previous activity'):
        print(line.replace('Meta+Shift+Tab', 'Meta+Shift+A'))
    elif line.startswith('ShowDesktopGrid'):
        pass
    elif line.startswith('Overview'):
        print('Overview=Meta+W,Meta+W,Toggle Overview')
        print('Cycle Overview=Meta+Tab,Meta+Tab,Cycle through Overview and Grid View')
        print('Cycle Overview Opposite=Meta+Shift+Tab,Meta+Shift+Tab,Cycle through Grid View and Overview')
        print('Grid View=Meta+G,Meta+G,Toggle Grid View')
    else:
        print(line)
