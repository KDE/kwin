/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "plugin.h"

#include "wayland/screencast_v1.h"

namespace KWin
{
class Output;
class ScreenCastStream;

class ScreencastManager : public Plugin
{
    Q_OBJECT

public:
    explicit ScreencastManager();

private:
    void streamWindow(ScreencastStreamV1Interface *stream,
                      const QString &winid,
                      ScreencastV1Interface::CursorMode mode);
    void streamWaylandOutput(ScreencastStreamV1Interface *stream,
                             OutputInterface *output,
                             ScreencastV1Interface::CursorMode mode);
    void
    streamOutput(ScreencastStreamV1Interface *stream, Output *output, ScreencastV1Interface::CursorMode mode);
    void streamVirtualOutput(ScreencastStreamV1Interface *stream,
                             const QString &name,
                             const QSize &size,
                             double scale,
                             ScreencastV1Interface::CursorMode mode);
    void streamRegion(ScreencastStreamV1Interface *stream,
                      const QRect &geometry,
                      qreal scale,
                      ScreencastV1Interface::CursorMode mode);

    void integrateStreams(ScreencastStreamV1Interface *waylandStream, ScreenCastStream *stream);

    ScreencastV1Interface *m_screencast;
};

} // namespace KWin
