/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef MOCK_UDEV_H
#define MOCK_UDEV_H

struct udev {
    static udev *s_mockUdev;
};

#endif

