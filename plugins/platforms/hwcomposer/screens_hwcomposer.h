/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-3.0-or-later
*/
#ifndef KWIN_SCREENS_HWCOMPOSER_H
#define KWIN_SCREENS_HWCOMPOSER_H
#include "outputscreens.h"

namespace KWin
{
class HwcomposerBackend;

class HwcomposerScreens : public OutputScreens
{
    Q_OBJECT
public:
    HwcomposerScreens(HwcomposerBackend *backend, QObject *parent = nullptr);
    virtual ~HwcomposerScreens() = default;

private:
    HwcomposerBackend *m_backend;
};

}

#endif
