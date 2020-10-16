/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_XWL_XWAYLAND_INTERFACE
#define KWIN_XWL_XWAYLAND_INTERFACE

#include <kwinglobals.h>

#include <QObject>
#include <QPoint>

class QProcess;

namespace KWin
{
class Toplevel;

namespace Xwl
{
enum class DragEventReply {
    // event should be ignored by the filter
    Ignore,
    // event is filtered out
    Take,
    // event should be handled as a Wayland native one
    Wayland,
};
} // namespace Xwl

class KWIN_EXPORT XwaylandInterface : public QObject
{
    Q_OBJECT

public:
    static XwaylandInterface *self();

    virtual Xwl::DragEventReply dragMoveFilter(Toplevel *target, const QPoint &pos) = 0;
    virtual QProcess *process() const = 0;

protected:
    explicit XwaylandInterface(QObject *parent = nullptr);
    ~XwaylandInterface() override;

private:
    Q_DISABLE_COPY(XwaylandInterface)
};

inline XwaylandInterface *xwayland()
{
    return XwaylandInterface::self();
}

} // namespace KWin

#endif
