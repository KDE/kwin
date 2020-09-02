/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <KWaylandServer/screencast_interface.h>

namespace KWin
{

class PipeWireStream;

class ScreencastManager : public QObject
{
    Q_OBJECT

public:
    explicit ScreencastManager(QObject *parent = nullptr);

    void streamWindow(KWaylandServer::ScreencastStreamInterface *stream, const QString &winid);
    void streamOutput(KWaylandServer::ScreencastStreamInterface *stream,
                      KWaylandServer::OutputInterface *output,
                      KWaylandServer::ScreencastInterface::CursorMode mode);

private:
    void integrateStreams(KWaylandServer::ScreencastStreamInterface *waylandStream, PipeWireStream *pipewireStream);

    KWaylandServer::ScreencastInterface *m_screencast;
};

} // namespace KWin
