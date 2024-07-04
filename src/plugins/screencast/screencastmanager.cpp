/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "screencastmanager.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "outputscreencastsource.h"
#include "pipewirecore.h"
#include "regionscreencastsource.h"
#include "screencaststream.h"
#include "wayland/display.h"
#include "wayland/output.h"
#include "wayland_server.h"
#include "window.h"
#include "windowscreencastsource.h"
#include "workspace.h"

#include <KLocalizedString>

namespace KWin
{

ScreencastManager::ScreencastManager()
    : m_screencast(new ScreencastV1Interface(waylandServer()->display(), this))
{
    getPipewireConnection();

    connect(m_screencast, &ScreencastV1Interface::windowScreencastRequested, this, &ScreencastManager::streamWindow);
    connect(m_screencast, &ScreencastV1Interface::outputScreencastRequested, this, &ScreencastManager::streamWaylandOutput);
    connect(m_screencast, &ScreencastV1Interface::virtualOutputScreencastRequested, this, &ScreencastManager::streamVirtualOutput);
    connect(m_screencast, &ScreencastV1Interface::regionScreencastRequested, this, &ScreencastManager::streamRegion);
}

void ScreencastManager::streamWindow(ScreencastStreamV1Interface *waylandStream,
                                     const QString &winid,
                                     ScreencastV1Interface::CursorMode mode)
{
    auto window = Workspace::self()->findWindow(QUuid(winid));
    if (!window) {
        waylandStream->sendFailed(i18n("Could not find window id %1", winid));
        return;
    }

    auto stream = new ScreenCastStream(new WindowScreenCastSource(window), getPipewireConnection(), this);
    stream->setObjectName(window->desktopFileName());
    stream->setCursorMode(mode);

    integrateStreams(waylandStream, stream);
}

void ScreencastManager::streamVirtualOutput(ScreencastStreamV1Interface *stream,
                                            const QString &name,
                                            const QSize &size,
                                            double scale,
                                            ScreencastV1Interface::CursorMode mode)
{
    auto output = kwinApp()->outputBackend()->createVirtualOutput(name, size, scale);
    streamOutput(stream, output, mode);
    connect(stream, &ScreencastStreamV1Interface::finished, output, [output] {
        kwinApp()->outputBackend()->removeVirtualOutput(output);
    });
}

void ScreencastManager::streamWaylandOutput(ScreencastStreamV1Interface *waylandStream,
                                            OutputInterface *output,
                                            ScreencastV1Interface::CursorMode mode)
{
    streamOutput(waylandStream, output->handle(), mode);
}

void ScreencastManager::streamOutput(ScreencastStreamV1Interface *waylandStream,
                                     Output *streamOutput,
                                     ScreencastV1Interface::CursorMode mode)
{
    if (!streamOutput) {
        waylandStream->sendFailed(i18n("Could not find output"));
        return;
    }

    auto stream = new ScreenCastStream(new OutputScreenCastSource(streamOutput), getPipewireConnection(), this);
    stream->setObjectName(streamOutput->name());
    stream->setCursorMode(mode);

    integrateStreams(waylandStream, stream);
}

static QString rectToString(const QRect &rect)
{
    return QStringLiteral("%1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}

void ScreencastManager::streamRegion(ScreencastStreamV1Interface *waylandStream, const QRect &geometry, qreal scale, ScreencastV1Interface::CursorMode mode)
{
    if (!geometry.isValid()) {
        waylandStream->sendFailed(i18n("Invalid region"));
        return;
    }

    auto source = new RegionScreenCastSource(geometry, scale);
    auto stream = new ScreenCastStream(source, getPipewireConnection(), this);
    stream->setObjectName(rectToString(geometry));
    stream->setCursorMode(mode);

    integrateStreams(waylandStream, stream);
}

void ScreencastManager::integrateStreams(ScreencastStreamV1Interface *waylandStream, ScreenCastStream *stream)
{
    connect(waylandStream, &ScreencastStreamV1Interface::finished, stream, &ScreenCastStream::close);
    connect(stream, &ScreenCastStream::closed, waylandStream, [stream, waylandStream] {
        waylandStream->sendClosed();
        stream->deleteLater();
    });
    connect(stream, &ScreenCastStream::ready, stream, [waylandStream](uint nodeid) {
        waylandStream->sendCreated(nodeid);
    });
    if (!stream->init()) {
        waylandStream->sendFailed(stream->error());
        delete stream;
    }
}

std::shared_ptr<PipeWireCore> ScreencastManager::getPipewireConnection()
{
    if (m_pipewireConnectionCache && m_pipewireConnectionCache->isValid()) {
        return m_pipewireConnectionCache;
    } else {
        std::shared_ptr<PipeWireCore> pipeWireCore = std::make_shared<PipeWireCore>();
        if (pipeWireCore->init()) {
            m_pipewireConnectionCache = pipeWireCore;
        }
        // return a valid object even if init fails
        return pipeWireCore;
    }
}

} // namespace KWin

#include "moc_screencastmanager.cpp"
