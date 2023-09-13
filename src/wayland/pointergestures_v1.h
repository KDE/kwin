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
class PointerGesturesV1InterfacePrivate;

/**
 * Manager object for the PointerGestures.
 *
 * Creates and manages pointer swipe and pointer pinch gestures which are
 * reported to the SeatInterface.
 */
class KWIN_EXPORT PointerGesturesV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit PointerGesturesV1Interface(Display *display, QObject *parent = nullptr);
    ~PointerGesturesV1Interface() override;

private:
    std::unique_ptr<PointerGesturesV1InterfacePrivate> d;
};

} // namespace KWaylandServer
