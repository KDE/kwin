/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "effect/globals.h"

#include <QObject>

class QProcess;

namespace KWin
{
class AbstractDropHandler;
class Window;

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

class KWIN_EXPORT XwaylandInterface
{
public:
    virtual Xwl::DragEventReply dragMoveFilter(Window *target) = 0;
    virtual AbstractDropHandler *xwlDropHandler() = 0;

protected:
    explicit XwaylandInterface() = default;
    virtual ~XwaylandInterface() = default;

private:
    Q_DISABLE_COPY(XwaylandInterface)
};

} // namespace KWin
