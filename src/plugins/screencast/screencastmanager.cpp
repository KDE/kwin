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
    , m_core(new PipeWireCore)
{
    m_core->init();
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

    auto stream = new ScreenCastStream(new WindowScreenCastSource(window), m_core, this);
    stream->setObjectName(window->desktopFileName());
    stream->setCursorMode(mode, 1, window->clientGeometry());

    if (mode != ScreencastV1Interface::CursorMode::Hidden) {
        connect(window, &Window::clientGeometryChanged, stream, [window, stream, mode]() {
            stream->setCursorMode(mode, 1, window->clientGeometry().toRect());
        });
    }

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

    auto stream = new ScreenCastStream(new OutputScreenCastSource(streamOutput), m_core, this);
    stream->setObjectName(streamOutput->name());
    stream->setCursorMode(mode, streamOutput->scale(), streamOutput->geometry());

    connect(streamOutput, &Output::changed, stream, [streamOutput, stream, mode]() {
        stream->setCursorMode(mode, streamOutput->scale(), streamOutput->geometry());
    });

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
    auto stream = new ScreenCastStream(source, m_core, this);
    stream->setObjectName(rectToString(geometry));
    stream->setCursorMode(mode, scale, geometry);

    integrateStreams(waylandStream, stream);
}

void ScreencastManager::integrateStreams(ScreencastStreamV1Interface *waylandStream, ScreenCastStream *stream)
{
    connect(waylandStream, &ScreencastStreamV1Interface::finished, stream, &ScreenCastStream::close);
    connect(stream, &ScreenCastStream::closed, waylandStream, [stream, waylandStream] {
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

#include "moc_screencastmanager.cpp"
