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
class Display;
class XdgToplevelInterface;

class XdgToplevelIconManagerV1InterfacePrivate;

class KWIN_EXPORT XdgToplevelIconManagerV1Interface : public QObject
{
    Q_OBJECT
public:
    XdgToplevelIconManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~XdgToplevelIconManagerV1Interface() override;

private:
    std::unique_ptr<XdgToplevelIconManagerV1InterfacePrivate> d;
};
}
