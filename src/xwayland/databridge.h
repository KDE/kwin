/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwinglobals.h"

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPoint>

namespace KWaylandServer
{
class DataDeviceInterface;
class SurfaceInterface;
}

namespace KWin
{
class Window;

namespace Xwl
{
class Clipboard;
class Dnd;
class Primary;
enum class DragEventReply;

/**
 * Interface class for all data sharing in the context of X selections
 * and Wayland's internal mechanism.
 *
 * Exists only once per Xwayland session.
 */
class DataBridge : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit DataBridge();

    DragEventReply dragMoveFilter(Window *target, const QPoint &pos);

    Dnd *dnd() const
    {
        return m_dnd;
    }

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray &eventType, void *message, long int *result) override;
#else
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private:
    void init();

    Clipboard *m_clipboard = nullptr;
    Dnd *m_dnd = nullptr;
    Primary *m_primary = nullptr;
};

} // namespace Xwl
} // namespace KWin
