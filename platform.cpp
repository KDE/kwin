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
#include <KCoreAddons>
#include "overlaywindow.h"
#include "outline.h"
#include "pointer_input.h"
#include "scene.h"
#include "screens.h"
#include "screenedge.h"
#include "wayland_server.h"
#include "colorcorrection/manager.h"

#include <KWaylandServer/outputconfiguration_interface.h>
#include <KWaylandServer/outputchangeset.h>

#include <QX11Info>

#include <cerrno>

namespace KWin
{

Platform::Platform(QObject *parent)
    : QObject(parent)
    , m_eglDisplay(EGL_NO_DISPLAY)
{
    setSoftWareCursor(false);
    m_colorCorrect = new ColorCorrect::Manager(this);
    connect(Cursors::self(), &Cursors::currentCursorRendered, this, &Platform::cursorRendered);
}

Platform::~Platform()
{
    if (m_eglDisplay != EGL_NO_DISPLAY) {
        eglTerminate(m_eglDisplay);
    }
}

PlatformCursorImage Platform::cursorImage() const
{
    Cursor* cursor = Cursors::self()->currentCursor();
    return PlatformCursorImage(cursor->image(), cursor->hotspot());
}

void Platform::hideCursor()
{
    m_hideCursorCounter++;
    if (m_hideCursorCounter == 1) {
        doHideCursor();
    }
}

void Platform::doHideCursor()
{
}

void Platform::showCursor()
{
    m_hideCursorCounter--;
    if (m_hideCursorCounter == 0) {
        doShowCursor();
    }
}

void Platform::doShowCursor()
{
}

Screens *Platform::createScreens(QObject *parent)
{
    Q_UNUSED(parent)
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

void Platform::prepareShutdown()
{
    setOutputsEnabled(false);
}

Edge *Platform::createScreenEdge(ScreenEdges *edges)
{
    return new Edge(edges);
}

void Platform::createPlatformCursor(QObject *parent)
{
    new InputRedirectionCursor(parent);
}

void Platform::requestOutputsChange(KWaylandServer::OutputConfigurationInterface *config)
{
    if (!m_supportsOutputChanges) {
        qCWarning(KWIN_CORE) << "This backend does not support configuration changes.";
        config->setFailed();
        return;
    }

    using Enablement = KWaylandServer::OutputDeviceInterface::Enablement;

    const auto changes = config->changes();

    //process all non-disabling changes
    for (auto it = changes.begin(); it != changes.end(); it++) {
        const KWaylandServer::OutputChangeSet *changeset = it.value();

        auto output = findOutput(it.key()->uuid());
        if (!output) {
            qCWarning(KWIN_CORE) << "Could NOT find output matching " << it.key()->uuid();
            continue;
        }

        if (changeset->enabledChanged() &&
                changeset->enabled() == Enablement::Enabled) {
            output->setEnabled(true);
        }
        output->applyChanges(changeset);
    }

    //process any disable requests
    for (auto it = changes.begin(); it != changes.end(); it++) {
        const KWaylandServer::OutputChangeSet *changeset = it.value();

        if (changeset->enabledChanged() &&
                changeset->enabled() == Enablement::Disabled) {
            if (enabledOutputs().count() == 1) {
                // TODO: check beforehand this condition and set failed otherwise
                // TODO: instead create a dummy output?
                qCWarning(KWIN_CORE) << "Not disabling final screen" << it.key()->uuid();
                continue;
            }
            auto output = findOutput(it.key()->uuid());
            if (!output) {
                qCWarning(KWIN_CORE) << "Could NOT find output matching " << it.key()->uuid();
                continue;
            }
            output->setEnabled(false);
        }
    }
    emit screens()->changed();
    config->setApplied();
}

AbstractOutput *Platform::findOutput(int screenId)
{
    return enabledOutputs().value(screenId);
}

AbstractOutput *Platform::findOutput(const QByteArray &uuid)
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

void Platform::setSoftWareCursor(bool set)
{
    if (qEnvironmentVariableIsSet("KWIN_FORCE_SW_CURSOR")) {
        set = true;
    }
    if (m_softWareCursor == set) {
        return;
    }
    m_softWareCursor = set;
    if (m_softWareCursor) {
        connect(Cursors::self(), &Cursors::positionChanged, this, &Platform::triggerCursorRepaint);
        connect(Cursors::self(), &Cursors::currentCursorChanged, this, &Platform::triggerCursorRepaint);
    } else {
        disconnect(Cursors::self(), &Cursors::positionChanged, this, &Platform::triggerCursorRepaint);
        disconnect(Cursors::self(), &Cursors::currentCursorChanged, this, &Platform::triggerCursorRepaint);
    }
    triggerCursorRepaint();
}

void Platform::triggerCursorRepaint()
{
    if (!Compositor::self()) {
        return;
    }
    Compositor::self()->addRepaint(m_cursor.lastRenderedGeometry);
    Compositor::self()->addRepaint(Cursors::self()->currentCursor()->geometry());
}

void Platform::cursorRendered(const QRect &geometry)
{
    if (m_softWareCursor) {
        m_cursor.lastRenderedGeometry = geometry;
    }
}

void Platform::keyboardKeyPressed(quint32 key, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processKeyboardKey(key, InputRedirection::KeyboardKeyPressed, time);
}

void Platform::keyboardKeyReleased(quint32 key, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processKeyboardKey(key, InputRedirection::KeyboardKeyReleased, time);
}

void Platform::keyboardModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    if (!input()) {
        return;
    }
    input()->processKeyboardModifiers(modsDepressed, modsLatched, modsLocked, group);
}

void Platform::keymapChange(int fd, uint32_t size)
{
    if (!input()) {
        return;
    }
    input()->processKeymapChange(fd, size);
}

void Platform::pointerAxisHorizontal(qreal delta, quint32 time, qint32 discreteDelta, InputRedirection::PointerAxisSource source)
{
    if (!input()) {
        return;
    }
    input()->processPointerAxis(InputRedirection::PointerAxisHorizontal, delta, discreteDelta, source, time);
}

void Platform::pointerAxisVertical(qreal delta, quint32 time, qint32 discreteDelta, InputRedirection::PointerAxisSource source)
{
    if (!input()) {
        return;
    }
    input()->processPointerAxis(InputRedirection::PointerAxisVertical, delta, discreteDelta, source, time);
}

void Platform::pointerButtonPressed(quint32 button, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processPointerButton(button, InputRedirection::PointerButtonPressed, time);
}

void Platform::pointerButtonReleased(quint32 button, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processPointerButton(button, InputRedirection::PointerButtonReleased, time);
}

void Platform::pointerMotion(const QPointF &position, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processPointerMotion(position, time);
}

void Platform::touchCancel()
{
    if (!input()) {
        return;
    }
    input()->cancelTouch();
}

void Platform::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processTouchDown(id, pos, time);
}

void Platform::touchFrame()
{
    if (!input()) {
        return;
    }
    input()->touchFrame();
}

void Platform::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processTouchMotion(id, pos, time);
}

void Platform::touchUp(qint32 id, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processTouchUp(id, time);
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
    if (!Compositor::self()) {
        return;
    }
    Compositor::self()->addRepaint(rect);
}

void Platform::setReady(bool ready)
{
    if (m_ready == ready) {
        return;
    }
    m_ready = ready;
    emit readyChanged(m_ready);
}

void Platform::warpPointer(const QPointF &globalPos)
{
    Q_UNUSED(globalPos)
}

bool Platform::supportsSurfacelessContext() const
{
    Compositor *compositor = Compositor::self();
    if (Q_UNLIKELY(!compositor)) {
        return false;
    }
    if (Scene *scene = compositor->scene()) {
        return scene->supportsSurfacelessContext();
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

QVector<QRect> Platform::screenGeometries() const
{
    return QVector<QRect>({QRect(QPoint(0, 0), screenSize())});
}

QVector<qreal> Platform::screenScales() const
{
    return QVector<qreal>({1});
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

Decoration::Renderer *Platform::createDecorationRenderer(Decoration::DecoratedClientImpl *client)
{
    if (Compositor::self()->scene()) {
        return Compositor::self()->scene()->createDecorationRenderer(client);
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

}
