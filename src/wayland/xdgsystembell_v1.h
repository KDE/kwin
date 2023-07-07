/*
    SPDX-FileCopyrightText: 2023 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>

#include <memory>

namespace KWin
{
class ClientConnection;
class Display;
class SurfaceInterface;
class XdgSystemBellV1InterfacePrivate;

class KWIN_EXPORT XdgSystemBellV1Interface : public QObject
{
    Q_OBJECT
public:
    XdgSystemBellV1Interface(Display *display, QObject *parent = nullptr);
    ~XdgSystemBellV1Interface();

    void remove();
Q_SIGNALS:
    void ring(ClientConnection *client);
    void ringSurface(SurfaceInterface *surface);

private:
    std::unique_ptr<XdgSystemBellV1InterfacePrivate> d;
};
}
