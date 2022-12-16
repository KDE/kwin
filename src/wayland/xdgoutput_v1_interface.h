/*
    SPDX-FileCopyrightText: 2018 David Edmundson <kde@davidedmundson.co.uk>
    SPDX-FileCopyrightText: 2020 David Edmundson <kde@davidedmundson.co.uk>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

namespace KWaylandServer
{
class Display;
class OutputInterface;
class XdgOutputManagerV1InterfacePrivate;

/**
 * Global manager for XdgOutputs
 */
class KWIN_EXPORT XdgOutputManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit XdgOutputManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~XdgOutputManagerV1Interface() override;

    void offer(OutputInterface *output);

private:
    std::unique_ptr<XdgOutputManagerV1InterfacePrivate> d;
};

}
