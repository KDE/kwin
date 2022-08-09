/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "plugin.h"

#include "wayland/screencast_v1_interface.h"

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
    void streamWindow(KWaylandServer::ScreencastStreamV1Interface *stream,
                      const QString &winid,
                      KWaylandServer::ScreencastV1Interface::CursorMode mode);
    void streamWaylandOutput(KWaylandServer::ScreencastStreamV1Interface *stream,
                             KWaylandServer::OutputInterface *output,
                             KWaylandServer::ScreencastV1Interface::CursorMode mode);
    void
    streamOutput(KWaylandServer::ScreencastStreamV1Interface *stream, Output *output, KWaylandServer::ScreencastV1Interface::CursorMode mode);
    void streamVirtualOutput(KWaylandServer::ScreencastStreamV1Interface *stream,
                             const QString &name,
                             const QSize &size,
                             double scale,
                             KWaylandServer::ScreencastV1Interface::CursorMode mode);
    void streamRegion(KWaylandServer::ScreencastStreamV1Interface *stream,
                      const QRect &geometry,
                      qreal scale,
                      KWaylandServer::ScreencastV1Interface::CursorMode mode);

    void integrateStreams(KWaylandServer::ScreencastStreamV1Interface *waylandStream, ScreenCastStream *stream);

    KWaylandServer::ScreencastV1Interface *m_screencast;
};

} // namespace KWin
