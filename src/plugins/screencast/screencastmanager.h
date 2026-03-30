/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "plugin.h"

#include "wayland/screencast_v2.h"

namespace KWin
{

class LogicalOutput;
class ScreenCastStream;
class PipeWireCore;

class ScreencastManager : public Plugin
{
    Q_OBJECT

public:
    explicit ScreencastManager();

private:
    void streamWindow(ScreencastStreamV2Interface *stream, const ScreencastWindowParamsV2 &params);
    void streamWaylandOutput(ScreencastStreamV2Interface *stream, const ScreencastOutputParamsV2 &params);
    void
    streamOutput(ScreencastStreamV2Interface *stream, LogicalOutput *output, ScreencastStreamV2Interface::CursorMode mode);
    void streamVirtualOutput(ScreencastStreamV2Interface *stream, const ScreencastVirtualOutputParamsV2 &params);
    void streamRegion(ScreencastStreamV2Interface *stream, const ScreencastRegionParamsV2 &params);

    void integrateStreams(ScreencastStreamV2Interface *waylandStream, ScreenCastStream *stream);

    std::shared_ptr<PipeWireCore> getPipewireConnection();

    ScreencastManagerV2Interface *m_screencast;
    std::shared_ptr<PipeWireCore> m_pipewireConnectionCache;
};

} // namespace KWin
