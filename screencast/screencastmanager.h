/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <KWaylandServer/screencast_v1_interface.h>

namespace KWin
{

class PipeWireStream;

class ScreencastManager : public QObject
{
    Q_OBJECT

public:
    explicit ScreencastManager(QObject *parent = nullptr);

    void streamWindow(KWaylandServer::ScreencastStreamV1Interface *stream, const QString &winid);
    void streamOutput(KWaylandServer::ScreencastStreamV1Interface *stream,
                      KWaylandServer::OutputInterface *output,
                      KWaylandServer::ScreencastV1Interface::CursorMode mode);

private:
    void integrateStreams(KWaylandServer::ScreencastStreamV1Interface *waylandStream, PipeWireStream *pipewireStream);

    KWaylandServer::ScreencastV1Interface *m_screencast;
};

} // namespace KWin
