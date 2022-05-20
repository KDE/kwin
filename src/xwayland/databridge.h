/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_XWL_DATABRIDGE
#define KWIN_XWL_DATABRIDGE

#include "kwinglobals.h"

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
class DataBridge : public QObject
{
    Q_OBJECT

public:
    static void destroy();

    ~DataBridge() override;

    DragEventReply dragMoveFilter(Window *target, const QPoint &pos);

    Dnd *dnd() const
    {
        return m_dnd;
    }

private:
    void init();

    Clipboard *m_clipboard = nullptr;
    Dnd *m_dnd = nullptr;
    Primary *m_primary = nullptr;

    KWIN_SINGLETON(DataBridge)
};

} // namespace Xwl
} // namespace KWin

#endif
