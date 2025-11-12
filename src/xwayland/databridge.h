/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "effect/globals.h"

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPoint>

namespace KWin
{
class SurfaceInterface;
class Window;

namespace Xwl
{
class Clipboard;
class Dnd;
class Primary;

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

    bool dragMoveFilter(Window *target, const QPointF &position);

    Dnd *dnd() const
    {
        return m_dnd;
    }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void init();

    Clipboard *m_clipboard = nullptr;
    Dnd *m_dnd = nullptr;
    Primary *m_primary = nullptr;
};

} // namespace Xwl
} // namespace KWin
