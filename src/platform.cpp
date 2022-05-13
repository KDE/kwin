/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "platform.h"

#include <config-kwin.h>

#include "composite.h"
#include "cursor.h"
#include "effects.h"
#include "outline.h"
#include "output.h"
#include "outputconfiguration.h"
#include "overlaywindow.h"
#include "pointer_input.h"
#include "scene.h"
#include "screenedge.h"
#include "screens.h"
#include "wayland/outputchangeset_v2.h"
#include "wayland/outputconfiguration_v2_interface.h"
#include "wayland_server.h"

#include <KCoreAddons>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <private/qtx11extras_p.h>
#else
#include <QX11Info>
#endif

#include <cerrno>

namespace KWin
{

Platform::Platform(QObject *parent)
    : QObject(parent)
    , m_eglDisplay(EGL_NO_DISPLAY)
{
    connect(this, &Platform::outputDisabled, this, [this](Output *output) {
        if (m_primaryOutput == output) {
            setPrimaryOutput(enabledOutputs().value(0, nullptr));
        }
    });
    connect(this, &Platform::outputEnabled, this, [this](Output *output) {
        if (!m_primaryOutput) {
            setPrimaryOutput(output);
        }
    });
}

Platform::~Platform()
{
}

PlatformCursorImage Platform::cursorImage() const
{
    Cursor *cursor = Cursors::self()->currentCursor();
    return PlatformCursorImage(cursor->image(), cursor->hotspot());
}

InputBackend *Platform::createInputBackend()
{
    return nullptr;
}

OpenGLBackend *Platform::createOpenGLBackend()
{
    return nullptr;
}

QPainterBackend *Platform::createQPainterBackend()
{
    return nullptr;
}

QSharedPointer<DmaBufTexture> Platform::createDmaBufTexture(const QSize &size)
{
    Q_UNUSED(size)
    return {};
}

Edge *Platform::createScreenEdge(ScreenEdges *edges)
{
    return new Edge(edges);
}

void Platform::createPlatformCursor(QObject *parent)
{
    new InputRedirectionCursor(parent);
}

void Platform::requestOutputsChange(KWaylandServer::OutputConfigurationV2Interface *config)
{
    if (!m_supportsOutputChanges) {
        qCWarning(KWIN_CORE) << "This backend does not support configuration changes.";
        config->setFailed();
        return;
    }

    OutputConfiguration cfg;
    const auto changes = config->changes();
    for (auto it = changes.begin(); it != changes.end(); it++) {
        const KWaylandServer::OutputChangeSetV2 *changeset = it.value();
        auto output = findOutput(it.key()->uuid());
        if (!output) {
            qCWarning(KWIN_CORE) << "Could NOT find output matching " << it.key()->uuid();
            continue;
        }
        auto props = cfg.changeSet(output);
        props->enabled = changeset->enabled();
        props->pos = changeset->position();
        props->scale = changeset->scale();
        props->modeSize = changeset->size();
        props->refreshRate = changeset->refreshRate();
        props->transform = static_cast<Output::Transform>(changeset->transform());
        props->overscan = changeset->overscan();
        props->rgbRange = static_cast<Output::RgbRange>(changeset->rgbRange());
        props->vrrPolicy = static_cast<RenderLoop::VrrPolicy>(changeset->vrrPolicy());
    }

    const auto allOutputs = outputs();
    bool allDisabled = !std::any_of(allOutputs.begin(), allOutputs.end(), [&cfg](const auto &output) {
        return cfg.changeSet(output)->enabled;
    });
    if (allDisabled) {
        qCWarning(KWIN_CORE) << "Disabling all outputs through configuration changes is not allowed";
        config->setFailed();
        return;
    }

    if (applyOutputChanges(cfg)) {
        if (config->primaryChanged() || !primaryOutput()->isEnabled()) {
            auto requestedPrimaryOutput = findOutput(config->primary()->uuid());
            if (requestedPrimaryOutput && requestedPrimaryOutput->isEnabled()) {
                setPrimaryOutput(requestedPrimaryOutput);
            } else {
                auto defaultPrimaryOutput = enabledOutputs().constFirst();
                qCWarning(KWIN_CORE) << "Requested invalid primary screen, using" << defaultPrimaryOutput;
                setPrimaryOutput(defaultPrimaryOutput);
            }
        }
        Q_EMIT screens()->changed();
        config->setApplied();
    } else {
        qCDebug(KWIN_CORE) << "Applying config failed";
        config->setFailed();
    }
}

bool Platform::applyOutputChanges(const OutputConfiguration &config)
{
    const auto availableOutputs = outputs();
    QVector<Output *> toBeEnabledOutputs;
    QVector<Output *> toBeDisabledOutputs;
    for (const auto &output : availableOutputs) {
        if (config.constChangeSet(output)->enabled) {
            toBeEnabledOutputs << output;
        } else {
            toBeDisabledOutputs << output;
        }
    }
    for (const auto &output : toBeEnabledOutputs) {
        output->applyChanges(config);
    }
    for (const auto &output : toBeDisabledOutputs) {
        output->applyChanges(config);
    }
    return true;
}

Output *Platform::findOutput(int screenId) const
{
    return enabledOutputs().value(screenId);
}

Output *Platform::findOutput(const QUuid &uuid) const
{
    const auto outs = outputs();
    auto it = std::find_if(outs.constBegin(), outs.constEnd(),
                           [uuid](Output *output) {
                               return output->uuid() == uuid;
                           });
    if (it != outs.constEnd()) {
        return *it;
    }
    return nullptr;
}

Output *Platform::findOutput(const QString &name) const
{
    const auto candidates = outputs();
    for (Output *candidate : candidates) {
        if (candidate->name() == name) {
            return candidate;
        }
    }
    return nullptr;
}

Output *Platform::outputAt(const QPoint &pos) const
{
    Output *bestOutput = nullptr;
    int minDistance = INT_MAX;
    const auto candidates = enabledOutputs();
    for (Output *output : candidates) {
        const QRect &geo = output->geometry();
        if (geo.contains(pos)) {
            return output;
        }
        int distance = QPoint(geo.topLeft() - pos).manhattanLength();
        distance = std::min(distance, QPoint(geo.topRight() - pos).manhattanLength());
        distance = std::min(distance, QPoint(geo.bottomRight() - pos).manhattanLength());
        distance = std::min(distance, QPoint(geo.bottomLeft() - pos).manhattanLength());
        if (distance < minDistance) {
            minDistance = distance;
            bestOutput = output;
        }
    }
    return bestOutput;
}

void Platform::repaint(const QRect &rect)
{
    if (Compositor::compositing()) {
        Compositor::self()->scene()->addRepaint(rect);
    }
}

void Platform::setReady(bool ready)
{
    if (m_ready == ready) {
        return;
    }
    m_ready = ready;
    Q_EMIT readyChanged(m_ready);
}

Output *Platform::createVirtualOutput(const QString &name, const QSize &size, double scale)
{
    Q_UNUSED(name);
    Q_UNUSED(size);
    Q_UNUSED(scale);
    return nullptr;
}

void Platform::removeVirtualOutput(Output *output)
{
    Q_ASSERT(!output);
}

void Platform::warpPointer(const QPointF &globalPos)
{
    Q_UNUSED(globalPos)
}

bool Platform::supportsNativeFence() const
{
    if (Compositor *compositor = Compositor::self()) {
        return compositor->scene()->supportsNativeFence();
    }
    return false;
}

EGLDisplay KWin::Platform::sceneEglDisplay() const
{
    return m_eglDisplay;
}

void Platform::setSceneEglDisplay(EGLDisplay display)
{
    m_eglDisplay = display;
}

bool Platform::requiresCompositing() const
{
    return true;
}

bool Platform::compositingPossible() const
{
    return true;
}

QString Platform::compositingNotPossibleReason() const
{
    return QString();
}

bool Platform::openGLCompositingIsBroken() const
{
    return false;
}

void Platform::createOpenGLSafePoint(OpenGLSafePoint safePoint)
{
    Q_UNUSED(safePoint)
}

void Platform::startInteractiveWindowSelection(std::function<void(KWin::Window *)> callback, const QByteArray &cursorName)
{
    if (!input()) {
        callback(nullptr);
        return;
    }
    input()->startInteractiveWindowSelection(callback, cursorName);
}

void Platform::startInteractivePositionSelection(std::function<void(const QPoint &)> callback)
{
    if (!input()) {
        callback(QPoint(-1, -1));
        return;
    }
    input()->startInteractivePositionSelection(callback);
}

void Platform::setupActionForGlobalAccel(QAction *action)
{
    Q_UNUSED(action)
}

OverlayWindow *Platform::createOverlayWindow()
{
    return nullptr;
}

static quint32 monotonicTime()
{
    timespec ts;

    const int result = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (result) {
        qCWarning(KWIN_CORE, "Failed to query monotonic time: %s", strerror(errno));
    }

    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000L;
}

void Platform::updateXTime()
{
    switch (kwinApp()->operationMode()) {
    case Application::OperationModeX11:
        kwinApp()->setX11Time(QX11Info::getTimestamp(), Application::TimestampUpdate::Always);
        break;

    case Application::OperationModeXwayland:
        kwinApp()->setX11Time(monotonicTime(), Application::TimestampUpdate::Always);
        break;

    default:
        // Do not update the current X11 time stamp if it's the Wayland only session.
        break;
    }
}

OutlineVisual *Platform::createOutline(Outline *outline)
{
    if (Compositor::compositing()) {
        return new CompositedOutlineVisual(outline);
    }
    return nullptr;
}

void Platform::invertScreen()
{
    if (effects) {
        if (Effect *inverter = static_cast<EffectsHandlerImpl *>(effects)->provides(Effect::ScreenInversion)) {
            qCDebug(KWIN_CORE) << "inverting screen using Effect plugin";
            QMetaObject::invokeMethod(inverter, "toggleScreenInversion", Qt::DirectConnection);
        }
    }
}

void Platform::createEffectsHandler(Compositor *compositor, Scene *scene)
{
    new EffectsHandlerImpl(compositor, scene);
}

QString Platform::supportInformation() const
{
    return QStringLiteral("Name: %1\n").arg(metaObject()->className());
}

EGLContext Platform::sceneEglGlobalShareContext() const
{
    return m_globalShareContext;
}

void Platform::setSceneEglGlobalShareContext(EGLContext context)
{
    m_globalShareContext = context;
}

void Platform::setPrimaryOutput(Output *primary)
{
    if (primary == m_primaryOutput) {
        return;
    }
    Q_ASSERT(kwinApp()->isTerminating() || primary->isEnabled());
    m_primaryOutput = primary;
    Q_EMIT primaryOutputChanged(primary);
}

}
