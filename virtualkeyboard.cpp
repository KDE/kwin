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
#include <KWaylandServer/textinput_interface.h>
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
        // we can announce support for the text input interface
        auto t = waylandServer()->display()->createTextInputManager(TextInputInterfaceVersion::UnstableV0, waylandServer()->display());
        t->create();
        auto t2 = waylandServer()->display()->createTextInputManager(TextInputInterfaceVersion::UnstableV2, waylandServer()->display());
        t2->create();

        connect(workspace(), &Workspace::clientAdded, this, [this] (AbstractClient *client) {
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
        });

        connect(waylandServer()->seat(), &SeatInterface::focusedTextInputChanged, this,
            [this] {
                disconnect(m_waylandShowConnection);
                disconnect(m_waylandHideConnection);
                disconnect(m_waylandHintsConnection);
                disconnect(m_waylandSurroundingTextConnection);
                disconnect(m_waylandResetConnection);
                disconnect(m_waylandEnabledConnection);
                disconnect(m_waylandStateCommittedConnection);
                if (auto t = waylandServer()->seat()->focusedTextInput()) {
                    m_waylandShowConnection = connect(t, &TextInputInterface::requestShowInputPanel, this, &VirtualKeyboard::show);
                    m_waylandHideConnection = connect(t, &TextInputInterface::requestHideInputPanel, this, &VirtualKeyboard::hide);
                    m_waylandSurroundingTextConnection = connect(t, &TextInputInterface::surroundingTextChanged, this,
                        [t] {
                            auto inputContext = waylandServer()->inputMethod()->context();
                            if (!inputContext) {
                                return;
                            }
                            inputContext->sendSurroundingText(QString::fromUtf8(t->surroundingText()), t->surroundingTextCursorPosition(), t->surroundingTextSelectionAnchor());
                        }
                    );
                    m_waylandHintsConnection = connect(t, &TextInputInterface::contentTypeChanged, this,
                        [t] {
                            auto inputContext = waylandServer()->inputMethod()->context();
                            if (!inputContext) {
                                return;
                            }
                            inputContext->sendContentType(t->contentHints(), t->contentPurpose());
                        }
                    );
                    m_waylandResetConnection = connect(t, &TextInputInterface::requestReset, this, [t] {
                        auto inputContext = waylandServer()->inputMethod()->context();
                        if (!inputContext) {
                            return;
                        }

                        inputContext->sendReset();
                        inputContext->sendSurroundingText(QString::fromUtf8(t->surroundingText()), t->surroundingTextCursorPosition(), t->surroundingTextSelectionAnchor());
                        inputContext->sendPreferredLanguage(QString::fromUtf8(t->preferredLanguage()));
                        inputContext->sendContentType(t->contentHints(), t->contentPurpose());
                    });
                    m_waylandEnabledConnection = connect(t, &TextInputInterface::enabledChanged, this, [t, this] {
                        if (t->isEnabled()) {
                            //FIXME This sendDeactivate shouldn't be necessary?
                            waylandServer()->inputMethod()->sendDeactivate();
                            waylandServer()->inputMethod()->sendActivate();
                            adoptInputMethodContext();
                        } else {
                            waylandServer()->inputMethod()->sendDeactivate();
                            hide();
                        }
                    });
                    m_waylandStateCommittedConnection = connect(t, &TextInputInterface::stateCommitted, this, [t](uint32_t serial) {
                        auto inputContext = waylandServer()->inputMethod()->context();
                        if (!inputContext) {
                            return;
                        }
                        inputContext->sendCommitState(serial);
                    });

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
        );
    }
}

void VirtualKeyboard::show()
{
    auto t = waylandServer()->seat()->focusedTextInput();
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
    auto t = waylandServer()->seat()->focusedTextInput();
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
    auto t = waylandServer()->seat()->focusedTextInput();
    if (t && t->isEnabled()) {
        t->commit(text.toUtf8());
        t->preEdit({}, {});
    }
}

static void setPreeditCursor(qint32 index)
{
    auto t = waylandServer()->seat()->focusedTextInput();
    if (t && t->isEnabled()) {
        t->setPreEditCursor(index);
    }
}

static void setPreeditString(uint32_t serial, const QString &text, const QString &commit)
{
    Q_UNUSED(serial)
    auto t = waylandServer()->seat()->focusedTextInput();
    if (t && t->isEnabled()) {
        t->preEdit(text.toUtf8(), commit.toUtf8());
    }
}

static void deleteSurroundingText(int32_t index, uint32_t length)
{
    auto t = waylandServer()->seat()->focusedTextInput();
    if (t && t->isEnabled()) {
        t->deleteSurroundingText(index, length);
    }
}

static void setCursorPosition(qint32 index, qint32 anchor)
{
    auto t = waylandServer()->seat()->focusedTextInput();
    if (t && t->isEnabled()) {
        t->setCursorPosition(index, anchor);
    }
}

static void setLanguage(uint32_t serial, const QString &language)
{
    Q_UNUSED(serial)
    auto t = waylandServer()->seat()->focusedTextInput();
    if (t && t->isEnabled()) {
        t->setLanguage(language.toUtf8());
    }
}

static void setTextDirection(uint32_t serial, Qt::LayoutDirection direction)
{
    Q_UNUSED(serial)
    auto t = waylandServer()->seat()->focusedTextInput();
    if (t && t->isEnabled()) {
        t->setTextDirection(direction);
    }
}

void VirtualKeyboard::adoptInputMethodContext()
{
    auto inputContext = waylandServer()->inputMethod()->context();
    TextInputInterface *ti = waylandServer()->seat()->focusedTextInput();

    inputContext->sendSurroundingText(QString::fromUtf8(ti->surroundingText()), ti->surroundingTextCursorPosition(), ti->surroundingTextSelectionAnchor());
    inputContext->sendPreferredLanguage(QString::fromUtf8(ti->preferredLanguage()));
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

    auto t = waylandServer()->seat()->focusedTextInput();

    if (!t) {
        return;
    }

    m_trackedClient->setVirtualKeyboardGeometry(m_inputClient ? m_inputClient->inputGeometry() : QRect());
    t->setInputPanelState(m_inputClient && m_inputClient->isShown(false), QRect(0, 0, 0, 0));
}

}
