/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <KWaylandServer/kwaylandserver_export.h>

#include <QObject>

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
class KWAYLANDSERVER_EXPORT RelativePointerManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit RelativePointerManagerV1Interface(Display *display, QObject *parent = nullptr);
    ~RelativePointerManagerV1Interface() override;

private:
    QScopedPointer<RelativePointerManagerV1InterfacePrivate> d;
};

} // namespace KWaylandServer
