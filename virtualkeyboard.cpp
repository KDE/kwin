/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#include "virtualkeyboard.h"
#include "virtualkeyboard_dbus.h"
#include "input.h"
#include "keyboard_input.h"
#include "utils.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "xkb.h"

#include <KWayland/Server/display.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/textinput_interface.h>

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
// xkbcommon
#include <xkbcommon/xkbcommon.h>

using namespace KWayland::Server;

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
    // TODO: need a shared Qml engine
    m_inputWindow.reset(new QQuickView(nullptr));
    m_inputWindow->setFlags(Qt::FramelessWindowHint);
    m_inputWindow->setGeometry(screens()->geometry(screens()->current()));
    m_inputWindow->setResizeMode(QQuickView::SizeRootObjectToView);
    m_inputWindow->setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral(KWIN_NAME "/virtualkeyboard/main.qml"))));
    if (m_inputWindow->status() != QQuickView::Status::Ready) {
        m_inputWindow.reset();
        return;
    }
    m_inputWindow->setProperty("__kwin_input_method", true);

    if (waylandServer()) {
        m_enabled = !input()->hasAlphaNumericKeyboard();
        connect(input(), &InputRedirection::hasAlphaNumericKeyboardChanged, this,
            [this] (bool set) {
                setEnabled(!set);
            }
        );
    }

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
    dbus->setEnabled(m_enabled);
    connect(dbus, &VirtualKeyboardDBus::activateRequested, this, &VirtualKeyboard::setEnabled);
    connect(this, &VirtualKeyboard::enabledChanged, dbus, &VirtualKeyboardDBus::setEnabled);

    if (waylandServer()) {
        // we can announce support for the text input interface
        auto t = waylandServer()->display()->createTextInputManager(TextInputInterfaceVersion::UnstableV0, waylandServer()->display());
        t->create();
        auto t2 = waylandServer()->display()->createTextInputManager(TextInputInterfaceVersion::UnstableV2, waylandServer()->display());
        t2->create();
        connect(waylandServer()->seat(), &SeatInterface::focusedTextInputChanged, this,
            [this] {
                disconnect(m_waylandShowConnection);
                disconnect(m_waylandHideConnection);
                disconnect(m_waylandHintsConnection);
                disconnect(m_waylandSurroundingTextConnection);
                disconnect(m_waylandResetConnection);
                disconnect(m_waylandEnabledConnection);
                qApp->inputMethod()->reset();
                if (auto t = waylandServer()->seat()->focusedTextInput()) {
                    m_waylandShowConnection = connect(t, &TextInputInterface::requestShowInputPanel, this, &VirtualKeyboard::show);
                    m_waylandHideConnection = connect(t, &TextInputInterface::requestHideInputPanel, this, &VirtualKeyboard::hide);
                    m_waylandSurroundingTextConnection = connect(t, &TextInputInterface::surroundingTextChanged, this,
                        [] {
                            qApp->inputMethod()->update(Qt::ImSurroundingText | Qt::ImCursorPosition | Qt::ImAnchorPosition);
                        }
                    );
                    m_waylandHintsConnection = connect(t, &TextInputInterface::contentTypeChanged, this,
                        [] {
                            qApp->inputMethod()->update(Qt::ImHints);
                        }
                    );
                    m_waylandResetConnection = connect(t, &TextInputInterface::requestReset, qApp->inputMethod(), &QInputMethod::reset);
                    m_waylandEnabledConnection = connect(t, &TextInputInterface::enabledChanged, this,
                        [] {
                            qApp->inputMethod()->update(Qt::ImQueryAll);
                        }
                    );
                    // TODO: calculate overlap
                    t->setInputPanelState(m_inputWindow->isVisible(), QRect(0, 0, 0, 0));
                } else {
                    m_waylandShowConnection = QMetaObject::Connection();
                    m_waylandHideConnection = QMetaObject::Connection();
                    m_waylandHintsConnection = QMetaObject::Connection();
                    m_waylandSurroundingTextConnection = QMetaObject::Connection();
                    m_waylandResetConnection = QMetaObject::Connection();
                    m_waylandEnabledConnection = QMetaObject::Connection();
                }
                qApp->inputMethod()->update(Qt::ImQueryAll);
            }
        );
    }
    m_inputWindow->installEventFilter(this);
    connect(Workspace::self(), &Workspace::destroyed, this,
        [this] {
            m_inputWindow.reset();
        }
    );
    m_inputWindow->setColor(Qt::transparent);
    m_inputWindow->setMask(m_inputWindow->rootObject()->childrenRect().toRect());
    connect(m_inputWindow->rootObject(), &QQuickItem::childrenRectChanged, m_inputWindow.data(),
        [this] {
            if (!m_inputWindow) {
                return;
            }
            m_inputWindow->setMask(m_inputWindow->rootObject()->childrenRect().toRect());
        }
    );
    connect(qApp->inputMethod(), &QInputMethod::visibleChanged, m_inputWindow.data(),
        [this] {
            m_inputWindow->setVisible(qApp->inputMethod()->isVisible());
            if (qApp->inputMethod()->isVisible()) {
                m_inputWindow->setMask(m_inputWindow->rootObject()->childrenRect().toRect());
            }
            if (waylandServer()) {
                if (auto t = waylandServer()->seat()->focusedTextInput()) {
                    // TODO: calculate overlap
                    t->setInputPanelState(m_inputWindow->isVisible(), QRect(0, 0, 0, 0));
                }
            }
        }
    );
}

void VirtualKeyboard::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    qApp->inputMethod()->update(Qt::ImQueryAll);
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

void VirtualKeyboard::updateSni()
{
    if (!m_sni) {
        return;
    }
    if (m_enabled) {
        m_sni->setIconByName(QStringLiteral("input-keyboard-virtual-on"));
        m_sni->setToolTipTitle(i18n("Virtual Keyboard is enabled."));
    } else {
        m_sni->setIconByName(QStringLiteral("input-keyboard-virtual-off"));
        m_sni->setToolTipTitle(i18n("Virtual Keyboard is disabled."));
    }
}

void VirtualKeyboard::show()
{
    if (m_inputWindow.isNull() || !m_enabled) {
        return;
    }
    m_inputWindow->setGeometry(screens()->geometry(screens()->current()));
    qApp->inputMethod()->show();
}

void VirtualKeyboard::hide()
{
    if (m_inputWindow.isNull()) {
        return;
    }
    qApp->inputMethod()->hide();
}

bool VirtualKeyboard::event(QEvent *e)
{
    if (e->type() == QEvent::InputMethod) {
        QInputMethodEvent *event = static_cast<QInputMethodEvent*>(e);
        if (m_enabled && waylandServer()) {
            bool isPreedit = false;
            for (auto attribute : event->attributes()) {
                switch (attribute.type) {
                case QInputMethodEvent::TextFormat:
                case QInputMethodEvent::Cursor:
                case QInputMethodEvent::Language:
                case QInputMethodEvent::Ruby:
                    isPreedit = true;
                    break;
                default:
                    break;
                }
            }
            TextInputInterface *ti = waylandServer()->seat()->focusedTextInput();
            if (ti && ti->isEnabled()) {
                if (!isPreedit && event->preeditString().isEmpty() && !event->commitString().isEmpty()) {
                    ti->commit(event->commitString().toUtf8());
                } else {
                    ti->preEdit(event->preeditString().toUtf8(), event->commitString().toUtf8());
                }
            }
        }
    }
    if (e->type() == QEvent::InputMethodQuery) {
        auto event = static_cast<QInputMethodQueryEvent*>(e);
        TextInputInterface *ti = nullptr;
        if (waylandServer() && m_enabled) {
            ti = waylandServer()->seat()->focusedTextInput();
        }
        if (event->queries().testFlag(Qt::ImEnabled)) {
            event->setValue(Qt::ImEnabled, QVariant(ti != nullptr && ti->isEnabled()));
        }
        if (event->queries().testFlag(Qt::ImCursorRectangle)) {
            // not used by virtual keyboard
        }
        if (event->queries().testFlag(Qt::ImFont)) {
            // not used by virtual keyboard
        }
        if (event->queries().testFlag(Qt::ImCursorPosition)) {
            // the virtual keyboard doesn't send us the cursor position in the preedit
            // this would break text input, thus we ignore it
            // see https://bugreports.qt.io/browse/QTBUG-53517
#if 0
            event->setValue(Qt::ImCursorPosition, QString::fromUtf8(ti->surroundingText().left(ti->surroundingTextCursorPosition())).size());
#else
            event->setValue(Qt::ImCursorPosition, 0);
#endif
        }
        if (event->queries().testFlag(Qt::ImSurroundingText)) {
            // the virtual keyboard doesn't send us the cursor position in the preedit
            // this would break text input, thus we ignore it
            // see https://bugreports.qt.io/browse/QTBUG-53517
#if 0
            event->setValue(Qt::ImSurroundingText, QString::fromUtf8(ti->surroundingText()));
#else
            event->setValue(Qt::ImSurroundingText, QString());
#endif
        }
        if (event->queries().testFlag(Qt::ImCurrentSelection)) {
            // TODO: should be text between cursor and anchor, but might be dangerous
        }
        if (event->queries().testFlag(Qt::ImMaximumTextLength)) {
            // not used by virtual keyboard
        }
        if (event->queries().testFlag(Qt::ImAnchorPosition)) {
            // not used by virtual keyboard
        }
        if (event->queries().testFlag(Qt::ImHints)) {
            if (ti && ti->isEnabled()) {
                Qt::InputMethodHints hints;
                const auto contentHints = ti->contentHints();
                if (!contentHints.testFlag(TextInputInterface::ContentHint::AutoCompletion)) {
                    hints |= Qt::ImhNoPredictiveText;
                }
                if (contentHints.testFlag(TextInputInterface::ContentHint::AutoCorrection)) {
                    // no mapping in Qt
                }
                if (!contentHints.testFlag(TextInputInterface::ContentHint::AutoCapitalization)) {
                    hints |= Qt::ImhNoAutoUppercase;
                }
                if (contentHints.testFlag(TextInputInterface::ContentHint::LowerCase)) {
                    hints |= Qt::ImhPreferLowercase;
                }
                if (contentHints.testFlag(TextInputInterface::ContentHint::UpperCase)) {
                    hints |= Qt::ImhPreferUppercase;
                }
                if (contentHints.testFlag(TextInputInterface::ContentHint::TitleCase)) {
                    // no mapping in Qt
                }
                if (contentHints.testFlag(TextInputInterface::ContentHint::HiddenText)) {
                    hints |= Qt::ImhHiddenText;
                }
                if (contentHints.testFlag(TextInputInterface::ContentHint::SensitiveData)) {
                    hints |= Qt::ImhSensitiveData;
                }
                if (contentHints.testFlag(TextInputInterface::ContentHint::Latin)) {
                    hints |= Qt::ImhPreferLatin;
                }
                if (contentHints.testFlag(TextInputInterface::ContentHint::MultiLine)) {
                    hints |= Qt::ImhMultiLine;
                }
                switch (ti->contentPurpose()) {
                case TextInputInterface::ContentPurpose::Digits:
                    hints |= Qt::ImhDigitsOnly;
                    break;
                case TextInputInterface::ContentPurpose::Number:
                    hints |= Qt::ImhFormattedNumbersOnly;
                    break;
                case TextInputInterface::ContentPurpose::Phone:
                    hints |= Qt::ImhDialableCharactersOnly;
                    break;
                case TextInputInterface::ContentPurpose::Url:
                    hints |= Qt::ImhUrlCharactersOnly;
                    break;
                case TextInputInterface::ContentPurpose::Email:
                    hints |= Qt::ImhEmailCharactersOnly;
                    break;
                case TextInputInterface::ContentPurpose::Date:
                    hints |= Qt::ImhDate;
                    break;
                case TextInputInterface::ContentPurpose::Time:
                    hints |= Qt::ImhTime;
                    break;
                case TextInputInterface::ContentPurpose::DateTime:
                    hints |= Qt::ImhDate;
                    hints |= Qt::ImhTime;
                    break;
                case TextInputInterface::ContentPurpose::Name:
                    // no mapping in Qt
                case TextInputInterface::ContentPurpose::Password:
                    // no mapping in Qt
                case TextInputInterface::ContentPurpose::Terminal:
                    // no mapping in Qt
                case TextInputInterface::ContentPurpose::Normal:
                    // that's the default
                case TextInputInterface::ContentPurpose::Alpha:
                    // no mapping in Qt
                    break;
                }
                event->setValue(Qt::ImHints, QVariant(int(hints)));
            } else {
                event->setValue(Qt::ImHints, Qt::ImhNone);
            }
        }
        if (event->queries().testFlag(Qt::ImPreferredLanguage)) {
            // not used by virtual keyboard
        }
        if (event->queries().testFlag(Qt::ImPlatformData)) {
            // not used by virtual keyboard
        }
        if (event->queries().testFlag(Qt::ImAbsolutePosition)) {
            // not used by virtual keyboard
        }
        if (event->queries().testFlag(Qt::ImTextBeforeCursor)) {
            // not used by virtual keyboard
        }
        if (event->queries().testFlag(Qt::ImTextAfterCursor)) {
            // not used by virtual keyboard
        }
        event->accept();
        return true;
    }
    return QObject::event(e);
}

bool VirtualKeyboard::eventFilter(QObject *o, QEvent *e)
{
    if (o != m_inputWindow.data() || !m_inputWindow->isVisible()) {
        return false;
    }
    if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) {
        QKeyEvent *event = static_cast<QKeyEvent*>(e);
        if (event->nativeScanCode() == 0) {
            // this is a key composed by the virtual keyboard - we need to send it to the client
            const auto sym = input()->keyboard()->xkb()->fromKeyEvent(event);
            if (sym != 0) {
                if (waylandServer()) {
                    auto t = waylandServer()->seat()->focusedTextInput();
                    if (t && t->isEnabled()) {
                        if (e->type() == QEvent::KeyPress) {
                            t->keysymPressed(sym);
                        } else if (e->type() == QEvent::KeyRelease) {
                            t->keysymReleased(sym);
                        }
                    }
                }
            }
            return true;
        }
    }
    return false;
}

QWindow *VirtualKeyboard::inputPanel() const
{
    return m_inputWindow.data();
}

}
