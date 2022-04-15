/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "x11placeholderoutput.h"
#include "main.h"

namespace KWin
{

X11PlaceholderOutput::X11PlaceholderOutput(RenderLoop *loop, QObject *parent)
    : Output(parent)
    , m_loop(loop)
{
    QSize pixelSize;
    xcb_screen_t *screen = kwinApp()->x11DefaultScreen();
    if (screen) {
        pixelSize = QSize(screen->width_in_pixels, screen->height_in_pixels);
    }

    auto mode = QSharedPointer<OutputMode>::create(pixelSize, 60000);
    setModesInternal({mode}, mode);

    setInformation(Information{
        .name = QStringLiteral("Placeholder-0"),
    });
}

RenderLoop *X11PlaceholderOutput::renderLoop() const
{
    return m_loop;
}

} // namespace KWin
