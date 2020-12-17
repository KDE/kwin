/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-3.0-or-later
*/
#ifndef KWIN_SCREENS_HWCOMPOSER_H
#define KWIN_SCREENS_HWCOMPOSER_H
#include "screens.h"

namespace KWin
{
class HwcomposerBackend;

class HwcomposerScreens : public Screens
{
    Q_OBJECT
public:
    HwcomposerScreens(HwcomposerBackend *backend, QObject *parent = nullptr);
};

}

#endif
