/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "virtualkeyboard.h"
#include "virtualkeyboard_dbus.h"
#include "input.h"
#include "keyboard_input.h"
#include "utils.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "screenlockerwatcher.h"

#include <KWaylandServer/display.h>
#include <KWaylandServer/seat_interface.h>
#include <KWaylandServer/textinput_v2_interface.h>
#include <KWaylandServer/textinput_v3_interface.h>
#include <KWaylandServer/surface_interface.h>
#include <KWaylandServer/inputmethod_v1_interface.h>

#include <KStatusNotifierItem>
#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QDBusMessage>

#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon-keysyms.h>

using namespace KWaylandServer;

namespace KWin
{

KWIN_SINGLETON_FACTORY(VirtualKeyboard)

VirtualKeyboard::VirtualKeyboard(QObject *parent)
    : QObject(parent)
{
    // this is actually too late. Other processes are started before init,
    // so might miss the availability of text input
    // but without Workspace we don't have the window listed at all
    connect(kwinApp(), &Application::workspaceCreated, this, &VirtualKeyboard::init);
}

VirtualKeyboard::~VirtualKeyboard() = default;

void VirtualKeyboard::init()
{
    connect(ScreenLockerWatcher::self(), &ScreenLockerWatcher::aboutToLock, this, &VirtualKeyboard::hide);

    if (waylandServer()) {
        m_enabled = !input()->hasAlphaNumericKeyboard();
        qCDebug(KWIN_VIRTUALKEYBOARD) << "enabled by default: " << m_enabled;
        connect(input(), &InputRedirection::hasAlphaNumericKeyboardChanged, this,
            [this] (bool set) {
                qCDebug(KWIN_VIRTUALKEYBOARD) << "AlphaNumeric Keyboard changed:" << set << "toggling VirtualKeyboard.";
                setEnabled(!set);
            }
        );
    }

    qCDebug(KWIN_VIRTUALKEYBOARD) << "Registering the SNI";
    m_sni = new KStatusNotifierItem(QStringLiteral("kwin-virtual-keyboard"), this);
    m_sni->setStandardActionsEnabled(false);
    m_sni->setCategory(KStatusNotifierItem::Hardware);
    m_sni->setStatus(KStatusNotifierItem::Passive);
    m_sni->setTitle(i18n("Virtual Keyboard"));
    updateSni();
    connect(m_sni, &KStatusNotifierItem::activateRequested, this,
        [this] {
            setEnabled(!m_enabled);
        }
    );
    connect(this, &VirtualKeyboard::enabledChanged, this, &VirtualKeyboard::updateSni);

    auto dbus = new VirtualKeyboardDBus(this);
    qCDebug(KWIN_VIRTUALKEYBOARD) << "Registering the DBus interface";
    dbus->setEnabled(m_enabled);
    connect(dbus, &VirtualKeyboardDBus::activateRequested, this, &VirtualKeyboard::setEnabled);
    connect(this, &VirtualKeyboard::enabledChanged, dbus, &VirtualKeyboardDBus::setEnabled);
    connect(input(), &InputRedirection::keyStateChanged, this, &VirtualKeyboard::hide);

    if (waylandServer()) {
        waylandServer()->display()->createTextInputManagerV2();
        waylandServer()->display()->createTextInputManagerV3();

        connect(workspace(), &Workspace::clientAdded, this, &VirtualKeyboard::clientAdded);
        connect(waylandServer()->seat(), &SeatInterface::focusedTextInputSurfaceChanged, this, &VirtualKeyboard::handleFocusedSurfaceChanged);

        TextInputV2Interface *textInputV2 = waylandServer()->seat()->textInputV2();
        connect(textInputV2, &TextInputV2Interface::requestShowInputPanel, this, &VirtualKeyboard::show);
        connect(textInputV2, &TextInputV2Interface::requestHideInputPanel, this, &VirtualKeyboard::hide);
        connect(textInputV2, &TextInputV2Interface::surroundingTextChanged, this, &VirtualKeyboard::surroundingTextChanged);
        connect(textInputV2, &TextInputV2Interface::contentTypeChanged, this, &VirtualKeyboard::contentTypeChanged);
        connect(textInputV2, &TextInputV2Interface::requestReset, this, &VirtualKeyboard::requestReset);
        connect(textInputV2, &TextInputV2Interface::enabledChanged, this, &VirtualKeyboard::textInputInterfaceV2EnabledChanged);
        connect(textInputV2, &TextInputV2Interface::stateCommitted, this, &VirtualKeyboard::stateCommitted);

        TextInputV3Interface *textInputV3 = waylandServer()->seat()->textInputV3();
        connect(textInputV3, &TextInputV3Interface::enabledChanged, this, &VirtualKeyboard::textInputInterfaceV3EnabledChanged);
        connect(textInputV3, &TextInputV3Interface::surroundingTextChanged, this, &VirtualKeyboard::surroundingTextChanged);
        connect(textInputV3, &TextInputV3Interface::contentTypeChanged, this, &VirtualKeyboard::contentTypeChanged);
        connect(textInputV3, &TextInputV3Interface::stateCommitted, this, &VirtualKeyboard::stateCommitted);
    }
}

void VirtualKeyboard::show()
{
    auto t = waylandServer()->seat()->textInputV2();
    if (t) {
        //FIXME: this shouldn't be necessary and causes double emits?
        Q_EMIT t->enabledChanged();
    }
}

void VirtualKeyboard::hide()
{
    waylandServer()->inputMethod()->sendDeactivate();
    updateInputPanelState();
}

void VirtualKeyboard::clientAdded(AbstractClient* client)
{
    if (!client->isInputMethod()) {
        return;
    }
    m_inputClient = client;
    auto refreshFrame = [this] {
        if (!m_trackedClient) {
            return;
        }

        if (m_inputClient && !m_inputClient->inputGeometry().isEmpty()) {
            m_trackedClient->setVirtualKeyboardGeometry(m_inputClient->inputGeometry());
        }
    };
    connect(client->surface(), &SurfaceInterface::inputChanged, this, refreshFrame);
    connect(client, &QObject::destroyed, this, [this] {
        if (m_trackedClient) {
            m_trackedClient->setVirtualKeyboardGeometry({});
        }
    });
    connect(m_inputClient, &AbstractClient::frameGeometryChanged, this, refreshFrame);
}

void VirtualKeyboard::handleFocusedSurfaceChanged()
{
    SurfaceInterface *focusedSurface = waylandServer()->seat()->focusedTextInputSurface();
    if (focusedSurface) {
        AbstractClient *focusedClient = waylandServer()->findClient(focusedSurface);
        // Reset the old client virtual keybaord geom if necessary
        // Old and new clients could be the same if focus moves between subsurfaces
        if (m_trackedClient != focusedClient) {
            if (m_trackedClient) {
                m_trackedClient->setVirtualKeyboardGeometry(QRect());
            }
            m_trackedClient = focusedClient;
        }
    } else {
        waylandServer()->inputMethod()->sendDeactivate();
    }
    updateInputPanelState();
}

void VirtualKeyboard::surroundingTextChanged()
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
}

void VirtualKeyboard::contentTypeChanged()
{
    auto t2 = waylandServer()->seat()->textInputV2();
    auto t3 = waylandServer()->seat()->textInputV3();
    auto inputContext = waylandServer()->inputMethod()->context();
    if (!inputContext) {
        return;
    }
    if (t2 && t2->isEnabled()) {
        inputContext->sendContentType(t2->contentHints(), t2->contentPurpose());
    }
    if (t3 && t3->isEnabled()) {
        inputContext->sendContentType(t3->contentHints(), t3->contentPurpose());
    }
}

void VirtualKeyboard::requestReset()
{
    auto t2 = waylandServer()->seat()->textInputV2();
    auto inputContext = waylandServer()->inputMethod()->context();
    if (!inputContext) {
        return;
    }
    inputContext->sendReset();
    if (t2 && t2->isEnabled()) {
        inputContext->sendSurroundingText(t2->surroundingText(), t2->surroundingTextCursorPosition(), t2->surroundingTextSelectionAnchor());
        inputContext->sendPreferredLanguage(t2->preferredLanguage());
        inputContext->sendContentType(t2->contentHints(), t2->contentPurpose());
    }
}

void VirtualKeyboard::textInputInterfaceV2EnabledChanged()
{
    auto t = waylandServer()->seat()->textInputV2();
    if (t->isEnabled()) {
        //FIXME This sendDeactivate shouldn't be necessary?
        waylandServer()->inputMethod()->sendDeactivate();
        waylandServer()->inputMethod()->sendActivate();
        adoptInputMethodContext();
    } else {
        waylandServer()->inputMethod()->sendDeactivate();
        hide();
    }
}

void VirtualKeyboard::textInputInterfaceV3EnabledChanged()
{
    auto t3 = waylandServer()->seat()->textInputV3();
    if (t3->isEnabled()) {
        waylandServer()->inputMethod()->sendActivate();
        adoptInputMethodContext();
    } else {
        waylandServer()->inputMethod()->sendDeactivate();
        // reset value of preedit when textinput is disabled
        preedit.text = QString();
        preedit.begin = 0;
        preedit.end = 0;
    }
}

void VirtualKeyboard::stateCommitted(uint32_t serial)
{
    auto inputContext = waylandServer()->inputMethod()->context();
    if (!inputContext) {
        return;
    }
    inputContext->sendCommitState(serial);
}

void VirtualKeyboard::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    emit enabledChanged(m_enabled);

    // send OSD message
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/org/kde/osdService"),
        QStringLiteral("org.kde.osdService"),
        QStringLiteral("virtualKeyboardEnabledChanged")
    );
    msg.setArguments({enabled});
    QDBusConnection::sessionBus().asyncCall(msg);
}

static quint32 keysymToKeycode(quint32 sym)
{
    switch(sym) {
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

static void keysymReceived(quint32 serial, quint32 time, quint32 sym, bool pressed, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(serial)
    Q_UNUSED(time)
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        if (pressed) {
            t2->keysymPressed(sym, modifiers);
        } else {
            t2->keysymReleased(sym, modifiers);
        }
        return;
    }
    auto t3 = waylandServer()->seat()->textInputV3();
    if (t3 && t3->isEnabled()) {
        if (pressed) {
            waylandServer()->seat()->keyPressed(keysymToKeycode(sym));
        } else {
            waylandServer()->seat()->keyReleased(keysymToKeycode(sym));
        }
        return;
    }
}

static void commitString(qint32 serial, const QString &text)
{
    Q_UNUSED(serial)
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->commitString(text.toUtf8());
        t2->preEdit({}, {});
        return;
    }
    auto t3 = waylandServer()->seat()->textInputV3();
    if (t3 && t3->isEnabled()) {
        t3->commitString(text.toUtf8());
        t3->done();
        return;
    }
}

static void deleteSurroundingText(int32_t index, uint32_t length)
{
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->deleteSurroundingText(index, length);
    }
    auto t3 = waylandServer()->seat()->textInputV3();
    if (t3 && t3->isEnabled()) {
        t3->deleteSurroundingText(index, length);
    }
}

static void setCursorPosition(qint32 index, qint32 anchor)
{
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->setCursorPosition(index, anchor);
    }
}

static void setLanguage(uint32_t serial, const QString &language)
{
    Q_UNUSED(serial)
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->setLanguage(language.toUtf8());
    }
}

static void setTextDirection(uint32_t serial, Qt::LayoutDirection direction)
{
    Q_UNUSED(serial)
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->setTextDirection(direction);
    }
}

void VirtualKeyboard::setPreeditCursor(qint32 index)
{
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->setPreEditCursor(index);
    }
    auto t3 = waylandServer()->seat()->textInputV3();
    if (t3 && t3->isEnabled()) {
        preedit.begin = index;
        preedit.end = index;
        t3->sendPreEditString(preedit.text, preedit.begin, preedit.end);
    }
}


void VirtualKeyboard::setPreeditString(uint32_t serial, const QString &text, const QString &commit)
{
    Q_UNUSED(serial)
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->preEdit(text.toUtf8(), commit.toUtf8());
    }
    auto t3 = waylandServer()->seat()->textInputV3();
    if (t3 && t3->isEnabled()) {
        preedit.text = text;
        t3->sendPreEditString(preedit.text, preedit.begin, preedit.end);
    }
}

void VirtualKeyboard::adoptInputMethodContext()
{
    auto inputContext = waylandServer()->inputMethod()->context();

    TextInputV2Interface *t2 = waylandServer()->seat()->textInputV2();
    TextInputV3Interface *t3 = waylandServer()->seat()->textInputV3();

    if (t2 && t2->isEnabled()) {
        inputContext->sendSurroundingText(t2->surroundingText(), t2->surroundingTextCursorPosition(), t2->surroundingTextSelectionAnchor());
        inputContext->sendPreferredLanguage(t2->preferredLanguage());
        inputContext->sendContentType(t2->contentHints(), t2->contentPurpose());
        connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::language, waylandServer(), &setLanguage);
        connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::textDirection, waylandServer(), &setTextDirection);
    }

    if (t3 && t3->isEnabled()) {
        inputContext->sendSurroundingText(t3->surroundingText(), t3->surroundingTextCursorPosition(), t3->surroundingTextSelectionAnchor());
        inputContext->sendContentType(t3->contentHints(), t3->contentPurpose());
    }

    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::keysym, waylandServer(), &keysymReceived);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::commitString, waylandServer(), &commitString);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::deleteSurroundingText, waylandServer(), &deleteSurroundingText);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::cursorPosition, waylandServer(), &setCursorPosition);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::preeditString, this, &VirtualKeyboard::setPreeditString);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::preeditCursor, this, &VirtualKeyboard::setPreeditCursor);
}

void VirtualKeyboard::updateSni()
{
    if (!m_sni) {
        return;
    }
    if (m_enabled) {
        m_sni->setIconByName(QStringLiteral("input-keyboard-virtual-on"));
        m_sni->setTitle(i18n("Virtual Keyboard: enabled"));
    } else {
        m_sni->setIconByName(QStringLiteral("input-keyboard-virtual-off"));
        m_sni->setTitle(i18n("Virtual Keyboard: disabled"));
    }
    m_sni->setToolTipTitle(i18n("Whether to show the virtual keyboard on demand."));
}

void VirtualKeyboard::updateInputPanelState()
{
    if (!waylandServer()) {
        return;
    }

    auto t = waylandServer()->seat()->textInputV2();

    if (!t) {
        return;
    }

    if (m_trackedClient) {
        m_trackedClient->setVirtualKeyboardGeometry(m_inputClient ? m_inputClient->inputGeometry() : QRect());
    }
    t->setInputPanelState(m_inputClient && m_inputClient->isShown(false), QRect(0, 0, 0, 0));
}

}
