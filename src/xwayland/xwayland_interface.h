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

class KWIN_EXPORT XwaylandInterface
{
public:
    virtual bool dragMoveFilter(Window *target, const QPointF &position) = 0;
    virtual AbstractDropHandler *xwlDropHandler() = 0;

protected:
    explicit XwaylandInterface() = default;
    virtual ~XwaylandInterface() = default;

private:
    Q_DISABLE_COPY(XwaylandInterface)
};

} // namespace KWin
