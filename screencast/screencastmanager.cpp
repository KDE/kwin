/*
 * Copyright © 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Aleix Pol Gonzalez <aleixpol@kde.org>
 */

#include "screencastmanager.h"
#include "scene.h"
#include "workspace.h"
#include "composite.h"
#include "platform.h"
#include "abstract_wayland_output.h"
#include "plugins/scenes/opengl/scene_opengl.h"
#include "pipewirestream.h"

#include <KWaylandServer/output_interface.h>
#include <KLocalizedString>
#include <abstract_client.h>
#include <effects.h>
#include <deleted.h>

using namespace KWin;

ScreencastManager::ScreencastManager(QObject *parent)
    : QObject(parent)
{
    connect(waylandServer()->screencast(), &KWaylandServer::ScreencastInterface::windowScreencastRequested, this, &ScreencastManager::streamWindow);
    connect(waylandServer()->screencast(), &KWaylandServer::ScreencastInterface::outputScreencastRequested, this, &ScreencastManager::streamOutput);
}
class EGLFence : public QObject
{
public:
    EGLFence(EGLDisplay eglDisplay)
        : m_eglDisplay(eglDisplay)
        , m_sync(eglCreateSync(eglDisplay, EGL_SYNC_FENCE_KHR, NULL))
    {
        Q_ASSERT(m_sync);
        glFinish();
    }

    bool clientWaitSync()
    {
        glFenceSync (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        int ret = eglClientWaitSync(m_eglDisplay, m_sync, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 0);
        Q_ASSERT(ret == EGL_CONDITION_SATISFIED_KHR);
        return ret == EGL_CONDITION_SATISFIED_KHR;
    }

    ~EGLFence() {
        auto ret = eglDestroySyncKHR(m_eglDisplay, m_sync);
        Q_ASSERT(ret == EGL_TRUE);
    }

private:
    const EGLDisplay m_eglDisplay;
    const EGLSyncKHR m_sync;
};

class WindowStream : public PipeWireStream
{
public:
    WindowStream(KWin::Toplevel *toplevel, QObject *parent)
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
        auto scene = KWin::Compositor::self()->scene();
        connect(scene, &Scene::frameRendered, this, &WindowStream::bufferToStream);

        connect(m_toplevel, &Toplevel::damaged, this, &WindowStream::includeDamage);
        m_toplevel->damaged(m_toplevel, m_toplevel->frameGeometry());
    }

    void includeDamage(KWin::Toplevel *toplevel, const QRect &damage) {
        Q_ASSERT(m_toplevel == toplevel);
        m_damagedRegion |= damage;
    }

    void bufferToStream () {
        if (m_damagedRegion.isEmpty()) {
            return;
        }
        EGLFence fence(kwinApp()->platform()->sceneEglDisplay());
        QSharedPointer<GLTexture> frameTexture(m_toplevel->effectWindow()->sceneWindow()->windowTexture());
        const bool wasYInverted = frameTexture->isYInverted();
        frameTexture->setYInverted(false);

        recordFrame(frameTexture.data(), m_damagedRegion);
        frameTexture->setYInverted(wasYInverted);
        m_damagedRegion = {};
        bool b = fence.clientWaitSync();
        Q_ASSERT(b);
    }

    QRegion m_damagedRegion;
    KWin::Toplevel *m_toplevel;
};

void ScreencastManager::streamWindow(KWaylandServer::ScreencastStreamInterface *waylandStream, const QString &winid)
{
    auto *toplevel = KWin::Workspace::self()->findToplevel(winid);

    if (!toplevel) {
        waylandStream->sendFailed(i18n("Could not find window id %1", winid));
        return;
    }

    auto stream = new WindowStream(toplevel, this);
    integrateStreams(waylandStream, stream);
}

void ScreencastManager::streamOutput(KWaylandServer::ScreencastStreamInterface *waylandStream,
                                  ::wl_resource *outputResource,
                                  KWaylandServer::ScreencastInterface::CursorMode mode)
{
    auto outputIface = KWaylandServer::OutputInterface::get(outputResource);
    if (!outputIface) {
        waylandStream->sendFailed(i18n("Invalid output"));
        return;
    }

    const auto outputs = kwinApp()->platform()->enabledOutputs();
    AbstractWaylandOutput *streamOutput = nullptr;
    for (auto output : outputs) {
        if (static_cast<AbstractWaylandOutput *>(output)->waylandOutput() == outputIface) {
            streamOutput = static_cast<AbstractWaylandOutput *>(output);
        }
    }

    if (!streamOutput) {
        waylandStream->sendFailed(i18n("Could not find output"));
        return;
    }

    auto stream = new PipeWireStream(true, streamOutput->pixelSize(), this);
    stream->setObjectName(streamOutput->name());
    stream->setCursorMode(mode, streamOutput->scale(), streamOutput->geometry());
    connect(streamOutput, &QObject::destroyed, stream, &PipeWireStream::stopStreaming);
    auto bufferToStream = [streamOutput, stream] (const QRegion &damagedRegion) {
        auto scene = KWin::Compositor::self()->scene();
        auto texture = scene->textureForOutput(streamOutput);

        const QRect frame({}, streamOutput->modeSize());
        const QRegion region = damagedRegion.isEmpty() || streamOutput->pixelSize() != streamOutput->modeSize() ? frame : damagedRegion.translated(-streamOutput->geometry().topLeft()).intersected(frame);
        stream->recordFrame(texture.data(), region);
    };
    connect(stream, &PipeWireStream::startStreaming, waylandStream, [streamOutput, stream, bufferToStream] {
        KWin::Compositor::self()->addRepaint(streamOutput->geometry());
        connect(streamOutput, &AbstractWaylandOutput::outputChange, stream, bufferToStream);
    });
    integrateStreams(waylandStream, stream);
}

void ScreencastManager::integrateStreams(KWaylandServer::ScreencastStreamInterface *waylandStream, PipeWireStream *stream)
{
    connect(waylandStream, &KWaylandServer::ScreencastStreamInterface::finished, stream, &PipeWireStream::stop);
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
