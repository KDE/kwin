/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "screencastmanager.h"
#include "composite.h"
#include "deleted.h"
#include "effects.h"
#include "kwingltexture.h"
#include "output.h"
#include "outputscreencastsource.h"
#include "platform.h"
#include "regionscreencastsource.h"
#include "scene.h"
#include "screencaststream.h"
#include "wayland/display.h"
#include "wayland/output_interface.h"
#include "wayland_server.h"
#include "window.h"
#include "windowscreencastsource.h"
#include "workspace.h"

#include <KLocalizedString>

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
    connect(m_screencast, &KWaylandServer::ScreencastV1Interface::regionScreencastRequested, this, &ScreencastManager::streamRegion);
}

class WindowStream : public ScreenCastStream
{
public:
    WindowStream(Window *window, QObject *parent)
        : ScreenCastStream(new WindowScreenCastSource(window), parent)
        , m_window(window)
    {
        setObjectName(window->desktopFileName());
        connect(this, &ScreenCastStream::startStreaming, this, &WindowStream::startFeeding);
        connect(this, &ScreenCastStream::stopStreaming, this, &WindowStream::stopFeeding);
    }

private:
    void startFeeding()
    {
        connect(Compositor::self()->scene(), &Scene::frameRendered, this, &WindowStream::bufferToStream);

        connect(m_window, &Window::damaged, this, &WindowStream::includeDamage);
        m_damagedRegion = m_window->visibleGeometry();
        m_window->output()->renderLoop()->scheduleRepaint();
    }

    void stopFeeding()
    {
        disconnect(Compositor::self()->scene(), &Scene::frameRendered, this, &WindowStream::bufferToStream);
    }

    void includeDamage(Window *window, const QRegion &damage)
    {
        Q_ASSERT(m_window == window);
        m_damagedRegion |= damage;
    }

    void bufferToStream()
    {
        if (!m_damagedRegion.isEmpty()) {
            recordFrame(m_damagedRegion);
            m_damagedRegion = {};
        }
    }

    QRegion m_damagedRegion;
    Window *m_window;
};

void ScreencastManager::streamWindow(KWaylandServer::ScreencastStreamV1Interface *waylandStream, const QString &winid)
{
    auto window = Workspace::self()->findToplevel(QUuid(winid));
    if (!window) {
        waylandStream->sendFailed(i18n("Could not find window id %1", winid));
        return;
    }

    auto stream = new WindowStream(window, this);
    integrateStreams(waylandStream, stream);
}

void ScreencastManager::streamVirtualOutput(KWaylandServer::ScreencastStreamV1Interface *stream,
                                            const QString &name,
                                            const QSize &size,
                                            double scale,
                                            KWaylandServer::ScreencastV1Interface::CursorMode mode)
{
    auto output = kwinApp()->platform()->createVirtualOutput(name, size, scale);
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
                                     Output *streamOutput,
                                     KWaylandServer::ScreencastV1Interface::CursorMode mode)
{
    if (!streamOutput) {
        waylandStream->sendFailed(i18n("Could not find output"));
        return;
    }

    auto stream = new ScreenCastStream(new OutputScreenCastSource(streamOutput), this);
    stream->setObjectName(streamOutput->name());
    stream->setCursorMode(mode, streamOutput->scale(), streamOutput->geometry());
    auto bufferToStream = [stream](const QRegion &damagedRegion) {
        if (!damagedRegion.isEmpty()) {
            stream->recordFrame(damagedRegion);
        }
    };
    connect(stream, &ScreenCastStream::startStreaming, waylandStream, [streamOutput, stream, bufferToStream] {
        Compositor::self()->scene()->addRepaint(streamOutput->geometry());
        connect(streamOutput, &Output::outputChange, stream, bufferToStream);
    });
    integrateStreams(waylandStream, stream);
}

static QString rectToString(const QRect &rect)
{
    return QStringLiteral("%1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}

void ScreencastManager::streamRegion(KWaylandServer::ScreencastStreamV1Interface *waylandStream, const QRect &geometry, qreal scale, KWaylandServer::ScreencastV1Interface::CursorMode mode)
{
    if (!geometry.isValid()) {
        waylandStream->sendFailed(i18n("Invalid region"));
        return;
    }

    auto source = new RegionScreenCastSource(geometry, scale);
    auto stream = new ScreenCastStream(source, this);
    stream->setObjectName(rectToString(geometry));
    stream->setCursorMode(mode, scale, geometry);

    connect(stream, &ScreenCastStream::startStreaming, waylandStream, [geometry, stream, source] {
        Compositor::self()->scene()->addRepaint(geometry);

        const auto allOutputs = kwinApp()->platform()->enabledOutputs();
        for (auto output : allOutputs) {
            if (output->geometry().intersects(geometry)) {
                auto bufferToStream = [output, stream, source](const QRegion &damagedRegion) {
                    if (damagedRegion.isEmpty()) {
                        return;
                    }

                    const QRect streamRegion = source->region();
                    const QRegion region = output->pixelSize() != output->modeSize() ? output->geometry() : damagedRegion;
                    source->updateOutput(output);
                    stream->recordFrame(region.translated(-streamRegion.topLeft()).intersected(streamRegion));
                };
                connect(output, &Output::outputChange, stream, bufferToStream);
            }
        }
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
    connect(stream, &ScreenCastStream::streamReady, stream, [waylandStream](uint nodeid) {
        waylandStream->sendCreated(nodeid);
    });
    if (!stream->init()) {
        waylandStream->sendFailed(stream->error());
        delete stream;
    }
}

} // namespace KWin
