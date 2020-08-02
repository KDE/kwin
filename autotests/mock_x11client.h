/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_MOCK_CLIENT_H
#define KWIN_MOCK_CLIENT_H

#include <abstract_client.h>

#include <QObject>
#include <QRect>

namespace KWin
{

class X11Client : public AbstractClient
{
    Q_OBJECT
public:
    explicit X11Client(QObject *parent);
    ~X11Client() override;
    void showOnScreenEdge() override;

};

}

#endif
