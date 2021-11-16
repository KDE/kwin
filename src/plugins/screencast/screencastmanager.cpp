/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "screencastmanager.h"
#include "abstract_client.h"
#include "abstract_wayland_output.h"
#include "composite.h"
#include "deleted.h"
#include "effects.h"
#include "kwingltexture.h"
#include "outputscreencastsource.h"
#include "screencaststream.h"
#include "platform.h"
#include "scene.h"
#include "wayland_server.h"
#include "windowscreencastsource.h"
#include "workspace.h"

#include <KLocalizedString>

#include <KWaylandServer/display.h>
#include <KWaylandServer/output_interface.h>

namespace KWin
{

ScreencastManager::ScreencastManager(QObject *parent)
    : Plugin(parent)
    , m_screencast(new KWaylandServer::ScreencastV1Interface(waylandServer()->display(), this))
{
    connect(m_screencast, &KWaylandServer::ScreencastV1Interface::windowScreencastRequested,
            this, &ScreencastManager::streamWindow);
    connect(m_screencast, &KWaylandServer::ScreencastV1Interface::outputScreencastRequested, this, &ScreencastManager::streamWaylandOutput);
    connect(m_screencast, &KWaylandServer::ScreencastV1Interface::virtualOutputScreencastRequested, this, &ScreencastManager::streamVirtualOutput);
}

class WindowStream : public ScreenCastStream
{
public:
    WindowStream(Toplevel *toplevel, QObject *parent)
        : ScreenCastStream(new WindowScreenCastSource(toplevel), parent)
        , m_toplevel(toplevel)
    {
        if (AbstractClient *client = qobject_cast<AbstractClient *>(toplevel)) {
            setObjectName(client->desktopFileName());
        }
        connect(this, &ScreenCastStream::startStreaming, this, &WindowStream::startFeeding);
        connect(this, &ScreenCastStream::stopStreaming, this, &WindowStream::stopFeeding);
    }

private:
    void startFeeding() {
        connect(Compositor::self()->scene(), &Scene::frameRendered, this, &WindowStream::bufferToStream);

        connect(m_toplevel, &Toplevel::damaged, this, &WindowStream::includeDamage);
        m_damagedRegion = m_toplevel->visibleGeometry();
        m_toplevel->addRepaintFull();
    }

    void stopFeeding() {
        disconnect(Compositor::self()->scene(), &Scene::frameRendered, this, &WindowStream::bufferToStream);
    }

    void includeDamage(Toplevel *toplevel, const QRegion &damage) {
        Q_ASSERT(m_toplevel == toplevel);
        m_damagedRegion |= damage;
    }

    void bufferToStream () {
        if (!m_damagedRegion.isEmpty()) {
            recordFrame(m_damagedRegion);
            m_damagedRegion = {};
        }
    }

    QRegion m_damagedRegion;
    Toplevel *m_toplevel;
};

void ScreencastManager::streamWindow(KWaylandServer::ScreencastStreamV1Interface *waylandStream, const QString &winid)
{
    auto *toplevel = Workspace::self()->findToplevel(winid);

    if (!toplevel) {
        waylandStream->sendFailed(i18n("Could not find window id %1", winid));
        return;
    }

    auto stream = new WindowStream(toplevel, this);
    integrateStreams(waylandStream, stream);
}

void ScreencastManager::streamVirtualOutput(KWaylandServer::ScreencastStreamV1Interface *stream,
                                            const QString &name,
                                            const QSize &size,
                                            double scale,
                                            KWaylandServer::ScreencastV1Interface::CursorMode mode)
{
    auto output = qobject_cast<AbstractWaylandOutput *>(kwinApp()->platform()->createVirtualOutput(name, size, scale));
    streamOutput(stream, output, mode);
    connect(stream, &KWaylandServer::ScreencastStreamV1Interface::finished, output, [output] {
        kwinApp()->platform()->removeVirtualOutput(output);
    });
}

void ScreencastManager::streamWaylandOutput(KWaylandServer::ScreencastStreamV1Interface *waylandStream,
                                            KWaylandServer::OutputInterface *output,
                                            KWaylandServer::ScreencastV1Interface::CursorMode mode)
{
    streamOutput(waylandStream, waylandServer()->findOutput(output), mode);
}

void ScreencastManager::streamOutput(KWaylandServer::ScreencastStreamV1Interface *waylandStream,
                                     AbstractWaylandOutput *streamOutput,
                                     KWaylandServer::ScreencastV1Interface::CursorMode mode)
{
    if (!streamOutput) {
        waylandStream->sendFailed(i18n("Could not find output"));
        return;
    }

    auto stream = new ScreenCastStream(new OutputScreenCastSource(streamOutput), this);
    stream->setObjectName(streamOutput->name());
    stream->setCursorMode(mode, streamOutput->scale(), streamOutput->geometry());
    auto bufferToStream = [streamOutput, stream] (const QRegion &damagedRegion) {
        if (damagedRegion.isEmpty()) {
            return;
        }

        const QRect frame({}, streamOutput->modeSize());
        const QRegion region = streamOutput->pixelSize() != streamOutput->modeSize() ? frame : damagedRegion.translated(-streamOutput->geometry().topLeft()).intersected(frame);
        stream->recordFrame(region);
    };
    connect(stream, &ScreenCastStream::startStreaming, waylandStream, [streamOutput, stream, bufferToStream] {
        Compositor::self()->scene()->addRepaint(streamOutput->geometry());
        streamOutput->recordingStarted();
        connect(streamOutput, &AbstractWaylandOutput::outputChange, stream, bufferToStream);
    });
    connect(stream, &ScreenCastStream::stopStreaming, waylandStream, [streamOutput]{
        streamOutput->recordingStopped();
    });
    integrateStreams(waylandStream, stream);
}

void ScreencastManager::integrateStreams(KWaylandServer::ScreencastStreamV1Interface *waylandStream, ScreenCastStream *stream)
{
    connect(waylandStream, &KWaylandServer::ScreencastStreamV1Interface::finished, stream, &ScreenCastStream::stop);
    connect(stream, &ScreenCastStream::stopStreaming, waylandStream, [stream, waylandStream] {
        waylandStream->sendClosed();
        stream->deleteLater();
    });
    connect(stream, &ScreenCastStream::streamReady, stream, [waylandStream] (uint nodeid) {
        waylandStream->sendCreated(nodeid);
    });
    if (!stream->init()) {
        waylandStream->sendFailed(stream->error());
        delete stream;
    }
}

} // namespace KWin
