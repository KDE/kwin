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
#include "client.h"
#include "effects.h"
#include "globalshortcuts.h"
#include "logind.h"
#include "main.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox/tabbox.h"
#endif
#include "unmanaged.h"
#include "screens.h"
#include "workspace.h"
#if HAVE_INPUT
#include "libinput/connection.h"
#endif
#include "abstract_backend.h"
#include "shell_client.h"
#include "wayland_server.h"
#include "virtual_terminal.h"
#include <KWayland/Server/display.h>
#include <KWayland/Server/fakeinput_interface.h>
#include <KWayland/Server/seat_interface.h>
#include <decorations/decoratedclient.h>
#include <KDecoration2/Decoration>
// Qt
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTemporaryFile>
// KDE
#include <kkeyserver.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
// system
#include <linux/input.h>
#include <sys/mman.h>
#include <unistd.h>

namespace KWin
{

Xkb::Xkb(InputRedirection *input)
    : m_input(input)
    , m_context(xkb_context_new(static_cast<xkb_context_flags>(0)))
    , m_keymap(NULL)
    , m_state(NULL)
    , m_shiftModifier(0)
    , m_controlModifier(0)
    , m_altModifier(0)
    , m_metaModifier(0)
    , m_modifiers(Qt::NoModifier)
{
    if (!m_context) {
        qCDebug(KWIN_CORE) << "Could not create xkb context";
    } else {
        // load default keymap
        xkb_keymap *keymap = xkb_keymap_new_from_names(m_context, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
        if (keymap) {
            updateKeymap(keymap);
        } else {
            qCDebug(KWIN_CORE) << "Could not create default xkb keymap";
        }
    }
}

Xkb::~Xkb()
{
    xkb_state_unref(m_state);
    xkb_keymap_unref(m_keymap);
    xkb_context_unref(m_context);
}

void Xkb::installKeymap(int fd, uint32_t size)
{
    if (!m_context) {
        return;
    }
    char *map = reinterpret_cast<char*>(mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0));
    if (map == MAP_FAILED) {
        return;
    }
    xkb_keymap *keymap = xkb_keymap_new_from_string(m_context, map, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_MAP_COMPILE_PLACEHOLDER);
    munmap(map, size);
    if (!keymap) {
        qCDebug(KWIN_CORE) << "Could not map keymap from file";
        return;
    }
    updateKeymap(keymap);
}

void Xkb::updateKeymap(xkb_keymap *keymap)
{
    Q_ASSERT(keymap);
    xkb_state *state = xkb_state_new(keymap);
    if (!state) {
        qCDebug(KWIN_CORE) << "Could not create XKB state";
        xkb_keymap_unref(keymap);
        return;
    }
    // now release the old ones
    xkb_state_unref(m_state);
    xkb_keymap_unref(m_keymap);

    m_keymap = keymap;
    m_state = state;

    m_shiftModifier   = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_SHIFT);
    m_controlModifier = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_CTRL);
    m_altModifier     = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_ALT);
    m_metaModifier    = xkb_keymap_mod_get_index(m_keymap, XKB_MOD_NAME_LOGO);

    createKeymapFile();
}

void Xkb::createKeymapFile()
{
    if (!waylandServer()) {
        return;
    }
    // TODO: uninstall keymap on server?
    if (!m_keymap) {
        return;
    }

    ScopedCPointer<char> keymapString(xkb_keymap_get_as_string(m_keymap, XKB_KEYMAP_FORMAT_TEXT_V1));
    if (keymapString.isNull()) {
        return;
    }
    const uint size = qstrlen(keymapString.data()) + 1;

    QTemporaryFile *tmp = new QTemporaryFile(m_input);
    if (!tmp->open()) {
        delete tmp;
        return;
    }
    unlink(tmp->fileName().toUtf8().constData());
    if (!tmp->resize(size)) {
        delete tmp;
        return;
    }
    uchar *address = tmp->map(0, size);
    if (!address) {
        return;
    }
    if (qstrncpy(reinterpret_cast<char*>(address), keymapString.data(), size) == nullptr) {
        delete tmp;
        return;
    }
    waylandServer()->seat()->setKeymap(tmp->handle(), size);
}

void Xkb::updateModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    if (!m_keymap || !m_state) {
        return;
    }
    xkb_state_update_mask(m_state, modsDepressed, modsLatched, modsLocked, 0, 0, group);
    updateModifiers();
}

void Xkb::updateKey(uint32_t key, InputRedirection::KeyboardKeyState state)
{
    if (!m_keymap || !m_state) {
        return;
    }
    xkb_state_update_key(m_state, key + 8, static_cast<xkb_key_direction>(state));
    updateModifiers();
    if (state == InputRedirection::KeyboardKeyPressed) {
        m_modOnlyShortcut.pressCount++;
        if (m_modOnlyShortcut.pressCount == 1) {
            m_modOnlyShortcut.modifier = Qt::KeyboardModifier(int(m_modifiers));
        } else {
            m_modOnlyShortcut.modifier = Qt::NoModifier;
        }
    } else {
        m_modOnlyShortcut.pressCount--;
        // TODO: ignore on lock screen
        if (m_modOnlyShortcut.pressCount == 0) {
            if (m_modOnlyShortcut.modifier != Qt::NoModifier) {
                const auto list = options->modifierOnlyDBusShortcut(m_modOnlyShortcut.modifier);
                if (list.size() >= 4) {
                    auto call = QDBusMessage::createMethodCall(list.at(0), list.at(1), list.at(2), list.at(3));
                    QVariantList args;
                    for (int i = 4; i < list.size(); ++i) {
                        args << list.at(i);
                    }
                    call.setArguments(args);
                    QDBusConnection::sessionBus().asyncCall(call);
                }
            }
        }
        m_modOnlyShortcut.modifier = Qt::NoModifier;
    }
}

void Xkb::updateModifiers()
{
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    if (xkb_state_mod_index_is_active(m_state, m_shiftModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::ShiftModifier;
    }
    if (xkb_state_mod_index_is_active(m_state, m_altModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::AltModifier;
    }
    if (xkb_state_mod_index_is_active(m_state, m_controlModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::ControlModifier;
    }
    if (xkb_state_mod_index_is_active(m_state, m_metaModifier, XKB_STATE_MODS_EFFECTIVE) == 1) {
        mods |= Qt::MetaModifier;
    }
    m_modifiers = mods;
}

xkb_keysym_t Xkb::toKeysym(uint32_t key)
{
    if (!m_state) {
        return XKB_KEY_NoSymbol;
    }
    return xkb_state_key_get_one_sym(m_state, key + 8);
}

QString Xkb::toString(xkb_keysym_t keysym)
{
    if (!m_state || keysym == XKB_KEY_NoSymbol) {
        return QString();
    }
    QByteArray byteArray(7, 0);
    int ok = xkb_keysym_to_utf8(keysym, byteArray.data(), byteArray.size());
    if (ok == -1 || ok == 0) {
        return QString();
    }
    return QString::fromUtf8(byteArray.constData());
}

Qt::Key Xkb::toQtKey(xkb_keysym_t keysym)
{
    int key = Qt::Key_unknown;
    KKeyServer::symXToKeyQt(keysym, &key);
    return static_cast<Qt::Key>(key);
}

quint32 Xkb::getMods(quint32 components)
{
    if (!m_state) {
        return 0;
    }
    return xkb_state_serialize_mods(m_state, xkb_state_component(components));
}

quint32 Xkb::getGroup()
{
    if (!m_state) {
        return 0;
    }
    return xkb_state_serialize_layout(m_state, XKB_STATE_LAYOUT_EFFECTIVE);
}

KWIN_SINGLETON_FACTORY(InputRedirection)

InputRedirection::InputRedirection(QObject *parent)
    : QObject(parent)
    , m_xkb(new Xkb(this))
    , m_pointerWindow()
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
}

void InputRedirection::init()
{
    m_shortcuts->init();
}

void InputRedirection::setupWorkspace()
{
    if (waylandServer()) {
        connect(workspace(), &Workspace::clientActivated, this, &InputRedirection::updateKeyboardWindow);
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
                        processPointerMotion(globalPointer() + QPointF(delta.width(), delta.height()), 0);
                    }
                );
                connect(device, &FakeInputDevice::pointerButtonPressRequested, this,
                    [this] (quint32 button) {
                        // TODO: Fix time
                        processPointerButton(button, InputRedirection::PointerButtonPressed, 0);
                    }
                );
                connect(device, &FakeInputDevice::pointerButtonReleaseRequested, this,
                    [this] (quint32 button) {
                        // TODO: Fix time
                        processPointerButton(button, InputRedirection::PointerButtonReleased, 0);
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
                        processPointerAxis(axis, delta, 0);
                    }
                );
            }
        );
        connect(this, &InputRedirection::keyboardModifiersChanged, waylandServer(),
            [this] {
                if (!waylandServer()->seat()) {
                    return;
                }
                waylandServer()->seat()->updateKeyboardModifiers(m_xkb->getMods(XKB_STATE_MODS_DEPRESSED),
                                                                 m_xkb->getMods(XKB_STATE_MODS_LATCHED),
                                                                 m_xkb->getMods(XKB_STATE_MODS_LOCKED),
                                                                 m_xkb->getGroup());
            }
        );
        connect(workspace(), &Workspace::configChanged, this, &InputRedirection::reconfigure);
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
        m_pointerWarping = true;
        connect(conn, &LibInput::Connection::pointerButtonChanged, this, &InputRedirection::processPointerButton);
        connect(conn, &LibInput::Connection::pointerAxisChanged, this, &InputRedirection::processPointerAxis);
        connect(conn, &LibInput::Connection::keyChanged, this, &InputRedirection::processKeyboardKey);
        connect(conn, &LibInput::Connection::pointerMotion, this,
            [this] (QPointF delta, uint32_t time) {
                processPointerMotion(m_globalPointer + delta, time);
            }
        );
        connect(conn, &LibInput::Connection::pointerMotionAbsolute, this,
            [this] (QPointF orig, QPointF screen, uint32_t time) {
                Q_UNUSED(orig)
                processPointerMotion(screen, time);
            }
        );
        connect(conn, &LibInput::Connection::touchDown, this, &InputRedirection::processTouchDown);
        connect(conn, &LibInput::Connection::touchUp, this, &InputRedirection::processTouchUp);
        connect(conn, &LibInput::Connection::touchMotion, this, &InputRedirection::processTouchMotion);
        connect(conn, &LibInput::Connection::touchCanceled, this, &InputRedirection::cancelTouch);
        connect(conn, &LibInput::Connection::touchFrame, this, &InputRedirection::touchFrame);
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
    // set pos to center of all screens
    connect(screens(), &Screens::changed, this, &InputRedirection::updatePointerAfterScreenChange);
    m_globalPointer = screens()->geometry().center();
    emit globalPointerChanged(m_globalPointer);
    // sanitize
    updatePointerAfterScreenChange();
#endif
}

void InputRedirection::updatePointerWindow()
{
    // TODO: handle pointer grab aka popups
    Toplevel *t = findToplevel(m_globalPointer.toPoint());
    updatePointerInternalWindow();
    if (!m_pointerInternalWindow) {
        updatePointerDecoration(t);
    } else {
        m_pointerDecoration.clear();
    }
    if (m_pointerDecoration || m_pointerInternalWindow) {
        t = nullptr;
    }
    auto oldWindow = m_pointerWindow;
    if (!oldWindow.isNull() && t == m_pointerWindow.data()) {
        return;
    }
    if (auto seat = findSeat()) {
        // disconnect old surface
        if (oldWindow) {
            disconnect(oldWindow.data(), &Toplevel::geometryChanged, this, &InputRedirection::updateFocusedPointerPosition);
            if (AbstractBackend *b = waylandServer()->backend()) {
                if (auto p = seat->focusedPointer()) {
                    if (auto c = p->cursor()) {
                        disconnect(c, &KWayland::Server::Cursor::changed, b, &AbstractBackend::installCursorFromServer);
                    }
                }
            }
        }
        if (t && t->surface()) {
            seat->setFocusedPointerSurface(t->surface(), t->pos());
            connect(t, &Toplevel::geometryChanged, this, &InputRedirection::updateFocusedPointerPosition);
            if (AbstractBackend *b = waylandServer()->backend()) {
                b->installCursorFromServer();
                if (auto p = seat->focusedPointer()) {
                    if (auto c = p->cursor()) {
                        connect(c, &KWayland::Server::Cursor::changed, b, &AbstractBackend::installCursorFromServer);
                    }
                }
            }
        } else {
            seat->setFocusedPointerSurface(nullptr);
            t = nullptr;
        }
    }
    if (!t) {
        m_pointerWindow.clear();
        return;
    }
    m_pointerWindow = QWeakPointer<Toplevel>(t);
}

void InputRedirection::updatePointerDecoration(Toplevel *t)
{
    const auto oldDeco = m_pointerDecoration;
    if (Client *c = dynamic_cast<Client*>(t)) {
        // check whether it's on a Decoration
        if (c->decoratedClient()) {
            const QRect clientRect = QRect(c->clientPos(), c->clientSize()).translated(c->pos());
            if (!clientRect.contains(m_globalPointer.toPoint())) {
                m_pointerDecoration = c->decoratedClient();
            } else {
                m_pointerDecoration.clear();
            }
        } else {
            m_pointerDecoration.clear();
        }
    } else {
        m_pointerDecoration.clear();
    }

    if (oldDeco && oldDeco != m_pointerDecoration) {
        // send leave
        QHoverEvent event(QEvent::HoverLeave, QPointF(), QPointF());
        QCoreApplication::instance()->sendEvent(oldDeco->decoration(), &event);
        if (!m_pointerDecoration && waylandServer()) {
            waylandServer()->backend()->installCursorImage(Qt::ArrowCursor);
        }
    }
    if (m_pointerDecoration) {
        const QPointF p = m_globalPointer - t->pos();
        QHoverEvent event(QEvent::HoverMove, p, p);
        QCoreApplication::instance()->sendEvent(m_pointerDecoration->decoration(), &event);
        m_pointerDecoration->client()->processDecorationMove();
        installCursorFromDecoration();
    }
}

void InputRedirection::updatePointerInternalWindow()
{
    const auto oldInternalWindow = m_pointerInternalWindow;
    if (waylandServer()) {
        bool found = false;
        const auto &internalClients = waylandServer()->internalClients();
        const bool change = m_pointerInternalWindow.isNull() || !(m_pointerInternalWindow->flags().testFlag(Qt::Popup) && m_pointerInternalWindow->isVisible());
        if (!internalClients.isEmpty() && change) {
            auto it = internalClients.end();
            do {
                it--;
                if (QWindow *w = (*it)->internalWindow()) {
                    if (!w->isVisible()) {
                        continue;
                    }
                    if (w->geometry().contains(m_globalPointer.toPoint())) {
                        m_pointerInternalWindow = QPointer<QWindow>(w);
                        found = true;
                        break;
                    }
                }
            } while (it != internalClients.begin());
            if (!found) {
                m_pointerInternalWindow.clear();
            }
        }
    }
    if (oldInternalWindow != m_pointerInternalWindow) {
        // changed
        if (oldInternalWindow) {
            disconnect(oldInternalWindow.data(), &QWindow::visibleChanged, this, &InputRedirection::pointerInternalWindowVisibilityChanged);
            QEvent event(QEvent::Leave);
            QCoreApplication::sendEvent(oldInternalWindow.data(), &event);
        }
        if (m_pointerInternalWindow) {
            connect(oldInternalWindow.data(), &QWindow::visibleChanged, this, &InputRedirection::pointerInternalWindowVisibilityChanged);
            QEnterEvent event(m_globalPointer - m_pointerInternalWindow->position(),
                              m_globalPointer - m_pointerInternalWindow->position(),
                              m_globalPointer);
            QCoreApplication::sendEvent(m_pointerInternalWindow.data(), &event);
            return;
        }
    }
    if (m_pointerInternalWindow) {
        // send mouse move
        QMouseEvent event(QEvent::MouseMove,
                          m_globalPointer.toPoint() - m_pointerInternalWindow->position(),
                          m_globalPointer.toPoint(),
                          Qt::NoButton, qtButtonStates(), keyboardModifiers());
        QCoreApplication::sendEvent(m_pointerInternalWindow.data(), &event);
    }
}

void InputRedirection::pointerInternalWindowVisibilityChanged(bool visible)
{
    if (!visible) {
        updatePointerWindow();
    }
}

void InputRedirection::installCursorFromDecoration()
{
    if (waylandServer() && m_pointerDecoration) {
        waylandServer()->backend()->installCursorImage(m_pointerDecoration->client()->cursor());
    }
}

void InputRedirection::updateFocusedPointerPosition()
{
    if (m_pointerWindow.isNull()) {
        return;
    }
    if (workspace()->getMovingClient()) {
        // don't update while moving
        return;
    }
    if (auto seat = findSeat()) {
        if (m_pointerWindow.data()->surface() != seat->focusedPointerSurface()) {
            return;
        }
        seat->setFocusedPointerSurfacePosition(m_pointerWindow.data()->pos());
    }
}

void InputRedirection::updateFocusedTouchPosition()
{
    if (m_touchWindow.isNull()) {
        return;
    }
    if (auto seat = findSeat()) {
        if (m_touchWindow.data()->surface() != seat->focusedTouchSurface()) {
            return;
        }
        seat->setFocusedTouchSurfacePosition(m_touchWindow.data()->pos());
    }
}

void InputRedirection::processPointerMotion(const QPointF &pos, uint32_t time)
{
    if (!workspace()) {
        return;
    }
    // first update to new mouse position
//     const QPointF oldPos = m_globalPointer;
    updatePointerPosition(pos);

    // TODO: check which part of KWin would like to intercept the event
    QMouseEvent event(QEvent::MouseMove, m_globalPointer.toPoint(), m_globalPointer.toPoint(),
                      Qt::NoButton, qtButtonStates(), keyboardModifiers());
    // check whether an effect has a mouse grab
    if (effects && static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent(&event)) {
        // an effect grabbed the pointer, we do not forward the event to surfaces
        return;
    }
    if (AbstractClient *c = workspace()->getMovingClient()) {
        c->updateMoveResize(m_globalPointer);
    } else {
        QWeakPointer<Toplevel> old = m_pointerWindow;
        if (!areButtonsPressed()) {
            // update pointer window only if no button is pressed
            updatePointerWindow();
        } else if (m_pointerDecoration) {
            const QPointF p = m_globalPointer - m_pointerDecoration->client()->pos();
            QHoverEvent event(QEvent::HoverMove, p, p);
            QCoreApplication::instance()->sendEvent(m_pointerDecoration->decoration(), &event);
            m_pointerDecoration->client()->processDecorationMove();
        }
    }
    if (auto seat = findSeat()) {
        seat->setTimestamp(time);
        seat->setPointerPos(m_globalPointer);
    }
}

void InputRedirection::processPointerButton(uint32_t button, InputRedirection::PointerButtonState state, uint32_t time)
{
    m_pointerButtons[button] = state;
    emit pointerButtonStateChanged(button, state);

    QMouseEvent event(buttonStateToEvent(state), m_globalPointer.toPoint(), m_globalPointer.toPoint(),
                      buttonToQtMouseButton(button), qtButtonStates(), keyboardModifiers());
    // check whether an effect has a mouse grab
    if (effects && static_cast<EffectsHandlerImpl*>(effects)->checkInputWindowEvent(&event)) {
        // an effect grabbed the pointer, we do not forward the event to surfaces
        return;
    }
    if (AbstractClient *c = workspace()->getMovingClient()) {
        if (state == KWin::InputRedirection::PointerButtonReleased) {
            if (!areButtonsPressed()) {
                c->endMoveResize();
            }
        }
        return;
    }
    if (state == KWin::InputRedirection::PointerButtonPressed) {
        if (m_shortcuts->processPointerPressed(m_xkb->modifiers(), qtButtonStates())) {
            return;
        }
    }
    if (m_pointerInternalWindow) {
        // send mouse move
        QMouseEvent event(buttonStateToEvent(state),
                          m_globalPointer.toPoint() - m_pointerInternalWindow->position(),
                          m_globalPointer.toPoint(),
                          buttonToQtMouseButton(button), qtButtonStates(), keyboardModifiers());
        event.setAccepted(false);
        QCoreApplication::sendEvent(m_pointerInternalWindow.data(), &event);
    }
    if (m_pointerDecoration) {
        const QPoint localPos = m_globalPointer.toPoint() - m_pointerDecoration->client()->pos();
        QMouseEvent event(buttonStateToEvent(state),
                          localPos,
                          m_globalPointer.toPoint(),
                          buttonToQtMouseButton(button), qtButtonStates(), keyboardModifiers());
        event.setAccepted(false);
        QCoreApplication::sendEvent(m_pointerDecoration->decoration(), &event);
        if (!event.isAccepted()) {
            if (state == PointerButtonPressed) {
                m_pointerDecoration->client()->processDecorationButtonPress(&event);
            }
        }
        if (state == PointerButtonReleased) {
            m_pointerDecoration->client()->processDecorationButtonRelease(&event);
        }
        installCursorFromDecoration();
    }
    // TODO: check which part of KWin would like to intercept the event
    if (auto seat = findSeat()) {
        seat->setTimestamp(time);
        bool passThrough = true;
        if (state == PointerButtonPressed) {
            if (AbstractClient *c = dynamic_cast<AbstractClient*>(m_pointerWindow.data())) {
                bool wasAction = false;
                const Options::MouseCommand command = c->getMouseCommand(buttonToQtMouseButton(button), &wasAction);
                if (wasAction) {
                    passThrough = c->performMouseCommand(command, m_globalPointer.toPoint());
                }
            }
        }
        if (passThrough) {
            state == PointerButtonPressed ? seat->pointerButtonPressed(button) : seat->pointerButtonReleased(button);
        }
    }
    if (state == PointerButtonReleased && !areButtonsPressed()) {
        updatePointerWindow();
    }
}

void InputRedirection::processPointerAxis(InputRedirection::PointerAxis axis, qreal delta, uint32_t time)
{
    if (delta == 0) {
        return;
    }
    emit pointerAxisChanged(axis, delta);
    if (m_xkb->modifiers() != Qt::NoModifier) {
        PointerAxisDirection direction = PointerAxisUp;
        if (axis == PointerAxisHorizontal) {
            if (delta > 0) {
                direction = PointerAxisUp;
            } else {
                direction = PointerAxisDown;
            }
        } else {
            if (delta > 0) {
                direction = PointerAxisLeft;
            } else {
                direction = PointerAxisRight;
            }
        }
        if (m_shortcuts->processAxis(m_xkb->modifiers(), direction)) {
            return;
        }
    }

    auto sendWheelEvent = [this, delta, axis] (const QPoint targetPos, QObject *target) -> bool {
        const QPointF localPos = m_globalPointer - targetPos;
        // TODO: add modifiers and buttons
        QWheelEvent event(localPos, m_globalPointer, QPoint(),
                          (axis == PointerAxisHorizontal) ? QPoint(delta, 0) : QPoint(0, delta),
                          delta,
                          (axis == PointerAxisHorizontal) ? Qt::Horizontal : Qt::Vertical,
                          Qt::NoButton,
                          Qt::NoModifier);
        event.setAccepted(false);
        QCoreApplication::sendEvent(target, &event);
        return event.isAccepted();
    };
    if (m_pointerDecoration) {
        if (!sendWheelEvent(m_pointerDecoration->client()->pos(), m_pointerDecoration.data())
                && axis == PointerAxisVertical) {
            if (m_pointerDecoration->decoration()->titleBar().contains(m_globalPointer.toPoint() - m_pointerDecoration->client()->pos())) {
                m_pointerDecoration->client()->performMouseCommand(options->operationTitlebarMouseWheel(delta * -1),
                                                                   m_globalPointer.toPoint());
            }
        }
    }
    if (m_pointerInternalWindow) {
        sendWheelEvent(m_pointerInternalWindow->position(), m_pointerInternalWindow.data());
    }

    // TODO: check which part of KWin would like to intercept the event
    // TODO: Axis support for effect redirection
    if (auto seat = findSeat()) {
        seat->setTimestamp(time);
        seat->pointerAxis(axis == InputRedirection::PointerAxisHorizontal ? Qt::Horizontal : Qt::Vertical, delta);
    }
}

void InputRedirection::updateKeyboardWindow()
{
    if (!workspace()) {
        return;
    }
    if (auto seat = findSeat()) {
        // TODO: this needs better integration
        // check unmanaged
        Toplevel *t = nullptr;
        if (!workspace()->unmanagedList().isEmpty()) {
            // TODO: better check whether this unmanaged should get the key event
            t = workspace()->unmanagedList().first();
        }
        if (!t) {
            t = workspace()->activeClient();
        }
        if (t && t->surface()) {
            if (t->surface() != seat->focusedKeyboardSurface()) {
                seat->setFocusedKeyboardSurface(t->surface());
            }
        }
    }
}

void InputRedirection::processKeyboardKey(uint32_t key, InputRedirection::KeyboardKeyState state, uint32_t time)
{
    emit keyStateChanged(key, state);
    const Qt::KeyboardModifiers oldMods = keyboardModifiers();
    m_xkb->updateKey(key, state);
    if (oldMods != keyboardModifiers()) {
        emit keyboardModifiersChanged(keyboardModifiers(), oldMods);
    }
    // check for vt-switch
    if (VirtualTerminal::self()) {
        const xkb_keysym_t keysym = m_xkb->toKeysym(key);
        if (state == KWin::InputRedirection::KeyboardKeyPressed &&
                (keysym >= XKB_KEY_XF86Switch_VT_1 && keysym <= XKB_KEY_XF86Switch_VT_12)) {
            VirtualTerminal::self()->activate(keysym - XKB_KEY_XF86Switch_VT_1 + 1);
            return;
        }
    }
    // TODO: pass to internal parts of KWin
#ifdef KWIN_BUILD_TABBOX
    if (TabBox::TabBox::self() && TabBox::TabBox::self()->isGrabbed()) {
        if (state == KWin::InputRedirection::KeyboardKeyPressed) {
            TabBox::TabBox::self()->keyPress(m_xkb->modifiers() | m_xkb->toQtKey(m_xkb->toKeysym(key)));
        }
        return;
    }
#endif
    auto toKeyEvent = [&] {
        const xkb_keysym_t keysym = m_xkb->toKeysym(key);
        // TODO: start auto-repeat
        // TODO: add modifiers to the event
        const QEvent::Type type = (state == KeyboardKeyPressed) ? QEvent::KeyPress : QEvent::KeyRelease;
        QKeyEvent event(type, m_xkb->toQtKey(keysym), m_xkb->modifiers(), m_xkb->toString(keysym));
        return event;
    };
    if (effects && static_cast< EffectsHandlerImpl* >(effects)->hasKeyboardGrab()) {
        QKeyEvent event = toKeyEvent();
        static_cast< EffectsHandlerImpl* >(effects)->grabbedKeyboardEvent(&event);
        return;
    }
    if (workspace()) {
        if (AbstractClient *c = workspace()->getMovingClient()) {
            c->keyPressEvent(m_xkb->toQtKey(m_xkb->toKeysym(key)) | m_xkb->modifiers());
            if (c->isMove() || c->isResize()) {
                // only update if mode didn't end
                c->updateMoveResize(m_globalPointer);
            }
            return;
        }
        // TODO: Maybe it's better to select the top most visible internal window?
        if (m_pointerInternalWindow) {
            QKeyEvent event = toKeyEvent();
            event.setAccepted(false);
            QCoreApplication::sendEvent(m_pointerInternalWindow.data(), &event);
            return;
        }
    }
    // process global shortcuts
    if (state == KeyboardKeyPressed) {
        if (m_shortcuts->processKey(m_xkb->modifiers(), m_xkb->toKeysym(key))) {
            return;
        }
    }
    if (auto seat = findSeat()) {
        seat->setTimestamp(time);
        state == InputRedirection::KeyboardKeyPressed ? seat->keyPressed(key) : seat->keyReleased(key);
    }
}

void InputRedirection::processKeyboardModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    // TODO: send to proper Client and also send when active Client changes
    Qt::KeyboardModifiers oldMods = keyboardModifiers();
    m_xkb->updateModifiers(modsDepressed, modsLatched, modsLocked, group);
    if (oldMods != keyboardModifiers()) {
        emit keyboardModifiersChanged(keyboardModifiers(), oldMods);
    }
}

void InputRedirection::processKeymapChange(int fd, uint32_t size)
{
    // TODO: should we pass the keymap to our Clients? Or only to the currently active one and update
    m_xkb->installKeymap(fd, size);
}

void InputRedirection::processTouchDown(qint32 id, const QPointF &pos, quint32 time)
{
    // TODO: internal handling?
    if (auto seat = findSeat()) {
        seat->setTimestamp(time);
        if (!seat->isTouchSequence()) {
            updateTouchWindow(pos);
            if (AbstractClient *c = dynamic_cast<AbstractClient*>(m_touchWindow.data())) {
                // perform same handling as if it were a left click
                bool wasAction = false;
                const Options::MouseCommand command = c->getMouseCommand(Qt::LeftButton, &wasAction);
                if (wasAction) {
                    // if no replay we filter out this touch point
                    if (!c->performMouseCommand(command, pos.toPoint())) {
                        return;
                    }
                }
            }
        }
        m_touchIdMapper.insert(id, seat->touchDown(pos));
    }
}

void InputRedirection::updateTouchWindow(const QPointF &pos)
{
    // TODO: handle pointer grab aka popups
    Toplevel *t = findToplevel(pos.toPoint());
    auto oldWindow = m_touchWindow;
    if (!oldWindow.isNull() && t == oldWindow.data()) {
        return;
    }
    if (auto seat = findSeat()) {
        // disconnect old surface
        if (oldWindow) {
            disconnect(oldWindow.data(), &Toplevel::geometryChanged, this, &InputRedirection::updateFocusedTouchPosition);
        }
        if (t && t->surface()) {
            seat->setFocusedTouchSurface(t->surface(), t->pos());
            connect(t, &Toplevel::geometryChanged, this, &InputRedirection::updateFocusedTouchPosition);
        } else {
            seat->setFocusedTouchSurface(nullptr);
            t = nullptr;
        }
    }
    if (!t) {
        m_touchWindow.clear();
        return;
    }
    m_touchWindow = QWeakPointer<Toplevel>(t);
}


void InputRedirection::processTouchUp(qint32 id, quint32 time)
{
    // TODO: internal handling?
    if (auto seat = findSeat()) {
        auto it = m_touchIdMapper.constFind(id);
        if (it != m_touchIdMapper.constEnd()) {
            seat->setTimestamp(time);
            seat->touchUp(it.value());
        }
    }
}

void InputRedirection::processTouchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    // TODO: internal handling?
    if (auto seat = findSeat()) {
        seat->setTimestamp(time);
        auto it = m_touchIdMapper.constFind(id);
        if (it != m_touchIdMapper.constEnd()) {
            seat->setTimestamp(time);
            seat->touchMove(it.value(), pos);
        }
    }
}

void InputRedirection::cancelTouch()
{
    if (auto seat = findSeat()) {
        seat->cancelTouchSequence();
    }
}

void InputRedirection::touchFrame()
{
    if (auto seat = findSeat()) {
        seat->touchFrame();
    }
}

QEvent::Type InputRedirection::buttonStateToEvent(InputRedirection::PointerButtonState state)
{
    switch (state) {
    case KWin::InputRedirection::PointerButtonReleased:
        return QEvent::MouseButtonRelease;
    case KWin::InputRedirection::PointerButtonPressed:
        return QEvent::MouseButtonPress;
    }
    return QEvent::None;
}

Qt::MouseButton InputRedirection::buttonToQtMouseButton(uint32_t button)
{
    switch (button) {
    case BTN_LEFT:
        return Qt::LeftButton;
    case BTN_MIDDLE:
        return Qt::MiddleButton;
    case BTN_RIGHT:
        return Qt::RightButton;
    case BTN_BACK:
        return Qt::XButton1;
    case BTN_FORWARD:
        return Qt::XButton2;
    }
    return Qt::NoButton;
}

Qt::MouseButtons InputRedirection::qtButtonStates() const
{
    Qt::MouseButtons buttons;
    for (auto it = m_pointerButtons.constBegin(); it != m_pointerButtons.constEnd(); ++it) {
        if (it.value() == KWin::InputRedirection::PointerButtonReleased) {
            continue;
        }
        Qt::MouseButton button = buttonToQtMouseButton(it.key());
        if (button != Qt::NoButton) {
            buttons |= button;
        }
    }
    return buttons;
}

bool InputRedirection::areButtonsPressed() const
{
    for (auto it = m_pointerButtons.constBegin(); it != m_pointerButtons.constEnd(); ++it) {
        if (it.value() == KWin::InputRedirection::PointerButtonPressed) {
            return true;
        }
    }
    return false;
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
    // TODO: check whether the unmanaged wants input events at all
    const UnmanagedList &unmanaged = Workspace::self()->unmanagedList();
    foreach (Unmanaged *u, unmanaged) {
        if (u->geometry().contains(pos) && acceptsInput(u, pos)) {
            return u;
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
        if (t->geometry().contains(pos) && acceptsInput(t, pos)) {
            return t;
        }
    } while (it != stacking.begin());
    return NULL;
}

uint8_t InputRedirection::toXPointerButton(uint32_t button)
{
    switch (button) {
    case BTN_LEFT:
        return XCB_BUTTON_INDEX_1;
    case BTN_RIGHT:
        return XCB_BUTTON_INDEX_3;
    case BTN_MIDDLE:
        return XCB_BUTTON_INDEX_2;
    default:
        // TODO: add more buttons
        return XCB_BUTTON_INDEX_ANY;
    }
}

uint8_t InputRedirection::toXPointerButton(InputRedirection::PointerAxis axis, qreal delta)
{
    switch (axis) {
    case PointerAxisVertical:
        if (delta < 0) {
            return 4;
        } else {
            return 5;
        }
    case PointerAxisHorizontal:
        if (delta < 0) {
            return 6;
        } else {
            return 7;
        }
    }
    return XCB_BUTTON_INDEX_ANY;
}

Qt::KeyboardModifiers InputRedirection::keyboardModifiers() const
{
    return m_xkb->modifiers();
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

static bool screenContainsPos(const QPointF &pos)
{
    for (int i = 0; i < screens()->count(); ++i) {
        if (screens()->geometry(i).contains(pos.toPoint())) {
            return true;
        }
    }
    return false;
}

void InputRedirection::updatePointerPosition(const QPointF &pos)
{
    // verify that at least one screen contains the pointer position
    QPointF p = pos;
    if (!screenContainsPos(p)) {
        // allow either x or y to pass
        p = QPointF(m_globalPointer.x(), pos.y());
        if (!screenContainsPos(p)) {
            p = QPointF(pos.x(), m_globalPointer.y());
            if (!screenContainsPos(p)) {
                return;
            }
        }
    }
    m_globalPointer = p;
    emit globalPointerChanged(m_globalPointer);
}

void InputRedirection::updatePointerAfterScreenChange()
{
    if (screenContainsPos(m_globalPointer)) {
        // pointer still on a screen
        return;
    }
    // pointer no longer on a screen, reposition to closes screen
    m_globalPointer = screens()->geometry(screens()->number(m_globalPointer.toPoint())).center();
    emit globalPointerChanged(m_globalPointer);
}

void InputRedirection::warpPointer(const QPointF &pos)
{
    if (supportsPointerWarping()) {
        if (waylandServer()) {
            waylandServer()->backend()->warpPointer(pos);
        }
        updatePointerPosition(pos);
    }
}

bool InputRedirection::supportsPointerWarping() const
{
    if (waylandServer() && waylandServer()->backend()->supportsPointerWarping()) {
        return true;
    }
    return m_pointerWarping;
}

} // namespace
