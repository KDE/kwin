/*
    SPDX-FileCopyrightText: 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

namespace KWaylandServer
{
class Display;
class KeyStateInterfacePrivate;

/**
 * @brief Exposes key states to wayland clients
 */
class KWIN_EXPORT KeyStateInterface : public QObject
{
    Q_OBJECT

public:
    explicit KeyStateInterface(Display *display, QObject *parent = nullptr);
    ~KeyStateInterface() override;

private:
    std::unique_ptr<KeyStateInterfacePrivate> d;
};

}
