/*
    SPDX-FileCopyrightText: 2023 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QObject>

#include "display.h"
#include <memory>

#include <QHash>
#include <QIcon>

struct wl_resource;

namespace KWin
{
class XdgToplevelInterface;

class XdgToplevelIconV1InterfacePrivate;

class XdgToplevelIconV1Interface : public QObject
{
    Q_OBJECT
public:
    XdgToplevelIconV1Interface(Display *display, QObject *parent = nullptr);
    ~XdgToplevelIconV1Interface() override;

private:
    std::unique_ptr<XdgToplevelIconV1InterfacePrivate> d;
};
}
