/*
    SPDX-FileCopyrightText: 2022 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "surface_interface.h"

#include "qwayland-server-tearing-control-v1.h"

#include <QObject>

namespace KWaylandServer
{

class TearingControlManagerV1InterfacePrivate;
class TearingControlV1Interface;
class Display;

class TearingControlManagerV1Interface : public QObject
{
    Q_OBJECT
public:
    TearingControlManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~TearingControlManagerV1Interface() override;

private:
    std::unique_ptr<TearingControlManagerV1InterfacePrivate> d;
};
}
