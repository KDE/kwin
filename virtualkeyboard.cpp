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
#include "xkb.h"
#include "screenlockerwatcher.h"

#include <KWaylandServer/display.h>
#include <KWaylandServer/seat_interface.h>
#include <KWaylandServer/textinput_v2_interface.h>
#include <KWaylandServer/surface_interface.h>
#include <KWaylandServer/inputmethod_v1_interface.h>

#include <KStatusNotifierItem>
#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QQuickWindow>
#include <QTimer>
// xkbcommon
#include <xkbcommon/xkbcommon.h>

using namespace KWaylandServer;

namespace KWin
{

KWIN_SINGLETON_FACTORY(VirtualKeyboard)

VirtualKeyboard::VirtualKeyboard(QObject *parent)
    : QObject(parent)
{
    m_floodTimer = new QTimer(this);
    m_floodTimer->setSingleShot(true);
    m_floodTimer->setInterval(250);
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
        connect(workspace(), &Workspace::clientAdded, this, &VirtualKeyboard::clientAdded);
        connect(waylandServer()->seat(), &SeatInterface::focusedTextInputSurfaceChanged, this, &VirtualKeyboard::focusedTextInputChanged);
    }
}

void VirtualKeyboard::show()
{
    auto t = waylandServer()->seat()->textInput();
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

void VirtualKeyboard::focusedTextInputChanged()
{
    disconnect(m_waylandShowConnection);
    disconnect(m_waylandHideConnection);
    disconnect(m_waylandHintsConnection);
    disconnect(m_waylandSurroundingTextConnection);
    disconnect(m_waylandResetConnection);
    disconnect(m_waylandEnabledConnection);
    disconnect(m_waylandStateCommittedConnection);
    if (auto t = waylandServer()->seat()->textInput()) {
        // connections from textinput_interface
        m_waylandShowConnection = connect(t, &TextInputV2Interface::requestShowInputPanel, this, &VirtualKeyboard::show);
        m_waylandHideConnection = connect(t, &TextInputV2Interface::requestHideInputPanel, this, &VirtualKeyboard::hide);
        m_waylandSurroundingTextConnection = connect(t, &TextInputV2Interface::surroundingTextChanged, this, &VirtualKeyboard::surroundingTextChanged);
        m_waylandHintsConnection = connect(t, &TextInputV2Interface::contentTypeChanged, this, &VirtualKeyboard::contentTypeChanged);
        m_waylandResetConnection = connect(t, &TextInputV2Interface::requestReset, this, &VirtualKeyboard::requestReset);
        m_waylandEnabledConnection = connect(t, &TextInputV2Interface::enabledChanged, this, &VirtualKeyboard::textInputInterfaceEnabledChanged);
        m_waylandStateCommittedConnection = connect(t, &TextInputV2Interface::stateCommitted, this, &VirtualKeyboard::stateCommitted);

        auto newClient = waylandServer()->findClient(waylandServer()->seat()->focusedTextInputSurface());
        // Reset the old client virtual keybaord geom if necessary
        // Old and new clients could be the same if focus moves between subsurfaces
        if (newClient != m_trackedClient) {
            if (m_trackedClient) {
                m_trackedClient->setVirtualKeyboardGeometry(QRect());
            }
            m_trackedClient = newClient;
        }
    } else {
        m_waylandShowConnection = QMetaObject::Connection();
        m_waylandHideConnection = QMetaObject::Connection();
        m_waylandHintsConnection = QMetaObject::Connection();
        m_waylandSurroundingTextConnection = QMetaObject::Connection();
        m_waylandResetConnection = QMetaObject::Connection();
        m_waylandEnabledConnection = QMetaObject::Connection();
        m_waylandStateCommittedConnection = QMetaObject::Connection();
        waylandServer()->inputMethod()->sendDeactivate();
    }
    updateInputPanelState();
}

void VirtualKeyboard::surroundingTextChanged()
{
    auto t = waylandServer()->seat()->textInput();
    auto inputContext = waylandServer()->inputMethod()->context();
    if (!inputContext) {
        return;
    }
    inputContext->sendSurroundingText(t->surroundingText(), t->surroundingTextCursorPosition(), t->surroundingTextSelectionAnchor());
}

void VirtualKeyboard::contentTypeChanged()
{
    auto t = waylandServer()->seat()->textInput();
    auto inputContext = waylandServer()->inputMethod()->context();
    if (!inputContext) {
        return;
    }
    inputContext->sendContentType(t->contentHints(), t->contentPurpose());
}

void VirtualKeyboard::requestReset()
{
    auto t = waylandServer()->seat()->textInput();
    auto inputContext = waylandServer()->inputMethod()->context();
    if (!inputContext) {
        return;
    }
    inputContext->sendReset();
    inputContext->sendSurroundingText(t->surroundingText(), t->surroundingTextCursorPosition(), t->surroundingTextSelectionAnchor());
    inputContext->sendPreferredLanguage(t->preferredLanguage());
    inputContext->sendContentType(t->contentHints(), t->contentPurpose());
}

void VirtualKeyboard::textInputInterfaceEnabledChanged()
{
    auto t = waylandServer()->seat()->textInput();
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

static void keysymReceived(quint32 serial, quint32 time, quint32 sym, bool pressed, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(serial)
    Q_UNUSED(time)
    auto t = waylandServer()->seat()->textInput();
    if (t && t->isEnabled()) {
        if (pressed) {
            t->keysymPressed(sym, modifiers);
        } else {
            t->keysymReleased(sym, modifiers);
        }
    }
}

static void commitString(qint32 serial, const QString &text)
{
    Q_UNUSED(serial)
    auto t = waylandServer()->seat()->textInput();
    if (t && t->isEnabled()) {
        t->commit(text.toUtf8());
        t->preEdit({}, {});
    }
}

static void setPreeditCursor(qint32 index)
{
    auto t = waylandServer()->seat()->textInput();
    if (t && t->isEnabled()) {
        t->setPreEditCursor(index);
    }
}

static void setPreeditString(uint32_t serial, const QString &text, const QString &commit)
{
    Q_UNUSED(serial)
    auto t = waylandServer()->seat()->textInput();
    if (t && t->isEnabled()) {
        t->preEdit(text.toUtf8(), commit.toUtf8());
    }
}

static void deleteSurroundingText(int32_t index, uint32_t length)
{
    auto t = waylandServer()->seat()->textInput();
    if (t && t->isEnabled()) {
        t->deleteSurroundingText(index, length);
    }
}

static void setCursorPosition(qint32 index, qint32 anchor)
{
    auto t = waylandServer()->seat()->textInput();
    if (t && t->isEnabled()) {
        t->setCursorPosition(index, anchor);
    }
}

static void setLanguage(uint32_t serial, const QString &language)
{
    Q_UNUSED(serial)
    auto t = waylandServer()->seat()->textInput();
    if (t && t->isEnabled()) {
        t->setLanguage(language.toUtf8());
    }
}

static void setTextDirection(uint32_t serial, Qt::LayoutDirection direction)
{
    Q_UNUSED(serial)
    auto t = waylandServer()->seat()->textInput();
    if (t && t->isEnabled()) {
        t->setTextDirection(direction);
    }
}

void VirtualKeyboard::adoptInputMethodContext()
{
    auto inputContext = waylandServer()->inputMethod()->context();
    TextInputV2Interface *ti = waylandServer()->seat()->textInput();

    inputContext->sendSurroundingText(ti->surroundingText(), ti->surroundingTextCursorPosition(), ti->surroundingTextSelectionAnchor());
    inputContext->sendPreferredLanguage(ti->preferredLanguage());
    inputContext->sendContentType(ti->contentHints(), ti->contentPurpose());

    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::keysym, waylandServer(), &keysymReceived);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::commitString, waylandServer(), &commitString);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::preeditCursor, waylandServer(), &setPreeditCursor);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::preeditString, waylandServer(), &setPreeditString);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::deleteSurroundingText, waylandServer(), &deleteSurroundingText);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::cursorPosition, waylandServer(), &setCursorPosition);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::language, waylandServer(), &setLanguage);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::textDirection, waylandServer(), &setTextDirection);
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

    auto t = waylandServer()->seat()->textInput();

    if (!t) {
        return;
    }

    if (m_trackedClient) {
        m_trackedClient->setVirtualKeyboardGeometry(m_inputClient ? m_inputClient->inputGeometry() : QRect());
    }
    t->setInputPanelState(m_inputClient && m_inputClient->isShown(false), QRect(0, 0, 0, 0));
}

}
