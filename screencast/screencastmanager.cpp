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
#include "pipewirestream.h"
#include "platform.h"
#include "scene.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KLocalizedString>

#include <KWaylandServer/display.h>
#include <KWaylandServer/output_interface.h>

namespace KWin
{

ScreencastManager::ScreencastManager(QObject *parent)
    : QObject(parent)
    , m_screencast(waylandServer()->display()->createScreencastV1Interface(this))
{
    connect(m_screencast, &KWaylandServer::ScreencastV1Interface::windowScreencastRequested,
            this, &ScreencastManager::streamWindow);
    connect(m_screencast, &KWaylandServer::ScreencastV1Interface::outputScreencastRequested,
            this, &ScreencastManager::streamOutput);
}

class WindowStream : public PipeWireStream
{
public:
    WindowStream(Toplevel *toplevel, QObject *parent)
        : PipeWireStream(toplevel->hasAlpha(), toplevel->clientSize() * toplevel->bufferScale(), parent)
        , m_toplevel(toplevel)
    {
        if (AbstractClient *client = qobject_cast<AbstractClient *>(toplevel)) {
            setObjectName(client->desktopFileName());
        }
        connect(toplevel, &Toplevel::windowClosed, this, &PipeWireStream::stopStreaming);
        connect(this, &PipeWireStream::startStreaming, this, &WindowStream::startFeeding);
    }

private:
    void startFeeding() {
        auto scene = Compositor::self()->scene();
        connect(scene, &Scene::frameRendered, this, &WindowStream::bufferToStream);

        connect(m_toplevel, &Toplevel::damaged, this, &WindowStream::includeDamage);
        m_damagedRegion = m_toplevel->visibleRect();
        m_toplevel->addRepaintFull();
    }

    void includeDamage(Toplevel *toplevel, const QRect &damage) {
        Q_ASSERT(m_toplevel == toplevel);
        m_damagedRegion |= damage;
    }

    void bufferToStream () {
        if (m_damagedRegion.isEmpty()) {
            return;
        }
        effects->makeOpenGLContextCurrent();
        QSharedPointer<GLTexture> frameTexture(m_toplevel->effectWindow()->sceneWindow()->windowTexture());
        const bool wasYInverted = frameTexture->isYInverted();
        frameTexture->setYInverted(false);

        recordFrame(frameTexture.data(), m_damagedRegion);
        frameTexture->setYInverted(wasYInverted);
        m_damagedRegion = {};
        glFinish(); // TODO: Don't stall the whole pipeline. Use EGL_ANDROID_native_fence_sync.
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

void ScreencastManager::streamOutput(KWaylandServer::ScreencastStreamV1Interface *waylandStream,
                                     KWaylandServer::OutputInterface *output,
                                     KWaylandServer::ScreencastV1Interface::CursorMode mode)
{
    AbstractWaylandOutput *streamOutput = waylandServer()->findOutput(output);

    if (!streamOutput) {
        waylandStream->sendFailed(i18n("Could not find output"));
        return;
    }

    auto stream = new PipeWireStream(true, streamOutput->pixelSize(), this);
    stream->setObjectName(streamOutput->name());
    stream->setCursorMode(mode, streamOutput->scale(), streamOutput->geometry());
    connect(streamOutput, &QObject::destroyed, stream, &PipeWireStream::stopStreaming);
    auto bufferToStream = [streamOutput, stream] (const QRegion &damagedRegion) {
        auto scene = Compositor::self()->scene();
        auto texture = scene->textureForOutput(streamOutput);

        const QRect frame({}, streamOutput->modeSize());
        const QRegion region = damagedRegion.isEmpty() || streamOutput->pixelSize() != streamOutput->modeSize() ? frame : damagedRegion.translated(-streamOutput->geometry().topLeft()).intersected(frame);
        stream->recordFrame(texture.data(), region);
    };
    connect(stream, &PipeWireStream::startStreaming, waylandStream, [streamOutput, stream, bufferToStream] {
        Compositor::self()->addRepaint(streamOutput->geometry());
        connect(streamOutput, &AbstractWaylandOutput::outputChange, stream, bufferToStream);
    });
    integrateStreams(waylandStream, stream);
}

void ScreencastManager::integrateStreams(KWaylandServer::ScreencastStreamV1Interface *waylandStream, PipeWireStream *stream)
{
    connect(waylandStream, &KWaylandServer::ScreencastStreamV1Interface::finished, stream, &PipeWireStream::stop);
    connect(stream, &PipeWireStream::stopStreaming, waylandStream, [stream, waylandStream] {
        waylandStream->sendClosed();
        delete stream;
    });
    connect(stream, &PipeWireStream::streamReady, stream, [waylandStream] (uint nodeid) {
        waylandStream->sendCreated(nodeid);
    });
    if (!stream->init()) {
        waylandStream->sendFailed(stream->error());
        delete stream;
    }
}

} // namespace KWin
