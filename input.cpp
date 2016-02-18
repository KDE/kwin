/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "input.h"
#include "keyboard_input.h"
#include "pointer_input.h"
#include "touch_input.h"
#include "client.h"
#include "effects.h"
#include "globalshortcuts.h"
#include "logind.h"
#include "main.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox/tabbox.h"
#endif
#include "unmanaged.h"
#include "screenedge.h"
#include "screens.h"
#include "workspace.h"
#if HAVE_INPUT
#include "libinput/connection.h"
#include "virtual_terminal.h"
#endif
#include "abstract_backend.h"
#include "shell_client.h"
#include "wayland_server.h"
#include <KWayland/Server/display.h>
#include <KWayland/Server/fakeinput_interface.h>
#include <KWayland/Server/seat_interface.h>
#include <decorations/decoratedclient.h>
#include <KDecoration2/Decoration>
//screenlocker
#include <KScreenLocker/KsldApp>
// Qt
#include <QKeyEvent>

#include <xkbcommon/xkbcommon.h>

namespace KWin
{

InputEventFilter::InputEventFilter() = default;

InputEventFilter::~InputEventFilter()
{
    if (input()) {
        input()->uninstallInputEventFilter(this);
    }
}

bool InputEventFilter::pointerEvent(QMouseEvent *event, quint32 nativeButton)
{
    Q_UNUSED(event)
    Q_UNUSED(nativeButton)
    return false;
}

bool InputEventFilter::wheelEvent(QWheelEvent *event)
{
    Q_UNUSED(event)
    return false;
}

bool InputEventFilter::keyEvent(QKeyEvent *event)
{
    Q_UNUSED(event)
    return false;
}

bool InputEventFilter::touchDown(quint32 id, const QPointF &point, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(point)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::touchMotion(quint32 id, const QPointF &point, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(point)
    Q_UNUSED(time)
    return false;
}

bool InputEventFilter::touchUp(quint32 id, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(time)
    return false;
}

#if HAVE_INPUT
class VirtualTerminalFilter : public InputEventFilter {
public:
    bool keyEvent(QKeyEvent *event) override {
        // really on press and not on release? X11 switches on press.
        if (event->type() == QEvent::KeyPress) {
            const xkb_keysym_t keysym = event->nativeVirtualKey();
            if (keysym >= XKB_KEY_XF86Switch_VT_1 && keysym <= XKB_KEY_XF86Switch_VT_12) {
                VirtualTerminal::self()->activate(keysym - XKB_KEY_XF86Switch_VT_1 + 1);
                return true;
            }
        }
        return false;
    }
};
#endif

class LockScreenFilter : public InputEventFilter {
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp());
        if (event->type() == QEvent::MouseMove) {
            if (event->buttons() == Qt::NoButton) {
                // update pointer window only if no button is pressed
                input()->pointer()->update();
            }
            if (pointerSurfaceAllowed()) {
                seat->setPointerPos(event->screenPos().toPoint());
            }
        } else if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
            if (pointerSurfaceAllowed()) {
                event->type() == QEvent::MouseButtonPress ? seat->pointerButtonPressed(nativeButton) : seat->pointerButtonReleased(nativeButton);
            }
        }
        return true;
    }
    bool wheelEvent(QWheelEvent *event) override {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        if (pointerSurfaceAllowed()) {
            seat->setTimestamp(event->timestamp());
            const Qt::Orientation orientation = event->angleDelta().x() == 0 ? Qt::Vertical : Qt::Horizontal;
            seat->pointerAxis(orientation, orientation == Qt::Horizontal ? event->angleDelta().x() : event->angleDelta().y());
        }
        return true;
    }
    bool keyEvent(QKeyEvent * event) override {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        // send event to KSldApp for global accel
        // if event is set to accepted it means a whitelisted shortcut was triggered
        // in that case we filter it out and don't process it further
        event->setAccepted(false);
        QCoreApplication::sendEvent(ScreenLocker::KSldApp::self(), event);
        if (event->isAccepted()) {
            return true;
        }

        // continue normal processing
        input()->keyboard()->update();
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp());
        if (!keyboardSurfaceAllowed()) {
            // don't pass event to seat
            return true;
        }
        switch (event->type()) {
        case QEvent::KeyPress:
            seat->keyPressed(event->nativeScanCode());
            break;
        case QEvent::KeyRelease:
            seat->keyReleased(event->nativeScanCode());
            break;
        default:
            break;
        }
        return true;
    }
    bool touchDown(quint32 id, const QPointF &pos, quint32 time) override {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        if (!seat->isTouchSequence()) {
            input()->touch()->update(pos);
        }
        if (touchSurfaceAllowed()) {
            input()->touch()->insertId(id, seat->touchDown(pos));
        }
        return true;
    }
    bool touchMotion(quint32 id, const QPointF &pos, quint32 time) override {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        if (touchSurfaceAllowed()) {
            const qint32 kwaylandId = input()->touch()->mappedId(id);
            if (kwaylandId != -1) {
                seat->touchMove(kwaylandId, pos);
            }
        }
        return true;
    }
    bool touchUp(quint32 id, quint32 time) override {
        if (!waylandServer()->isScreenLocked()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        if (touchSurfaceAllowed()) {
            const qint32 kwaylandId = input()->touch()->mappedId(id);
            if (kwaylandId != -1) {
                seat->touchUp(kwaylandId);
                input()->touch()->removeId(id);
            }
        }
        return true;
    }
private:
    bool surfaceAllowed(KWayland::Server::SurfaceInterface *(KWayland::Server::SeatInterface::*method)() const) const {
        if (KWayland::Server::SurfaceInterface *s = (waylandServer()->seat()->*method)()) {
            if (Toplevel *t = waylandServer()->findClient(s)) {
                return t->isLockScreen() || t->isInputMethod();
            }
            return false;
        }
        return true;
    }
    bool pointerSurfaceAllowed() const {
        return surfaceAllowed(&KWayland::Server::SeatInterface::focusedPointerSurface);
    }
    bool keyboardSurfaceAllowed() const {
        return surfaceAllowed(&KWayland::Server::SeatInterface::focusedKeyboardSurface);
    }
    bool touchSurfaceAllowed() const {
        return surfaceAllowed(&KWayland::Server::SeatInterface::focusedTouchSurface);
    }
};

class EffectsFilter : public InputEventFilter {
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        if (!effects) {
            return false;
        }
        return static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent(event);
    }
    bool keyEvent(QKeyEvent *event) override {
        if (!effects || !static_cast< EffectsHandlerImpl* >(effects)->hasKeyboardGrab()) {
            return false;
        }
        static_cast< EffectsHandlerImpl* >(effects)->grabbedKeyboardEvent(event);
        return true;
    }
};

class MoveResizeFilter : public InputEventFilter {
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        AbstractClient *c = workspace()->getMovingClient();
        if (!c) {
            return false;
        }
        switch (event->type()) {
        case QEvent::MouseMove:
            c->updateMoveResize(event->screenPos().toPoint());
            break;
        case QEvent::MouseButtonRelease:
            if (event->buttons() == Qt::NoButton) {
                c->endMoveResize();
            }
            break;
        default:
            break;
        }
        return true;
    }
    bool wheelEvent(QWheelEvent *event) override {
        Q_UNUSED(event)
        // filter out while moving a window
        return workspace()->getMovingClient() != nullptr;
    }
    bool keyEvent(QKeyEvent *event) override {
        AbstractClient *c = workspace()->getMovingClient();
        if (!c) {
            return false;
        }
        if (event->type() == QEvent::KeyPress) {
            c->keyPressEvent(event->key() | event->modifiers());
            if (c->isMove() || c->isResize()) {
                // only update if mode didn't end
                c->updateMoveResize(input()->globalPointer());
            }
        }
        return true;
    }
};

class GlobalShortcutFilter : public InputEventFilter {
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton);
        if (event->type() == QEvent::MouseButtonPress) {
            if (input()->shortcuts()->processPointerPressed(event->modifiers(), event->buttons())) {
                return true;
            }
        }
        return false;
    }
    bool wheelEvent(QWheelEvent *event) override {
        if (event->modifiers() == Qt::NoModifier) {
            return false;
        }
        PointerAxisDirection direction = PointerAxisUp;
        if (event->angleDelta().x() < 0) {
            direction = PointerAxisRight;
        } else if (event->angleDelta().x() > 0) {
            direction = PointerAxisLeft;
        } else if (event->angleDelta().y() < 0) {
            direction = PointerAxisDown;
        } else if (event->angleDelta().y() > 0) {
            direction = PointerAxisUp;
        }
        return input()->shortcuts()->processAxis(event->modifiers(), direction);
    }
    bool keyEvent(QKeyEvent *event) override {
        if (event->type() == QEvent::KeyPress) {
            return input()->shortcuts()->processKey(event->modifiers(), event->nativeVirtualKey());
        }
        return false;
    }
};

class InternalWindowEventFilter : public InputEventFilter {
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        auto internal = input()->pointer()->internalWindow();
        if (!internal) {
            return false;
        }
        QMouseEvent e(event->type(),
                        event->pos() - internal->position(),
                        event->globalPos(),
                        event->button(), event->buttons(), event->modifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(internal.data(), &e);
        return e.isAccepted();
    }
    bool wheelEvent(QWheelEvent *event) override {
        auto internal = input()->pointer()->internalWindow();
        if (!internal) {
            return false;
        }
        const QPointF localPos = event->globalPosF() - QPointF(internal->x(), internal->y());
        const Qt::Orientation orientation = (event->angleDelta().x() != 0) ? Qt::Horizontal : Qt::Vertical;
        const int delta = event->angleDelta().x() != 0 ? event->angleDelta().x() : event->angleDelta().y();
        QWheelEvent e(localPos, event->globalPosF(), QPoint(),
                        event->angleDelta(),
                        delta,
                        orientation,
                        event->buttons(),
                        event->modifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(internal.data(), &e);
        return e.isAccepted();
    }
    bool keyEvent(QKeyEvent *event) override {
        const auto &internalClients = waylandServer()->internalClients();
        if (internalClients.isEmpty()) {
            return false;
        }
        QWindow *found = nullptr;
        auto it = internalClients.end();
        do {
            it--;
            if (QWindow *w = (*it)->internalWindow()) {
                if (!w->isVisible()) {
                    continue;
                }
                found = w;
                break;
            }
        } while (it != internalClients.begin());
        if (!found) {
            return false;
        }
        event->setAccepted(false);
        return QCoreApplication::sendEvent(found, event);
    }
};

class DecorationEventFilter : public InputEventFilter {
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        auto decoration = input()->pointer()->decoration();
        if (!decoration) {
            return false;
        }
        const QPointF p = event->globalPos() - decoration->client()->pos();
        switch (event->type()) {
        case QEvent::MouseMove: {
            if (event->buttons() == Qt::NoButton) {
                return false;
            }
            QHoverEvent e(QEvent::HoverMove, p, p);
            QCoreApplication::instance()->sendEvent(decoration->decoration(), &e);
            decoration->client()->processDecorationMove();
            return true;
        }
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease: {
            QMouseEvent e(event->type(), p, event->globalPos(), event->button(), event->buttons(), event->modifiers());
            e.setAccepted(false);
            QCoreApplication::sendEvent(decoration->decoration(), &e);
            if (!e.isAccepted() && event->type() == QEvent::MouseButtonPress) {
                decoration->client()->processDecorationButtonPress(&e);
            }
            if (event->type() == QEvent::MouseButtonRelease) {
                decoration->client()->processDecorationButtonRelease(&e);
            }
            input()->pointer()->installCursorFromDecoration();
            return true;
        }
        default:
            break;
        }
        return false;
    }
    bool wheelEvent(QWheelEvent *event) override {
        auto decoration = input()->pointer()->decoration();
        if (!decoration) {
            return false;
        }
        const QPointF localPos = event->globalPosF() - decoration->client()->pos();
        const Qt::Orientation orientation = (event->angleDelta().x() != 0) ? Qt::Horizontal : Qt::Vertical;
        const int delta = event->angleDelta().x() != 0 ? event->angleDelta().x() : event->angleDelta().y();
        QWheelEvent e(localPos, event->globalPosF(), QPoint(),
                        event->angleDelta(),
                        delta,
                        orientation,
                        event->buttons(),
                        event->modifiers());
        e.setAccepted(false);
        QCoreApplication::sendEvent(decoration.data(), &e);
        if (e.isAccepted()) {
            return true;
        }
        if (orientation == Qt::Vertical && decoration->decoration()->titleBar().contains(localPos.toPoint())) {
            decoration->client()->performMouseCommand(options->operationTitlebarMouseWheel(delta * -1),
                                                        event->globalPosF().toPoint());
        }
        return true;
    }
};

#ifdef KWIN_BUILD_TABBOX
class TabBoxInputFilter : public InputEventFilter
{
public:
    bool keyEvent(QKeyEvent *event) override {
        if (!TabBox::TabBox::self() || !TabBox::TabBox::self()->isGrabbed()) {
            return false;
        }
        TabBox::TabBox::self()->keyPress(event->modifiers() | event->key());
        return true;
    }
};
#endif

class ScreenEdgeInputFilter : public InputEventFilter
{
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        ScreenEdges::self()->isEntered(event);
        // always forward
        return false;
    }
};

/**
 * This filter implements window actions. If the event should not be passed to the
 * current pointer window it will filter out the event
 **/
class WindowActionInputFilter : public InputEventFilter
{
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        Q_UNUSED(nativeButton)
        if (event->type() != QEvent::MouseButtonPress) {
            return false;
        }
        AbstractClient *c = dynamic_cast<AbstractClient*>(input()->pointer()->window().data());
        if (!c) {
            return false;
        }
        bool wasAction = false;
        Options::MouseCommand command = Options::MouseNothing;
        if (event->modifiers() == options->commandAllModifier()) {
            wasAction = true;
            switch (event->button()) {
            case Qt::LeftButton:
                command = options->commandAll1();
                break;
            case Qt::MiddleButton:
                command = options->commandAll2();
                break;
            case Qt::RightButton:
                command = options->commandAll3();
                break;
            default:
                // nothing
                break;
            }
        } else {
            c->getMouseCommand(event->button(), &wasAction);
        }
        if (wasAction) {
            return !c->performMouseCommand(command, event->globalPos());
        }
        return false;
    }
    bool wheelEvent(QWheelEvent *event) override {
        if (event->angleDelta().y() == 0) {
            // only actions on vertical scroll
            return false;
        }
        AbstractClient *c = dynamic_cast<AbstractClient*>(input()->pointer()->window().data());
        if (!c) {
            return false;
        }
        bool wasAction = false;
        Options::MouseCommand command = Options::MouseNothing;
        if (event->modifiers() == options->commandAllModifier()) {
            wasAction = true;
            command = options->operationWindowMouseWheel(event->angleDelta().y());
        } else {
            command = c->getWheelCommand(Qt::Vertical, &wasAction);
        }
        if (wasAction) {
            return !c->performMouseCommand(command, event->globalPos());
        }
        return false;
    }
    bool touchDown(quint32 id, const QPointF &pos, quint32 time) override {
        Q_UNUSED(id)
        Q_UNUSED(time)
        auto seat = waylandServer()->seat();
        if (seat->isTouchSequence()) {
            return false;
        }
        input()->touch()->update(pos);
        AbstractClient *c = dynamic_cast<AbstractClient*>(input()->touch()->window().data());
        if (!c) {
            return false;
        }
        bool wasAction = false;
        const Options::MouseCommand command = c->getMouseCommand(Qt::LeftButton, &wasAction);
        if (wasAction) {
            return !c->performMouseCommand(command, pos.toPoint());
        }
        return false;
    }
};

/**
 * The remaining default input filter which forwards events to other windows
 **/
class ForwardInputFilter : public InputEventFilter
{
public:
    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp());
        switch (event->type()) {
        case QEvent::MouseMove:
            if (event->buttons() == Qt::NoButton) {
                // update pointer window only if no button is pressed
                input()->pointer()->update();
            }
            seat->setPointerPos(event->globalPos());
            break;
        case QEvent::MouseButtonPress:
            seat->pointerButtonPressed(nativeButton);
            break;
        case QEvent::MouseButtonRelease:
            seat->pointerButtonReleased(nativeButton);
            if (event->buttons() == Qt::NoButton) {
                input()->pointer()->update();
            }
            break;
        default:
            break;
        }
        return true;
    }
    bool wheelEvent(QWheelEvent *event) override {
        auto seat = waylandServer()->seat();
        seat->setTimestamp(event->timestamp());
        const Qt::Orientation orientation = event->angleDelta().x() == 0 ? Qt::Vertical : Qt::Horizontal;
        seat->pointerAxis(orientation, orientation == Qt::Horizontal ? event->angleDelta().x() : event->angleDelta().y());
        return true;
    }
    bool keyEvent(QKeyEvent *event) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        input()->keyboard()->update();
        seat->setTimestamp(event->timestamp());
        switch (event->type()) {
        case QEvent::KeyPress:
            seat->keyPressed(event->nativeScanCode());
            break;
        case QEvent::KeyRelease:
            seat->keyReleased(event->nativeScanCode());
            break;
        default:
            break;
        }
        return true;
    }
    bool touchDown(quint32 id, const QPointF &pos, quint32 time) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        if (!seat->isTouchSequence()) {
            input()->touch()->update(pos);
        }
        input()->touch()->insertId(id, seat->touchDown(pos));
        return true;
    }
    bool touchMotion(quint32 id, const QPointF &pos, quint32 time) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        const qint32 kwaylandId = input()->touch()->mappedId(id);
        if (kwaylandId != -1) {
            seat->touchMove(kwaylandId, pos);
        }
        return true;
    }
    bool touchUp(quint32 id, quint32 time) override {
        if (!workspace()) {
            return false;
        }
        auto seat = waylandServer()->seat();
        seat->setTimestamp(time);
        const qint32 kwaylandId = input()->touch()->mappedId(id);
        if (kwaylandId != -1) {
            seat->touchUp(kwaylandId);
            input()->touch()->removeId(id);
        }
        return true;
    }
};

KWIN_SINGLETON_FACTORY(InputRedirection)

InputRedirection::InputRedirection(QObject *parent)
    : QObject(parent)
    , m_keyboard(new KeyboardInputRedirection(this))
    , m_pointer(new PointerInputRedirection(this))
    , m_touch(new TouchInputRedirection(this))
    , m_shortcuts(new GlobalShortcutsManager(this))
{
    qRegisterMetaType<KWin::InputRedirection::KeyboardKeyState>();
    qRegisterMetaType<KWin::InputRedirection::PointerButtonState>();
    qRegisterMetaType<KWin::InputRedirection::PointerAxis>();
#if HAVE_INPUT
    if (Application::usesLibinput()) {
        if (VirtualTerminal::self()) {
            setupLibInput();
        } else {
            connect(kwinApp(), &Application::virtualTerminalCreated, this, &InputRedirection::setupLibInput);
        }
    }
#endif
    connect(kwinApp(), &Application::workspaceCreated, this, &InputRedirection::setupWorkspace);
    reconfigure();
}

InputRedirection::~InputRedirection()
{
    s_self = NULL;
    qDeleteAll(m_filters);
}

void InputRedirection::installInputEventFilter(InputEventFilter *filter)
{
    m_filters << filter;
}

void InputRedirection::prepandInputEventFilter(InputEventFilter *filter)
{
    m_filters.prepend(filter);
}

void InputRedirection::uninstallInputEventFilter(InputEventFilter *filter)
{
    m_filters.removeAll(filter);
}

void InputRedirection::init()
{
    m_shortcuts->init();
}

void InputRedirection::setupWorkspace()
{
    if (waylandServer()) {
        using namespace KWayland::Server;
        FakeInputInterface *fakeInput = waylandServer()->display()->createFakeInput(this);
        fakeInput->create();
        connect(fakeInput, &FakeInputInterface::deviceCreated, this,
            [this] (FakeInputDevice *device) {
                connect(device, &FakeInputDevice::authenticationRequested, this,
                    [this, device] (const QString &application, const QString &reason) {
                        // TODO: make secure
                        device->setAuthentication(true);
                    }
                );
                connect(device, &FakeInputDevice::pointerMotionRequested, this,
                    [this] (const QSizeF &delta) {
                        // TODO: Fix time
                        m_pointer->processMotion(globalPointer() + QPointF(delta.width(), delta.height()), 0);
                    }
                );
                connect(device, &FakeInputDevice::pointerButtonPressRequested, this,
                    [this] (quint32 button) {
                        // TODO: Fix time
                        m_pointer->processButton(button, InputRedirection::PointerButtonPressed, 0);
                    }
                );
                connect(device, &FakeInputDevice::pointerButtonReleaseRequested, this,
                    [this] (quint32 button) {
                        // TODO: Fix time
                        m_pointer->processButton(button, InputRedirection::PointerButtonReleased, 0);
                    }
                );
                connect(device, &FakeInputDevice::pointerAxisRequested, this,
                    [this] (Qt::Orientation orientation, qreal delta) {
                        // TODO: Fix time
                        InputRedirection::PointerAxis axis;
                        switch (orientation) {
                        case Qt::Horizontal:
                            axis = InputRedirection::PointerAxisHorizontal;
                            break;
                        case Qt::Vertical:
                            axis = InputRedirection::PointerAxisVertical;
                            break;
                        default:
                            Q_UNREACHABLE();
                            break;
                        }
                        // TODO: Fix time
                        m_pointer->processAxis(axis, delta, 0);
                    }
                );
            }
        );
        connect(this, &InputRedirection::keyboardModifiersChanged, waylandServer(),
            [this] {
                if (!waylandServer()->seat()) {
                    return;
                }
                waylandServer()->seat()->updateKeyboardModifiers(m_keyboard->xkb()->getMods(XKB_STATE_MODS_DEPRESSED),
                                                                 m_keyboard->xkb()->getMods(XKB_STATE_MODS_LATCHED),
                                                                 m_keyboard->xkb()->getMods(XKB_STATE_MODS_LOCKED),
                                                                 m_keyboard->xkb()->getGroup());
            }
        );
        connect(workspace(), &Workspace::configChanged, this, &InputRedirection::reconfigure);

        m_keyboard->init();
        m_pointer->init();
        m_touch->init();
    }
    setupInputFilters();
}

void InputRedirection::setupInputFilters()
{
#if HAVE_INPUT
    if (VirtualTerminal::self()) {
        installInputEventFilter(new VirtualTerminalFilter);
    }
#endif
    if (waylandServer()) {
        installInputEventFilter(new LockScreenFilter);
    }
    installInputEventFilter(new ScreenEdgeInputFilter);
    installInputEventFilter(new EffectsFilter);
    installInputEventFilter(new MoveResizeFilter);
    installInputEventFilter(new GlobalShortcutFilter);
#ifdef KWIN_BUILD_TABBOX
    installInputEventFilter(new TabBoxInputFilter);
#endif
    installInputEventFilter(new InternalWindowEventFilter);
    installInputEventFilter(new DecorationEventFilter);
    if (waylandServer()) {
        installInputEventFilter(new WindowActionInputFilter);
        installInputEventFilter(new ForwardInputFilter);
    }
}

void InputRedirection::reconfigure()
{
#if HAVE_INPUT
    if (Application::usesLibinput()) {
        const auto config = KSharedConfig::openConfig(QStringLiteral("kcminputrc"))->group(QStringLiteral("keyboard"));
        const int delay = config.readEntry("RepeatDelay", 660);
        const int rate = config.readEntry("RepeatRate", 25);
        const bool enabled = config.readEntry("KeyboardRepeating", 0) == 0;

        waylandServer()->seat()->setKeyRepeatInfo(enabled ? rate : 0, delay);
    }
#endif
}

static KWayland::Server::SeatInterface *findSeat()
{
    auto server = waylandServer();
    if (!server) {
        return nullptr;
    }
    return server->seat();
}

void InputRedirection::setupLibInput()
{
#if HAVE_INPUT
    if (!Application::usesLibinput()) {
        return;
    }
    if (m_libInput) {
        return;
    }
    LibInput::Connection *conn = LibInput::Connection::create(this);
    m_libInput = conn;
    if (conn) {
        conn->setup();
        connect(conn, &LibInput::Connection::eventsRead, this,
            [this] {
                m_libInput->processEvents();
            }, Qt::QueuedConnection
        );
        connect(conn, &LibInput::Connection::pointerButtonChanged, m_pointer, &PointerInputRedirection::processButton);
        connect(conn, &LibInput::Connection::pointerAxisChanged, m_pointer, &PointerInputRedirection::processAxis);
        connect(conn, &LibInput::Connection::keyChanged, m_keyboard, &KeyboardInputRedirection::processKey);
        connect(conn, &LibInput::Connection::pointerMotion, this,
            [this] (QPointF delta, uint32_t time) {
                m_pointer->processMotion(m_pointer->pos() + delta, time);
            }
        );
        connect(conn, &LibInput::Connection::pointerMotionAbsolute, this,
            [this] (QPointF orig, QPointF screen, uint32_t time) {
                Q_UNUSED(orig)
                m_pointer->processMotion(screen, time);
            }
        );
        connect(conn, &LibInput::Connection::touchDown, m_touch, &TouchInputRedirection::processDown);
        connect(conn, &LibInput::Connection::touchUp, m_touch, &TouchInputRedirection::processUp);
        connect(conn, &LibInput::Connection::touchMotion, m_touch, &TouchInputRedirection::processMotion);
        connect(conn, &LibInput::Connection::touchCanceled, m_touch, &TouchInputRedirection::cancel);
        connect(conn, &LibInput::Connection::touchFrame, m_touch, &TouchInputRedirection::frame);
        if (screens()) {
            setupLibInputWithScreens();
        } else {
            connect(kwinApp(), &Application::screensCreated, this, &InputRedirection::setupLibInputWithScreens);
        }
        if (auto s = findSeat()) {
            s->setHasKeyboard(conn->hasKeyboard());
            s->setHasPointer(conn->hasPointer());
            s->setHasTouch(conn->hasTouch());
            connect(conn, &LibInput::Connection::hasKeyboardChanged, this,
                [this, s] (bool set) {
                    if (m_libInput->isSuspended()) {
                        return;
                    }
                    s->setHasKeyboard(set);
                }
            );
            connect(conn, &LibInput::Connection::hasPointerChanged, this,
                [this, s] (bool set) {
                    if (m_libInput->isSuspended()) {
                        return;
                    }
                    s->setHasPointer(set);
                }
            );
            connect(conn, &LibInput::Connection::hasTouchChanged, this,
                [this, s] (bool set) {
                    if (m_libInput->isSuspended()) {
                        return;
                    }
                    s->setHasTouch(set);
                }
            );
        }
        connect(VirtualTerminal::self(), &VirtualTerminal::activeChanged, m_libInput,
            [this] (bool active) {
                if (!active) {
                    m_libInput->deactivate();
                }
            }
        );
    }
#endif
}

void InputRedirection::setupLibInputWithScreens()
{
#if HAVE_INPUT
    if (!screens() || !m_libInput) {
        return;
    }
    m_libInput->setScreenSize(screens()->size());
    connect(screens(), &Screens::sizeChanged, this,
        [this] {
            m_libInput->setScreenSize(screens()->size());
        }
    );
#endif
}

void InputRedirection::processPointerMotion(const QPointF &pos, uint32_t time)
{
    m_pointer->processMotion(pos, time);
}

void InputRedirection::processPointerButton(uint32_t button, InputRedirection::PointerButtonState state, uint32_t time)
{
    m_pointer->processButton(button, state, time);
}

void InputRedirection::processPointerAxis(InputRedirection::PointerAxis axis, qreal delta, uint32_t time)
{
    m_pointer->processAxis(axis, delta, time);
}

void InputRedirection::processKeyboardKey(uint32_t key, InputRedirection::KeyboardKeyState state, uint32_t time)
{
    m_keyboard->processKey(key, state, time);
}

void InputRedirection::processKeyboardModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    m_keyboard->processModifiers(modsDepressed, modsLatched, modsLocked, group);
}

void InputRedirection::processKeymapChange(int fd, uint32_t size)
{
    m_keyboard->processKeymapChange(fd, size);
}

void InputRedirection::processTouchDown(qint32 id, const QPointF &pos, quint32 time)
{
    m_touch->processDown(id, pos, time);
}

void InputRedirection::processTouchUp(qint32 id, quint32 time)
{
    m_touch->processUp(id, time);
}

void InputRedirection::processTouchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    m_touch->processMotion(id, pos, time);
}

void InputRedirection::cancelTouch()
{
    m_touch->cancel();
}

void InputRedirection::touchFrame()
{
    m_touch->frame();
}

Qt::MouseButtons InputRedirection::qtButtonStates() const
{
    return m_pointer->buttons();
}

static bool acceptsInput(Toplevel *t, const QPoint &pos)
{
    const QRegion input = t->inputShape();
    if (input.isEmpty()) {
        return true;
    }
    return input.translated(t->pos()).contains(pos);
}

Toplevel *InputRedirection::findToplevel(const QPoint &pos)
{
    if (!Workspace::self()) {
        return nullptr;
    }
    const bool isScreenLocked = waylandServer() && waylandServer()->isScreenLocked();
    // TODO: check whether the unmanaged wants input events at all
    if (!isScreenLocked) {
        const UnmanagedList &unmanaged = Workspace::self()->unmanagedList();
        foreach (Unmanaged *u, unmanaged) {
            if (u->geometry().contains(pos) && acceptsInput(u, pos)) {
                return u;
            }
        }
    }
    const ToplevelList &stacking = Workspace::self()->stackingOrder();
    if (stacking.isEmpty()) {
        return NULL;
    }
    auto it = stacking.end();
    do {
        --it;
        Toplevel *t = (*it);
        if (t->isDeleted()) {
            // a deleted window doesn't get mouse events
            continue;
        }
        if (AbstractClient *c = dynamic_cast<AbstractClient*>(t)) {
            if (!c->isOnCurrentActivity() || !c->isOnCurrentDesktop() || c->isMinimized() || !c->isCurrentTab()) {
                continue;
            }
        }
        if (!t->readyForPainting()) {
            continue;
        }
        if (isScreenLocked) {
            if (!t->isLockScreen() && !t->isInputMethod()) {
                continue;
            }
        }
        if (t->geometry().contains(pos) && acceptsInput(t, pos)) {
            return t;
        }
    } while (it != stacking.begin());
    return NULL;
}

Qt::KeyboardModifiers InputRedirection::keyboardModifiers() const
{
    return m_keyboard->modifiers();
}

void InputRedirection::registerShortcut(const QKeySequence &shortcut, QAction *action)
{
    m_shortcuts->registerShortcut(action, shortcut);
    registerShortcutForGlobalAccelTimestamp(action);
}

void InputRedirection::registerPointerShortcut(Qt::KeyboardModifiers modifiers, Qt::MouseButton pointerButtons, QAction *action)
{
    m_shortcuts->registerPointerShortcut(action, modifiers, pointerButtons);
}

void InputRedirection::registerAxisShortcut(Qt::KeyboardModifiers modifiers, PointerAxisDirection axis, QAction *action)
{
    m_shortcuts->registerAxisShortcut(action, modifiers, axis);
}

void InputRedirection::registerGlobalAccel(KGlobalAccelInterface *interface)
{
    m_shortcuts->setKGlobalAccelInterface(interface);
}

void InputRedirection::registerShortcutForGlobalAccelTimestamp(QAction *action)
{
    connect(action, &QAction::triggered, kwinApp(), [action] {
        QVariant timestamp = action->property("org.kde.kglobalaccel.activationTimestamp");
        bool ok = false;
        const quint32 t = timestamp.toULongLong(&ok);
        if (ok) {
            kwinApp()->setX11Time(t);
        }
    });
}

void InputRedirection::warpPointer(const QPointF &pos)
{
    m_pointer->warp(pos);
}

bool InputRedirection::supportsPointerWarping() const
{
    return m_pointer->supportsWarping();
}


QPointF InputRedirection::globalPointer() const
{
    return m_pointer->pos();
}

} // namespace
