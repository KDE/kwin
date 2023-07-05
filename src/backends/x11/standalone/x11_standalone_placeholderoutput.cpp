/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "x11_standalone_placeholderoutput.h"
#include "utils/xcbutils.h"
#include "x11_standalone_backend.h"

namespace KWin
{

X11PlaceholderOutput::X11PlaceholderOutput(X11StandaloneBackend *backend, QObject *parent)
    : Output(parent)
    , m_backend(backend)
{
    QSize pixelSize;
    xcb_screen_t *screen = Xcb::defaultScreen();
    if (screen) {
        pixelSize = QSize(screen->width_in_pixels, screen->height_in_pixels);
    }

    auto mode = std::make_shared<OutputMode>(pixelSize, 60000);

    setInformation(Information{
        .name = QStringLiteral("Placeholder-0"),
    });

    setState(State{
        .modes = {mode},
        .currentMode = mode,
    });
}

RenderLoop *X11PlaceholderOutput::renderLoop() const
{
    return m_backend->renderLoop();
}

void X11PlaceholderOutput::updateEnabled(bool enabled)
{
    State next = m_state;
    next.enabled = enabled;
    setState(next);
}

} // namespace KWin

#include "moc_x11_standalone_placeholderoutput.cpp"
