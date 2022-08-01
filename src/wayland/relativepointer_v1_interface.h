/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

namespace KWaylandServer
{
class Display;
class RelativePointerManagerV1InterfacePrivate;

/**
 * Manager object to create relative pointer interfaces.
 *
 * Once created the interaction happens through the SeatInterface class
 * which automatically delegates relative motion events to the created relative pointer
 * interfaces.
 */
class KWIN_EXPORT RelativePointerManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit RelativePointerManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~RelativePointerManagerV1Interface() override;

private:
    std::unique_ptr<RelativePointerManagerV1InterfacePrivate> d;
};

} // namespace KWaylandServer
