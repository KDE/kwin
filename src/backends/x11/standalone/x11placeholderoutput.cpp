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

    const QByteArray model = QByteArrayLiteral("kwin");
    const QByteArray manufacturer = QByteArrayLiteral("xorg");
    const QByteArray eisaId;
    const QByteArray serial;

    initialize(model, manufacturer, eisaId, serial, pixelSize, QByteArray());
    setName(QStringLiteral("Placeholder-0"));

    auto mode = QSharedPointer<OutputMode>::create(pixelSize, 60000);
    setModesInternal({mode}, mode);
}

RenderLoop *X11PlaceholderOutput::renderLoop() const
{
    return m_loop;
}

} // namespace KWin
