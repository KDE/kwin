/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "platform.h"

#include "abstract_output.h"
#include <config-kwin.h>
#include "composite.h"
#include "cursor.h"
#include "effects.h"
#include "keyboard_input.h"
#include <KCoreAddons>
#include "overlaywindow.h"
#include "outline.h"
#include "pointer_input.h"
#include "scene.h"
#include "screens.h"
#include "screenedge.h"
#include "touch_input.h"
#include "wayland_server.h"
#include "waylandoutputconfig.h"

#include <KWaylandServer/outputconfiguration_v2_interface.h>
#include <KWaylandServer/outputchangeset_v2.h>

#include <QX11Info>

#include <cerrno>

namespace KWin
{

Platform::Platform(QObject *parent)
    : QObject(parent)
    , m_eglDisplay(EGL_NO_DISPLAY)
{
    connect(this, &Platform::outputDisabled, this, [this] (AbstractOutput *output) {
        if (m_primaryOutput == output) {
            setPrimaryOutput(enabledOutputs().value(0, nullptr));
        }
    });
    connect(this, &Platform::outputEnabled, this, [this] (AbstractOutput *output) {
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
    Cursor* cursor = Cursors::self()->currentCursor();
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

    WaylandOutputConfig cfg;
    const auto changes = config->changes();
    for (auto it = changes.begin(); it != changes.end(); it++) {
        const KWaylandServer::OutputChangeSetV2 *changeset = it.value();
        auto output = qobject_cast<AbstractWaylandOutput*>(findOutput(it.key()->uuid()));
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
        props->transform = static_cast<AbstractWaylandOutput::Transform>(changeset->transform());
        props->overscan = changeset->overscan();
        props->rgbRange = static_cast<AbstractWaylandOutput::RgbRange>(changeset->rgbRange());
        props->vrrPolicy = static_cast<RenderLoop::VrrPolicy>(changeset->vrrPolicy());
    }

    const auto allOutputs = outputs();
    bool allDisabled = !std::any_of(allOutputs.begin(), allOutputs.end(), [&cfg](const auto &output){
        auto o = qobject_cast<AbstractWaylandOutput*>(output);
        if (!o) {
            qCWarning(KWIN_CORE) << "Platform::requestOutputsChange should only be called for Wayland platforms!";
            return false;
        }
        return cfg.changeSet(o)->enabled;
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

bool Platform::applyOutputChanges(const WaylandOutputConfig &config)
{
    const auto availableOutputs = outputs();
    QVector<AbstractOutput*> toBeEnabledOutputs;
    QVector<AbstractOutput*> toBeDisabledOutputs;
    for (const auto &output : availableOutputs) {
        if (config.constChangeSet(qobject_cast<AbstractWaylandOutput*>(output))->enabled) {
            toBeEnabledOutputs << output;
        } else {
            toBeDisabledOutputs << output;
        }
    }
    for (const auto &output : toBeEnabledOutputs) {
        static_cast<AbstractWaylandOutput*>(output)->applyChanges(config);
    }
    for (const auto &output : toBeDisabledOutputs) {
        static_cast<AbstractWaylandOutput*>(output)->applyChanges(config);
    }
    return true;
}

AbstractOutput *Platform::findOutput(int screenId) const
{
    return enabledOutputs().value(screenId);
}

AbstractOutput *Platform::findOutput(const QUuid &uuid) const
{
    const auto outs = outputs();
    auto it = std::find_if(outs.constBegin(), outs.constEnd(),
        [uuid](AbstractOutput *output) {
            return output->uuid() == uuid; }
    );
    if (it != outs.constEnd()) {
        return *it;
    }
    return nullptr;
}

AbstractOutput *Platform::findOutput(const QString &name) const
{
    const auto candidates = outputs();
    for (AbstractOutput *candidate : candidates) {
        if (candidate->name() == name) {
            return candidate;
        }
    }
    return nullptr;
}

AbstractOutput *Platform::outputAt(const QPoint &pos) const
{
    AbstractOutput *bestOutput = nullptr;
    int minDistance = INT_MAX;
    const auto candidates = enabledOutputs();
    for (AbstractOutput *output : candidates) {
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

void Platform::triggerCursorRepaint()
{
    if (Compositor::compositing()) {
        Compositor::self()->scene()->addRepaint(m_cursor.lastRenderedGeometry);
        Compositor::self()->scene()->addRepaint(Cursors::self()->currentCursor()->geometry());
    }
}

void Platform::cursorRendered(const QRect &geometry)
{
    m_cursor.lastRenderedGeometry = geometry;
}

void Platform::keyboardKeyPressed(quint32 key, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->keyboard()->processKey(key, InputRedirection::KeyboardKeyPressed, time);
}

void Platform::keyboardKeyReleased(quint32 key, quint32 time)
{
    if (!input()) {
        return;
    }
   input()->keyboard()->processKey(key, InputRedirection::KeyboardKeyReleased, time);
}

void Platform::keyboardModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    if (!input()) {
        return;
    }
    input()->keyboard()->processModifiers(modsDepressed, modsLatched, modsLocked, group);
}

void Platform::keymapChange(int fd, uint32_t size)
{
    if (!input()) {
        return;
    }
    input()->keyboard()->processKeymapChange(fd, size);
}

void Platform::pointerAxisHorizontal(qreal delta, quint32 time, qint32 discreteDelta, InputRedirection::PointerAxisSource source)
{
    if (!input()) {
        return;
    }
    input()->pointer()->processAxis(InputRedirection::PointerAxisHorizontal, delta, discreteDelta, source, time);
}

void Platform::pointerAxisVertical(qreal delta, quint32 time, qint32 discreteDelta, InputRedirection::PointerAxisSource source)
{
    if (!input()) {
        return;
    }
    input()->pointer()->processAxis(InputRedirection::PointerAxisVertical, delta, discreteDelta, source, time);
}

void Platform::pointerButtonPressed(quint32 button, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->pointer()->processButton(button, InputRedirection::PointerButtonPressed, time);
}

void Platform::pointerButtonReleased(quint32 button, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->pointer()->processButton(button, InputRedirection::PointerButtonReleased, time);
}

void Platform::pointerMotion(const QPointF &position, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->pointer()->processMotionAbsolute(position, time);
}

void Platform::cancelTouchSequence()
{
    if (!input()) {
        return;
    }
    input()->touch()->cancel();
}

void Platform::touchCancel()
{
    if (!input()) {
        return;
    }
    input()->touch()->cancel();
}

void Platform::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->touch()->processDown(id, pos, time);
}

void Platform::touchFrame()
{
    if (!input()) {
        return;
    }
    input()->touch()->frame();
}

void Platform::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->touch()->processMotion(id, pos, time);
}

void Platform::touchUp(qint32 id, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->touch()->processUp(id, time);
}

void Platform::processSwipeGestureBegin(int fingerCount, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->pointer()->processSwipeGestureBegin(fingerCount, time);
}

void Platform::processSwipeGestureUpdate(const QSizeF &delta, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->pointer()->processSwipeGestureUpdate(delta, time);
}

void Platform::processSwipeGestureEnd(quint32 time)
{
    if (!input()) {
        return;
    }
    input()->pointer()->processSwipeGestureEnd(time);
}

void Platform::processSwipeGestureCancelled(quint32 time)
{
    if (!input()) {
        return;
    }
    input()->pointer()->processSwipeGestureCancelled(time);
}

void Platform::processPinchGestureBegin(int fingerCount, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->pointer()->processPinchGestureBegin(fingerCount, time);
}

void Platform::processPinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->pointer()->processPinchGestureUpdate(scale, angleDelta, delta, time);
}

void Platform::processPinchGestureEnd(quint32 time)
{
    if (!input()) {
        return;
    }
    input()->pointer()->processPinchGestureEnd(time);
}

void Platform::processPinchGestureCancelled(quint32 time)
{
    if (!input()) {
        return;
    }
    input()->pointer()->processPinchGestureCancelled(time);
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

bool Platform::isPerScreenRenderingEnabled() const
{
    return m_isPerScreenRenderingEnabled;
}

void Platform::setPerScreenRenderingEnabled(bool enabled)
{
    m_isPerScreenRenderingEnabled = enabled;
}

RenderLoop *Platform::renderLoop() const
{
    return nullptr;
}

AbstractOutput *Platform::createVirtualOutput(const QString &name, const QSize &size, double scale)
{
    Q_UNUSED(name);
    Q_UNUSED(size);
    Q_UNUSED(scale);
    return nullptr;
}

void Platform::removeVirtualOutput(AbstractOutput *output)
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

QSize Platform::screenSize() const
{
    return QSize();
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

void Platform::startInteractiveWindowSelection(std::function<void(KWin::Toplevel*)> callback, const QByteArray &cursorName)
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
    if (result)
        qCWarning(KWIN_CORE, "Failed to query monotonic time: %s", strerror(errno));

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
        if (Effect *inverter = static_cast<EffectsHandlerImpl*>(effects)->provides(Effect::ScreenInversion)) {
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

void Platform::setPrimaryOutput(AbstractOutput *primary)
{
    if (primary == m_primaryOutput) {
        return;
    }
    Q_ASSERT(kwinApp()->isTerminating() || primary->isEnabled());
    m_primaryOutput = primary;
    Q_EMIT primaryOutputChanged(primary);
}

}
