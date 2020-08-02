/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCREENS_VIRTUAL_H
#define KWIN_SCREENS_VIRTUAL_H
#include "outputscreens.h"
#include <QVector>

namespace KWin
{
class VirtualBackend;

class VirtualScreens : public OutputScreens
{
    Q_OBJECT
public:
    VirtualScreens(VirtualBackend *backend, QObject *parent = nullptr);
    ~VirtualScreens() override;
    void init() override;

private:
    void createOutputs();
    VirtualBackend *m_backend;
};

}

#endif

