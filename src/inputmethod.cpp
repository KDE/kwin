/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "inputmethod.h"

#include "config-kwin.h"

#include "effect/effecthandler.h"
#include "input.h"
#include "input_event.h"
#include "inputpanelv1window.h"
#include "keyboard_input.h"
#include "utils/common.h"
#include "virtualkeyboard_dbus.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#if KWIN_BUILD_SCREENLOCKER
#include <KScreenLocker/KsldApp>
#endif

#include "internalinputmethodcontext.h"
#include "pointer_input.h"
#include "tablet_input.h"
#include "touch_input.h"
#include "wayland/display.h"
#include "wayland/inputmethod_v1.h"
#include "wayland/keyboard.h"
#include "wayland/seat.h"
#include "wayland/surface.h"
#include "wayland/textinput_v1.h"
#include "wayland/textinput_v3.h"
#include "xkb.h"

#include <KLocalizedString>
#include <KShell>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QKeyEvent>
#include <QMenu>

#include <linux/input-event-codes.h>
#include <private/qxkbcommon_p.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon-keysyms.h>

namespace KWin
{

static std::vector<quint32> textToKey(const QString &text)
{
    if (text.isEmpty()) {
        return {};
    }

    auto sequence = QKeySequence::fromString(text);
    if (sequence.isEmpty()) {
        return {};
    }

    const QList<xkb_keysym_t> syms = KWin::Xkb::keysymsFromQtKey(sequence[0]);
    if (syms.empty()) {
        return {};
    }

    std::optional<xkb_keycode_t> keyCode;
    for (xkb_keysym_t sym : syms) {
        auto code = input()->keyboard()->xkb()->keycodeFromKeysym(sym);
        if (code) {
            keyCode = code->first;
            break;
        }
    }
    if (!keyCode) {
        return {};
    }

    if (text.isUpper()) {
        return {KEY_LEFTSHIFT, quint32(keyCode.value())};
    }

    return {quint32(keyCode.value())};
}

InputMethod::InputMethod()
{
    m_internalContext = new InternalInputMethodContext(this);

    m_enabled = kwinApp()->config()->group(QStringLiteral("Wayland")).readEntry("VirtualKeyboardEnabled", true);
    // this is actually too late. Other processes are started before init,
    // so might miss the availability of text input
    // but without Workspace we don't have the window listed at all
    if (workspace()) {
        init();
    } else {
        connect(kwinApp(), &Application::workspaceCreated, this, &InputMethod::init);
    }
}

InputMethod::~InputMethod()
{
    stopInputMethod();
}

void InputMethod::init()
{
    // Stop restarting the input method if it starts crashing very frequently
    m_inputMethodCrashTimer.setInterval(20000);
    m_inputMethodCrashTimer.setSingleShot(true);
    connect(&m_inputMethodCrashTimer, &QTimer::timeout, this, [this] {
        m_inputMethodCrashes = 0;
    });
#if KWIN_BUILD_SCREENLOCKER
    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::aboutToLock, this, &InputMethod::hide);
#endif

    new VirtualKeyboardDBus(this);
    qCDebug(KWIN_VIRTUALKEYBOARD) << "Registering the DBus interface";

    if (waylandServer()) {
        new TextInputManagerV1Interface(waylandServer()->display(), this);
        new TextInputManagerV2Interface(waylandServer()->display(), this);
        new TextInputManagerV3Interface(waylandServer()->display(), this);

        connect(waylandServer()->seat(), &SeatInterface::focusedKeyboardSurfaceAboutToChange, this, &InputMethod::commitPendingText);
        connect(waylandServer()->seat(), &SeatInterface::focusedTextInputSurfaceChanged, this, &InputMethod::handleFocusedSurfaceChanged);

        TextInputV1Interface *textInputV1 = waylandServer()->seat()->textInputV1();
        connect(textInputV1, &TextInputV1Interface::requestShowInputPanel, this, &InputMethod::show);
        connect(textInputV1, &TextInputV1Interface::requestHideInputPanel, this, &InputMethod::hide);
        connect(textInputV1, &TextInputV1Interface::surroundingTextChanged, this, &InputMethod::surroundingTextChanged);
        connect(textInputV1, &TextInputV1Interface::contentTypeChanged, this, &InputMethod::contentTypeChanged);
        connect(textInputV1, &TextInputV1Interface::stateUpdated, this, &InputMethod::textInputInterfaceV1StateUpdated);
        connect(textInputV1, &TextInputV1Interface::reset, this, &InputMethod::textInputInterfaceV1Reset);
        connect(textInputV1, &TextInputV1Interface::invokeAction, this, &InputMethod::invokeAction);
        connect(textInputV1, &TextInputV1Interface::enabledChanged, this, &InputMethod::textInputInterfaceV1EnabledChanged);

        TextInputV2Interface *textInputV2 = waylandServer()->seat()->textInputV2();
        connect(textInputV2, &TextInputV2Interface::requestShowInputPanel, this, &InputMethod::show);
        connect(textInputV2, &TextInputV2Interface::requestHideInputPanel, this, &InputMethod::hide);
        connect(textInputV2, &TextInputV2Interface::surroundingTextChanged, this, &InputMethod::surroundingTextChanged);
        connect(textInputV2, &TextInputV2Interface::contentTypeChanged, this, &InputMethod::contentTypeChanged);
        connect(textInputV2, &TextInputV2Interface::stateUpdated, this, &InputMethod::textInputInterfaceV2StateUpdated);
        connect(textInputV2, &TextInputV2Interface::enabledChanged, this, &InputMethod::textInputInterfaceV2EnabledChanged);

        TextInputV3Interface *textInputV3 = waylandServer()->seat()->textInputV3();
        connect(textInputV3, &TextInputV3Interface::surroundingTextChanged, this, &InputMethod::surroundingTextChanged);
        connect(textInputV3, &TextInputV3Interface::contentTypeChanged, this, &InputMethod::contentTypeChanged);
        connect(textInputV3, &TextInputV3Interface::stateCommitted, this, &InputMethod::stateCommitted);
        connect(textInputV3, &TextInputV3Interface::enabledChanged, this, &InputMethod::textInputInterfaceV3EnabledChanged);
        connect(textInputV3, &TextInputV3Interface::enableRequested, this, &InputMethod::textInputInterfaceV3EnableRequested);

        connect(m_internalContext, &InternalInputMethodContext::surroundingTextChanged, this, &InputMethod::surroundingTextChanged);
        connect(m_internalContext, &InternalInputMethodContext::contentTypeChanged, this, &InputMethod::contentTypeChanged);
        connect(m_internalContext, &InternalInputMethodContext::enabledChanged, this, &InputMethod::refreshActive);
        connect(m_internalContext, &InternalInputMethodContext::showInputPanelRequested, this, &InputMethod::show);
        connect(m_internalContext, &InternalInputMethodContext::hideInputPanelRequested, this, &InputMethod::hide);

        connect(input()->keyboard()->xkb(), &Xkb::modifierStateChanged, this, [this]() {
            m_hasPendingModifiers = true;
        });
    }
}

void InputMethod::show()
{
    m_shouldShowPanel = true;

    // If the panel has something to display, show it (e.g. if we hid it from kwin rather than the IM hiding itself)
    // Otherwise, ensure the input context is current and the IM will see to having itself shown if there's something to show.
    // If there's no context available, then nothing will be shown
    if (m_panel && !m_panel->wasUnmapped()) {
        m_panel->show();
        updateInputPanelState();
    } else {
        // making the input context current will trigger the IM
        // to show the panel (if there is something to show)
        if (!isActive()) {
            refreshActive();
        }

        // refreshActive affects the result of isActive
        if (isActive()) {
            adoptInputMethodContext();
        }
    }
}

void InputMethod::hide()
{
    m_shouldShowPanel = false;
    if (m_panel) {
        m_panel->hide();
        updateInputPanelState();
    }
}

bool InputMethod::shouldShowOnActive() const
{
    static bool alwaysShowIm = qEnvironmentVariableIntValue("KWIN_IM_SHOW_ALWAYS") != 0;
    return alwaysShowIm || input()->touch() == input()->lastInputHandler()
        || input()->tablet() == input()->lastInputHandler();
}

void InputMethod::refreshActive()
{
    auto seat = waylandServer()->seat();
    auto t1 = seat->textInputV1();
    auto t2 = seat->textInputV2();
    auto t3 = seat->textInputV3();

    bool active = false;
    if (auto focusedSurface = seat->focusedTextInputSurface()) {
        auto client = focusedSurface->client();
        if ((t1->clientSupportsTextInput(client) && t1->isEnabled()) || (t2->clientSupportsTextInput(client) && t2->isEnabled()) || (t3->clientSupportsTextInput(client) && t3->isEnabled())) {
            active = true;
        }
    }
    if (m_internalContext->isEnabled()) {
        active = true;
    }
    setActive(active);
}

void InputMethod::forwardKeyToEffects(bool pressed, int keyCode, int keySym)
{
    if (!input()->keyboard()) {
        return;
    }
    auto xkb = input()->keyboard()->xkb();

    QKeyEvent event(pressed ? QEvent::KeyPress : QEvent::KeyRelease,
                    xkb->toQtKey(keySym, keyCode, Qt::KeyboardModifiers()),
                    xkb->modifiers(),
                    keyCode,
                    keySym,
                    0,
                    xkb->toString(keySym));
    waylandServer()->seat()->setFocusedKeyboardSurface(nullptr, waylandServer()->seat()->nextSerial());
    effects->grabbedKeyboardEvent(&event);
}

void InputMethod::commitPendingText()
{
    if (!m_pendingText.isEmpty()) {
        commitString(m_serial++, m_pendingText);
        m_pendingText = QString();
        auto imContext = waylandServer()->inputMethod()->context();
        if (imContext) {
            imContext->sendReset();
        }
    }
}

void InputMethod::setActive(bool active)
{
    const bool wasActive = waylandServer()->inputMethod()->context();
    if (wasActive && !active) {
        waylandServer()->inputMethod()->sendDeactivate();
    }

    if (active) {
        if (!m_enabled) {
            return;
        }

        if (!wasActive) {
            waylandServer()->inputMethod()->sendActivate();
        }
        adoptInputMethodContext();
    } else {
        updateInputPanelState();
    }

    if (wasActive != isActive()) {
        Q_EMIT activeChanged(active);
    }
}

InputPanelV1Window *InputMethod::panel() const
{
    return m_panel;
}

void InputMethod::setPanel(InputPanelV1Window *panel)
{
    Q_ASSERT(panel->isInputMethod());
    if (m_panel) {
        qCWarning(KWIN_VIRTUALKEYBOARD) << "Replacing input panel" << m_panel << "with" << panel;
        m_panel->destroyWindow();
    }

    m_panel = panel;
    connect(m_panel, &Window::closed, this, [this]() {
        updateInputPanelState();
        Q_EMIT visibleChanged();
    });
    connect(m_panel, &Window::frameGeometryChanged, this, &InputMethod::updateInputPanelState);
    connect(m_panel, &Window::hiddenChanged, this, &InputMethod::updateInputPanelState);
    connect(m_panel, &Window::hiddenChanged, this, &InputMethod::visibleChanged);
    connect(m_panel, &Window::readyForPaintingChanged, this, &InputMethod::visibleChanged);
    Q_EMIT visibleChanged();
    updateInputPanelState();
    Q_EMIT panelChanged();

    if (m_shouldShowPanel) {
        show();
    }
}

void InputMethod::setTrackedWindow(Window *trackedWindow)
{
    // Reset the old window virtual keybaord geom if necessary
    // Old and new windows could be the same if focus moves between subsurfaces
    if (m_trackedWindow == trackedWindow) {
        return;
    }
    if (m_trackedWindow) {
        m_trackedWindow->setVirtualKeyboardGeometry(QRect());
        disconnect(m_trackedWindow, &Window::frameGeometryChanged, this, &InputMethod::updateInputPanelState);
    }
    m_trackedWindow = trackedWindow;
    m_shouldShowPanel = false;
    if (m_trackedWindow) {
        connect(m_trackedWindow, &Window::frameGeometryChanged, this, &InputMethod::updateInputPanelState, Qt::QueuedConnection);
    }
    updateInputPanelState();
}

void InputMethod::handleFocusedSurfaceChanged()
{
    resetPendingPreedit();
    m_pendingText = QString();

    auto seat = waylandServer()->seat();
    SurfaceInterface *focusedSurface = seat->focusedTextInputSurface();

    setTrackedWindow(waylandServer()->findWindow(focusedSurface));

    const auto client = focusedSurface ? focusedSurface->client() : nullptr;
    bool ret = seat->textInputV2()->clientSupportsTextInput(client)
            || seat->textInputV3()->clientSupportsTextInput(client)
            || m_internalContext->isEnabled();

    if (ret != m_activeClientSupportsTextInput) {
        m_activeClientSupportsTextInput = ret;
        Q_EMIT activeClientSupportsTextInputChanged();
    }
}

void InputMethod::surroundingTextChanged()
{
    auto t2 = waylandServer()->seat()->textInputV2();
    auto t3 = waylandServer()->seat()->textInputV3();
    auto inputContext = waylandServer()->inputMethod()->context();
    if (!inputContext) {
        return;
    }
    if (t2 && t2->isEnabled()) {
        inputContext->sendSurroundingText(t2->surroundingText(), t2->surroundingTextCursorPosition(), t2->surroundingTextSelectionAnchor());
        return;
    }
    if (t3 && t3->isEnabled()) {
        inputContext->sendSurroundingText(t3->surroundingText(), t3->surroundingTextCursorPosition(), t3->surroundingTextSelectionAnchor());
        return;
    }
    if (m_internalContext->isEnabled()) {
        inputContext->sendSurroundingText(m_internalContext->surroundingText(), m_internalContext->cursorPosition(), m_internalContext->anchorPosition());
        return;
    }
}

void InputMethod::contentTypeChanged()
{
    auto t1 = waylandServer()->seat()->textInputV1();
    auto t2 = waylandServer()->seat()->textInputV2();
    auto t3 = waylandServer()->seat()->textInputV3();
    auto inputContext = waylandServer()->inputMethod()->context();
    if (!inputContext) {
        return;
    }
    if (t1 && t1->isEnabled()) {
        inputContext->sendContentType(t1->contentHints(), t1->contentPurpose());
    }
    if (t2 && t2->isEnabled()) {
        inputContext->sendContentType(t2->contentHints(), t2->contentPurpose());
    }
    if (t3 && t3->isEnabled()) {
        inputContext->sendContentType(t3->contentHints(), t3->contentPurpose());
    }
}

void InputMethod::textInputInterfaceV1Reset()
{
    if (!m_enabled) {
        return;
    }
    auto t1 = waylandServer()->seat()->textInputV1();
    auto inputContext = waylandServer()->inputMethod()->context();
    if (!inputContext) {
        return;
    }
    if (!t1 || !t1->isEnabled()) {
        return;
    }
    inputContext->sendReset();
}

void InputMethod::invokeAction(quint32 button, quint32 index)
{
    if (!m_enabled) {
        return;
    }
    auto t1 = waylandServer()->seat()->textInputV1();
    auto inputContext = waylandServer()->inputMethod()->context();
    if (!inputContext) {
        return;
    }
    if (!t1 || !t1->isEnabled()) {
        return;
    }
    inputContext->sendInvokeAction(button, index);
}

void InputMethod::textInputInterfaceV1StateUpdated(quint32 serial)
{
    if (!m_enabled) {
        return;
    }
    auto t1 = waylandServer()->seat()->textInputV1();
    auto inputContext = waylandServer()->inputMethod()->context();
    if (!inputContext) {
        return;
    }
    if (!t1 || !t1->isEnabled()) {
        return;
    }
    inputContext->sendCommitState(serial);
}

void InputMethod::textInputInterfaceV2StateUpdated(quint32 serial, TextInputV2Interface::UpdateReason reason)
{
    if (!m_enabled) {
        return;
    }

    auto t2 = waylandServer()->seat()->textInputV2();
    auto inputContext = waylandServer()->inputMethod()->context();
    if (!inputContext) {
        return;
    }
    if (!t2 || !t2->isEnabled()) {
        return;
    }
    if (m_panel && shouldShowOnActive()) {
        m_panel->allow();
    }
    switch (reason) {
    case TextInputV2Interface::UpdateReason::StateChange:
        break;
    case TextInputV2Interface::UpdateReason::StateEnter:
    case TextInputV2Interface::UpdateReason::StateFull:
        adoptInputMethodContext();
        break;
    case TextInputV2Interface::UpdateReason::StateReset:
        inputContext->sendReset();
        break;
    }
}

void InputMethod::textInputInterfaceV1EnabledChanged()
{
    if (!m_enabled) {
        return;
    }

    refreshActive();
}

void InputMethod::textInputInterfaceV2EnabledChanged()
{
    if (!m_enabled) {
        return;
    }

    refreshActive();
}

void InputMethod::textInputInterfaceV3EnabledChanged()
{
    if (!m_enabled) {
        return;
    }

    auto t3 = waylandServer()->seat()->textInputV3();
    refreshActive();
    if (t3->isEnabled()) {
        show();
    } else {
        // reset value of preedit when textinput is disabled
        resetPendingPreedit();
        m_pendingText = QString();
    }
    auto context = waylandServer()->inputMethod()->context();
    if (context) {
        context->sendReset();
        adoptInputMethodContext();
    }
}

void InputMethod::stateCommitted(uint32_t serial)
{
    if (!isEnabled()) {
        return;
    }
    TextInputV3Interface *textInputV3 = waylandServer()->seat()->textInputV3();
    if (!textInputV3) {
        return;
    }

    if (auto inputContext = waylandServer()->inputMethod()->context()) {
        inputContext->sendCommitState(serial);
    }
}

void InputMethod::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    Q_EMIT enabledChanged(m_enabled);

    // send OSD message
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/org/kde/osdService"),
        QStringLiteral("org.kde.osdService"),
        QStringLiteral("virtualKeyboardEnabledChanged"));
    msg.setArguments({enabled});
    QDBusConnection::sessionBus().asyncCall(msg);
    if (!m_enabled) {
        hide();
        stopInputMethod();
    } else {
        startInputMethod();
    }
    // save value into config
    kwinApp()->config()->group(QStringLiteral("Wayland")).writeEntry("VirtualKeyboardEnabled", m_enabled);
    kwinApp()->config()->sync();
}

static quint32 keysymToKeycode(quint32 sym)
{
    switch (sym) {
    case XKB_KEY_BackSpace:
        return KEY_BACKSPACE;
    case XKB_KEY_Return:
        return KEY_ENTER;
    case XKB_KEY_Left:
        return KEY_LEFT;
    case XKB_KEY_Right:
        return KEY_RIGHT;
    case XKB_KEY_Up:
        return KEY_UP;
    case XKB_KEY_Down:
        return KEY_DOWN;
    default:
        return KEY_UNKNOWN;
    }
}

void InputMethod::keysymReceived(quint32 serial, quint32 time, quint32 sym, bool pressed, quint32 modifiers)
{
    if (auto t1 = waylandServer()->seat()->textInputV1(); t1 && t1->isEnabled()) {
        if (pressed) {
            t1->keysymPressed(time, sym, modifiers);
        } else {
            t1->keysymReleased(time, sym, modifiers);
        }
        return;
    }

    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        if (pressed) {
            t2->keysymPressed(sym, modifiers);
        } else {
            t2->keysymReleased(sym, modifiers);
        }
        return;
    }

    if (effects && effects->hasKeyboardGrab()) {
        const int keyCode = keysymToKeycode(sym);
        forwardKeyToEffects(pressed, keyCode, sym);
        return;
    }

    KeyboardKeyState state;
    if (pressed) {
        state = KeyboardKeyState::Pressed;
    } else {
        state = KeyboardKeyState::Released;
    }
    waylandServer()->seat()->notifyKeyboardKey(keysymToKeycode(sym), state, waylandServer()->seat()->nextSerial()); // TODO: use serial?
}

void InputMethod::commitString(qint32 serial, const QString &text)
{
    if (auto t1 = waylandServer()->seat()->textInputV1(); t1 && t1->isEnabled()) {
        t1->commitString(text);
        t1->setPreEditCursor(0);
        t1->preEdit({}, {});
        return;
    }
    if (auto t2 = waylandServer()->seat()->textInputV2(); t2 && t2->isEnabled()) {
        t2->commitString(text);
        t2->setPreEditCursor(0);
        t2->preEdit({}, {});
        return;
    } else if (auto t3 = waylandServer()->seat()->textInputV3(); t3 && t3->isEnabled()) {
        t3->sendPreEditString(QString(), 0, 0);
        t3->commitString(text);
        t3->done();
        return;
    } else if (m_internalContext->isEnabled()) {
        m_internalContext->handlePreeditText({}, 0, 0);
        m_internalContext->handleCommitString(text);
    } else {
        // The application has no way of communicating with the input method.
        // So instead, try to convert what we get from the input method into
        // keycodes and send those as fake input to the client.
        auto keys = textToKey(text);
        if (keys.empty()) {
            return;
        }

        // First, send all the extracted keys as pressed keys to the client.
        for (const auto &key : keys) {
            waylandServer()->seat()->notifyKeyboardKey(key, KeyboardKeyState::Pressed, waylandServer()->seat()->nextSerial()); // TODO: use serial?
        }

        // Then, send key release for those keys in reverse.
        for (auto itr = keys.rbegin(); itr != keys.rend(); ++itr) {
            // Since we are faking key events, we do not have distinct press/release
            // events. So instead, just queue the button release so it gets sent
            // a few moments after the press.
            auto key = *itr;
            QMetaObject::invokeMethod(
                this, [key]() {
                waylandServer()->seat()->notifyKeyboardKey(key, KeyboardKeyState::Released, waylandServer()->seat()->nextSerial()); // TODO: use serial?
            }, Qt::QueuedConnection);
        }
    }
}

void InputMethod::deleteSurroundingText(int32_t index, uint32_t length)
{
    // zwp_input_method_v1 Delete surrounding text interface is designed for text-input-v1.
    // The parameter has different meaning in text-input-v{2,3}.
    // Current cursor is at index 0.
    // The actualy deleted text range is [index, index + length].
    // In v{2,3}'s before/after style, text to be deleted with v{2,3} interface is [-before, after].
    // And before/after are all unsigned, which make it impossible to do certain things.
    // Those request will be ignored.

    // Verify we can handle such request.
    if (index > 0 || index + static_cast<ssize_t>(length) < 0) {
        return;
    }
    const quint32 before = -index;
    const quint32 after = index + length;

    auto t1 = waylandServer()->seat()->textInputV1();
    if (t1 && t1->isEnabled()) {
        t1->deleteSurroundingText(before, after);
    }
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->deleteSurroundingText(before, after);
    }
    auto t3 = waylandServer()->seat()->textInputV3();
    if (t3 && t3->isEnabled()) {
        t3->deleteSurroundingText(before, after);
        t3->done();
    }
    if (internalContext()->isEnabled()) {
        internalContext()->handleDeleteSurroundingText(before, after);
    }
}

void InputMethod::setCursorPosition(qint32 index, qint32 anchor)
{
    auto t1 = waylandServer()->seat()->textInputV1();
    if (t1 && t1->isEnabled()) {
        t1->setCursorPosition(index, anchor);
    }
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->setCursorPosition(index, anchor);
    }
}

void InputMethod::setLanguage(uint32_t serial, const QString &language)
{
    auto t1 = waylandServer()->seat()->textInputV1();
    if (t1 && t1->isEnabled()) {
        t1->setLanguage(language);
    }
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->setLanguage(language);
    }
}

void InputMethod::setTextDirection(uint32_t serial, Qt::LayoutDirection direction)
{
    auto t1 = waylandServer()->seat()->textInputV1();
    if (t1 && t1->isEnabled()) {
        t1->setTextDirection(direction);
    }
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->setTextDirection(direction);
    }
}

void InputMethod::setPreeditCursor(qint32 index)
{
    auto t1 = waylandServer()->seat()->textInputV1();
    if (t1 && t1->isEnabled()) {
        t1->setPreEditCursor(index);
    }
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->setPreEditCursor(index);
    }
    auto t3 = waylandServer()->seat()->textInputV3();
    if (t3 && t3->isEnabled()) {
        preedit.cursor = index;
    }
}

void InputMethod::setPreeditStyling(quint32 index, quint32 length, quint32 style)
{
    auto t1 = waylandServer()->seat()->textInputV1();
    if (t1 && t1->isEnabled()) {
        t1->preEditStyling(index, length, style);
    }
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->preEditStyling(index, length, style);
    }
    auto t3 = waylandServer()->seat()->textInputV3();
    if (t3 && t3->isEnabled()) {
        // preedit style: highlight(4) or selection(6)
        if (style == 4 || style == 6) {
            preedit.highlightRanges.emplace_back(index, index + length);
        }
    }
}

void InputMethod::setPreeditString(uint32_t serial, const QString &text, const QString &commit)
{
    auto t1 = waylandServer()->seat()->textInputV1();
    if (t1 && t1->isEnabled()) {
        t1->preEdit(text, commit);
    }
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->preEdit(text, commit);
    }
    auto t3 = waylandServer()->seat()->textInputV3();
    if (t3 && t3->isEnabled()) {
        m_pendingText = commit;
        if (!text.isEmpty()) {
            quint32 cursor = 0, cursorEnd = 0;
            if (preedit.cursor > 0) {
                cursor = cursorEnd = preedit.cursor;
            }
            // Check if we can convert highlight style to a range of selection.
            if (!preedit.highlightRanges.empty()) {
                std::sort(preedit.highlightRanges.begin(), preedit.highlightRanges.end());
                // Check if starting point matches.
                if (preedit.highlightRanges.front().first == cursor) {
                    quint32 end = preedit.highlightRanges.front().second;
                    bool nonContinousHighlight = false;
                    for (size_t i = 1; i < preedit.highlightRanges.size(); i++) {
                        if (end >= preedit.highlightRanges[i].first) {
                            end = std::max(end, preedit.highlightRanges[i].second);
                        } else {
                            nonContinousHighlight = true;
                            break;
                        }
                    }
                    if (!nonContinousHighlight) {
                        cursorEnd = end;
                    }
                }
            }
            t3->sendPreEditString(text, cursor, cursorEnd);
        } else {
            t3->sendPreEditString(text, 0, 0);
        }
        t3->done();
    }
    if (m_internalContext->isEnabled()) {
        m_internalContext->handlePreeditText(text, preedit.cursor, preedit.cursor);
    }
    resetPendingPreedit();
}

void InputMethod::key(quint32 /*serial*/, quint32 time, quint32 keyCode, bool pressed)
{
    if (!input()->keyboard()) {
        return;
    }
    if (effects && effects->hasKeyboardGrab()) {
        Xkb *xkb = input()->keyboard()->xkb();
        const xkb_keysym_t keySym = xkb->toKeysym(keyCode);
        forwardKeyToEffects(pressed, keyCode, keySym);
        return;
    }

    waylandServer()->seat()->notifyKeyboardKey(keyCode,
                                               pressed ? KeyboardKeyState::Pressed : KeyboardKeyState::Released, waylandServer()->seat()->nextSerial()); // TODO: use serial?
}

void InputMethod::modifiers(quint32 serial, quint32 mods_depressed, quint32 mods_latched, quint32 mods_locked, quint32 group)
{
    auto xkb = input()->keyboard()->xkb();
    xkb->updateModifiers(mods_depressed, mods_latched, mods_locked, group);
}

void InputMethod::forwardModifiers(ForwardModifiersForce force)
{
    const bool sendModifiers = m_hasPendingModifiers || force == Force;
    m_hasPendingModifiers = false;
    if (!sendModifiers) {
        return;
    }
    auto xkb = input()->keyboard()->xkb();
    if (m_keyboardGrab) {
        m_keyboardGrab->sendModifiers(waylandServer()->seat()->nextSerial(),
                                      xkb->modifierState().depressed,
                                      xkb->modifierState().latched,
                                      xkb->modifierState().locked,
                                      xkb->currentLayout());
    }
}

void InputMethod::adoptInputMethodContext()
{
    auto inputContext = waylandServer()->inputMethod()->context();

    TextInputV1Interface *t1 = waylandServer()->seat()->textInputV1();
    TextInputV2Interface *t2 = waylandServer()->seat()->textInputV2();
    TextInputV3Interface *t3 = waylandServer()->seat()->textInputV3();

    if (t1 && t1->isEnabled()) {
        inputContext->sendSurroundingText(t1->surroundingText(), t1->surroundingTextCursorPosition(), t1->surroundingTextSelectionAnchor());
        inputContext->sendPreferredLanguage(t1->preferredLanguage());
        inputContext->sendContentType(t1->contentHints(), t2->contentPurpose());
        connect(inputContext, &InputMethodContextV1Interface::language, this, &InputMethod::setLanguage);
        connect(inputContext, &InputMethodContextV1Interface::textDirection, this, &InputMethod::setTextDirection);
    } else if (t2 && t2->isEnabled()) {
        inputContext->sendSurroundingText(t2->surroundingText(), t2->surroundingTextCursorPosition(), t2->surroundingTextSelectionAnchor());
        inputContext->sendPreferredLanguage(t2->preferredLanguage());
        inputContext->sendContentType(t2->contentHints(), t2->contentPurpose());
        connect(inputContext, &InputMethodContextV1Interface::language, this, &InputMethod::setLanguage);
        connect(inputContext, &InputMethodContextV1Interface::textDirection, this, &InputMethod::setTextDirection);
    } else if (t3 && t3->isEnabled()) {
        inputContext->sendSurroundingText(t3->surroundingText(), t3->surroundingTextCursorPosition(), t3->surroundingTextSelectionAnchor());
        inputContext->sendContentType(t3->contentHints(), t3->contentPurpose());
    } else if (m_internalContext->isEnabled()) {
        inputContext->sendSurroundingText(m_internalContext->surroundingText(), m_internalContext->cursorPosition(), m_internalContext->anchorPosition());
        inputContext->sendContentType(TextInputContentHint::Latin, TextInputContentPurpose::Normal);
    } else {
        // When we have neither text-input-v2 nor text-input-v3 we can only send
        // fake key events, not more complex text. So ask the input method to
        // only send basic characters without any pre-editing.
        inputContext->sendContentType(TextInputContentHint::Latin, TextInputContentPurpose::Normal);
    }

    inputContext->sendCommitState(m_serial++);

    connect(inputContext, &InputMethodContextV1Interface::keysym, this, &InputMethod::keysymReceived, Qt::UniqueConnection);
    connect(inputContext, &InputMethodContextV1Interface::key, this, &InputMethod::key, Qt::UniqueConnection);
    connect(inputContext, &InputMethodContextV1Interface::modifiers, this, &InputMethod::modifiers, Qt::UniqueConnection);
    connect(inputContext, &InputMethodContextV1Interface::commitString, this, &InputMethod::commitString, Qt::UniqueConnection);
    connect(inputContext, &InputMethodContextV1Interface::deleteSurroundingText, this, &InputMethod::deleteSurroundingText, Qt::UniqueConnection);
    connect(inputContext, &InputMethodContextV1Interface::cursorPosition, this, &InputMethod::setCursorPosition, Qt::UniqueConnection);
    connect(inputContext, &InputMethodContextV1Interface::preeditStyling, this, &InputMethod::setPreeditStyling, Qt::UniqueConnection);
    connect(inputContext, &InputMethodContextV1Interface::preeditString, this, &InputMethod::setPreeditString, Qt::UniqueConnection);
    connect(inputContext, &InputMethodContextV1Interface::preeditCursor, this, &InputMethod::setPreeditCursor, Qt::UniqueConnection);
    connect(inputContext, &InputMethodContextV1Interface::keyboardGrabRequested, this, &InputMethod::installKeyboardGrab, Qt::UniqueConnection);
    connect(inputContext, &InputMethodContextV1Interface::modifiersMap, this, &InputMethod::updateModifiersMap, Qt::UniqueConnection);
}

void InputMethod::updateInputPanelState()
{
    if (!waylandServer()) {
        return;
    }

    if (m_panel && shouldShowOnActive()) {
        m_panel->allow();
    }

    QRectF overlap = QRectF(0, 0, 0, 0);
    if (m_trackedWindow) {
        const bool bottomKeyboard = m_panel && m_panel->mode() != InputPanelV1Window::Mode::Overlay && m_panel->isShown();
        m_trackedWindow->setVirtualKeyboardGeometry(bottomKeyboard ? m_panel->frameGeometry() : QRectF());

        if (m_panel && m_panel->mode() != InputPanelV1Window::Mode::Overlay) {
            overlap = m_trackedWindow->frameGeometry() & m_panel->frameGeometry();
            overlap.moveTo(m_trackedWindow->mapToLocal(overlap.topLeft()));
        }
    }

    auto t = waylandServer()->seat()->textInputV2();
    if (!t) {
        return;
    }
    t->setInputPanelState(m_panel && m_panel->isShown(), overlap.toRect());
}

void InputMethod::setInputMethodCommand(const QString &command)
{
    if (m_inputMethodCommand == command) {
        return;
    }

    m_inputMethodCommand = command;

    if (m_enabled) {
        startInputMethod();
    }
    Q_EMIT availableChanged();
}

void InputMethod::stopInputMethod()
{
    if (!m_inputMethodProcess) {
        return;
    }
    disconnect(m_inputMethodProcess, nullptr, this, nullptr);

    m_inputMethodProcess->terminate();
    if (!m_inputMethodProcess->waitForFinished()) {
        m_inputMethodProcess->kill();
        m_inputMethodProcess->waitForFinished();
    }
    m_inputMethodProcess->deleteLater();
    m_inputMethodProcess = nullptr;

    waylandServer()->destroyInputMethodConnection();
}

void InputMethod::startInputMethod()
{
    stopInputMethod();
    if (m_inputMethodCommand.isEmpty() || kwinApp()->isTerminating()) {
        return;
    }

    QStringList arguments = KShell::splitArgs(m_inputMethodCommand);
    if (arguments.isEmpty()) {
        qWarning("Failed to launch the input method server: %s is an invalid command", qPrintable(m_inputMethodCommand));
        return;
    }

    const QString program = arguments.takeFirst();
    int socket = waylandServer()->createInputMethodConnection();
    if (socket < 0) {
        qWarning("Failed to create the input method connection");
        return;
    }
    socket = dup(socket);

    QProcessEnvironment environment = kwinApp()->processStartupEnvironment();
    environment.insert(QStringLiteral("WAYLAND_SOCKET"), QString::number(socket));
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    // When we use Maliit as virtual keyboard, we want KWin to handle the animation
    // since that works a lot better. So we need to tell Maliit to not do client side
    // animation.
    environment.insert(QStringLiteral("MALIIT_ENABLE_ANIMATIONS"), QStringLiteral("0"));

    if (qEnvironmentVariableIntValue("KWIN_IM_WAYLAND_DEBUG") == 1) {
        environment.insert("WAYLAND_DEBUG", QByteArrayLiteral("1"));
    }

    m_inputMethodProcess = new QProcess(this);
    m_inputMethodProcess->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    m_inputMethodProcess->setProcessEnvironment(environment);
    m_inputMethodProcess->setProgram(program);
    m_inputMethodProcess->setArguments(arguments);
    m_inputMethodProcess->start();
    close(socket);
    connect(m_inputMethodProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus == QProcess::CrashExit) {
            m_inputMethodCrashes++;
            m_inputMethodCrashTimer.start();
            qWarning() << "Input Method crashed" << m_inputMethodProcess->program() << m_inputMethodProcess->arguments() << exitCode << exitStatus;
            if (m_inputMethodCrashes < 5) {
                startInputMethod();
            } else {
                qWarning() << "Input Method keeps crashing, please fix" << m_inputMethodProcess->program() << m_inputMethodProcess->arguments();
                stopInputMethod();
            }
        }
    });
}
bool InputMethod::isActive() const
{
    return waylandServer()->inputMethod()->context();
}

InputMethodGrabV1 *InputMethod::keyboardGrab()
{
    return isActive() ? m_keyboardGrab : nullptr;
}

void InputMethod::installKeyboardGrab(InputMethodGrabV1 *keyboardGrab)
{
    auto xkb = input()->keyboard()->xkb();
    m_keyboardGrab = keyboardGrab;
    keyboardGrab->sendKeymap(xkb->keymapContents());
    forwardModifiers(Force);
}

void InputMethod::updateModifiersMap(const QByteArray &modifiers)
{
    TextInputV2Interface *t2 = waylandServer()->seat()->textInputV2();

    if (t2 && t2->isEnabled()) {
        t2->setModifiersMap(modifiers);
    }
}

bool InputMethod::isVisible() const
{
    return m_panel && m_panel->isShown() && m_panel->readyForPainting();
}

bool InputMethod::isAvailable() const
{
    return !m_inputMethodCommand.isEmpty();
}

Window *InputMethod::activeWindow() const
{
    return m_trackedWindow;
}

void InputMethod::resetPendingPreedit()
{
    preedit.cursor = 0;
    preedit.highlightRanges.clear();
}

bool InputMethod::activeClientSupportsTextInput() const
{
    return m_activeClientSupportsTextInput;
}

void InputMethod::forceActivate()
{
    setActive(true);
    show();
}

void InputMethod::textInputInterfaceV3EnableRequested()
{
    refreshActive();
    show();
}
}

#include "moc_inputmethod.cpp"
