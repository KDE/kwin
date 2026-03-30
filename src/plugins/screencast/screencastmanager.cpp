/*
    SPDX-FileCopyrightText: 2018-2020 Red Hat Inc
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileContributor: Jan Grulich <jgrulich@redhat.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "screencastmanager.h"
#include "compositor.h"
#include "core/backendoutput.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "core/renderbackend.h"
#include "outputscreencastsource.h"
#include "pipewirecore.h"
#include "regionscreencastsource.h"
#include "screencaststream.h"
#include "wayland/clientconnection.h"
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
    : m_screencast(new ScreencastManagerV2Interface(waylandServer()->display(), this))
{
    getPipewireConnection();

    connect(m_screencast, &ScreencastManagerV2Interface::windowScreencastRequested, this, &ScreencastManager::streamWindow);
    connect(m_screencast, &ScreencastManagerV2Interface::outputScreencastRequested, this, &ScreencastManager::streamWaylandOutput);
    connect(m_screencast, &ScreencastManagerV2Interface::virtualOutputScreencastRequested, this, &ScreencastManager::streamVirtualOutput);
    connect(m_screencast, &ScreencastManagerV2Interface::regionScreencastRequested, this, &ScreencastManager::streamRegion);
}

static bool isSupportedCompositingType()
{
    if (auto backend = Compositor::self()->backend()) {
        return backend->compositingType() == OpenGLCompositing;
    }
    return false;
}

void ScreencastManager::streamWindow(ScreencastStreamV2Interface *waylandStream, const ScreencastWindowParamsV2 &params)

{
    if (!isSupportedCompositingType()) {
        waylandStream->sendFailed(i18n("Unsupported compositing type"));
        return;
    }

    auto window = Workspace::self()->findWindow(QUuid(params.window()));
    if (!window) {
        waylandStream->sendFailed(i18n("Could not find window id %1", params.window()));
        return;
    }

    auto stream = new ScreenCastStream(new WindowScreenCastSource(window), getPipewireConnection(), this);
    stream->setObjectName(window->desktopFileName());
    stream->setCursorMode(params.cursorMode());

    integrateStreams(waylandStream, stream);
}

void ScreencastManager::streamVirtualOutput(ScreencastStreamV2Interface *stream, const ScreencastVirtualOutputParamsV2 &params)
{
    if (!isSupportedCompositingType()) {
        stream->sendFailed(i18n("Unsupported compositing type"));
        return;
    }

    auto output = kwinApp()->outputBackend()->createVirtualOutput(params.name(), params.description(), params.size(), params.scale());
    streamOutput(stream, workspace()->findOutput(output), params.cursorMode());
    connect(stream, &ScreencastStreamV2Interface::finished, output, [output] {
        kwinApp()->outputBackend()->removeVirtualOutput(output);
    });
}

void ScreencastManager::streamWaylandOutput(ScreencastStreamV2Interface *waylandStream, const ScreencastOutputParamsV2 &params)
{
    if (!isSupportedCompositingType()) {
        waylandStream->sendFailed(i18n("Unsupported compositing type"));
        return;
    }

    auto output = workspace()->findOutput(params.output());
    if (!output) {
        waylandStream->sendFailed(i18n("No such output %1", params.output()));
    }

    streamOutput(waylandStream, output, params.cursorMode());
}

void ScreencastManager::streamOutput(ScreencastStreamV2Interface *waylandStream,
                                     LogicalOutput *streamOutput,
                                     ScreencastStreamV2Interface::CursorMode mode)
{
    if (!streamOutput) {
        waylandStream->sendFailed(i18n("Could not find output"));
        return;
    }

    auto stream = new ScreenCastStream(new OutputScreenCastSource(streamOutput, waylandStream->connection()->processId()), getPipewireConnection(), this);
    stream->setObjectName(streamOutput->name());
    stream->setCursorMode(mode);

    integrateStreams(waylandStream, stream);
}

static QString rectToString(const Rect &rect)
{
    return QStringLiteral("%1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}

static qreal devicePixelRatioForRegion(const Rect &region)
{
    qreal devicePixelRatio = 1.0;

    const auto outputs = workspace()->outputs();
    for (const LogicalOutput *output : outputs) {
        if (output->geometry().intersects(region)) {
            devicePixelRatio = std::max(devicePixelRatio, output->scale());
        }
    }

    return devicePixelRatio;
}

void ScreencastManager::streamRegion(ScreencastStreamV2Interface *waylandStream, const ScreencastRegionParamsV2 &params)
{
    if (!isSupportedCompositingType()) {
        waylandStream->sendFailed(i18n("Unsupported compositing type"));
        return;
    }

    if (!params.region().isValid()) {
        waylandStream->sendFailed(i18n("Invalid region"));
        return;
    }

    double scale = params.scale();
    if (scale == 0) {
        scale = devicePixelRatioForRegion(params.region());
    }

    auto source = new RegionScreenCastSource(params.region(), scale, waylandStream->connection()->processId());
    auto stream = new ScreenCastStream(source, getPipewireConnection(), this);
    stream->setObjectName(rectToString(params.region()));
    stream->setCursorMode(params.cursorMode());

    integrateStreams(waylandStream, stream);
}

void ScreencastManager::integrateStreams(ScreencastStreamV2Interface *waylandStream, ScreenCastStream *stream)
{
    connect(waylandStream, &ScreencastStreamV2Interface::finished, stream, &ScreenCastStream::close);
    connect(stream, &ScreenCastStream::closed, waylandStream, [stream, waylandStream] {
        waylandStream->sendClosed();
        stream->deleteLater();
    });
    connect(stream, &ScreenCastStream::ready, waylandStream, [waylandStream](uint nodeid, quint64 objectSerial) {
        waylandStream->sendCreated(nodeid, objectSerial);
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
