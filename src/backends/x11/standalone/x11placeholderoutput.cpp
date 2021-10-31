/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "x11placeholderoutput.h"
#include "main.h"

namespace KWin
{

X11PlaceholderOutput::X11PlaceholderOutput(QObject *parent)
    : AbstractOutput(parent)
{
}

QString X11PlaceholderOutput::name() const
{
    return QStringLiteral("Placeholder-0");
}

QRect X11PlaceholderOutput::geometry() const
{
    xcb_screen_t *screen = kwinApp()->x11DefaultScreen();
    if (screen) {
        return QRect(0, 0, screen->width_in_pixels, screen->height_in_pixels);
    }
    return QRect();
}

int X11PlaceholderOutput::refreshRate() const
{
    return 60000;
}

QSize X11PlaceholderOutput::pixelSize() const
{
    return geometry().size();
}

} // namespace KWin
