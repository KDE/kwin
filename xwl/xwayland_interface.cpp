/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "xwayland_interface.h"

namespace KWin
{

XwaylandInterface *s_self = nullptr;

XwaylandInterface *XwaylandInterface::self()
{
    return s_self;
}

XwaylandInterface::XwaylandInterface(QObject *parent)
    : QObject(parent)
{
    s_self = this;
}

XwaylandInterface::~XwaylandInterface()
{
    s_self = nullptr;
}

} // namespace KWin
