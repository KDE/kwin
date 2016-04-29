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
#include "input_methods.h"
#include "utils.h"
#include "screens.h"
#include "scripting/scripting.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Server/display.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/text_interface.h>

#include <KKeyServer>

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QQuickWindow>

using namespace KWayland::Server;

namespace KWin
{

InputMethods::InputMethods(QObject *parent)
    : QObject(parent)
{
}

InputMethods::~InputMethods() = default;

void InputMethods::init()
{
    m_inputWindow.reset(new QQuickView(Scripting::self()->qmlEngine(), nullptr));
    m_inputWindow->setGeometry(screens()->geometry(screens()->current()));
    m_inputWindow->setResizeMode(QQuickView::SizeRootObjectToView);
    m_inputWindow->setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral(KWIN_NAME "/virtualkeyboard/main.qml"))));
    if (m_inputWindow->status() != QQuickView::Status::Ready) {
        m_inputWindow.reset();
        return;
    }
    m_inputWindow->setProperty("__kwin_input_method", true);
    if (waylandServer()) {
        // we can announce support for the text input interface
        auto t = waylandServer()->display()->createTextInputManagerUnstableV0(waylandServer()->display());
        t->create();
        connect(waylandServer()->seat(), &SeatInterface::textInputChanged, this,
            [this] {
                disconnect(m_waylandShowConnection);
                disconnect(m_waylandHideConnection);
                disconnect(m_waylandHintsConnection);
                disconnect(m_waylandSurroundingTextConnection);
                disconnect(m_waylandResetConnection);
                qApp->inputMethod()->reset();
                if (auto t = waylandServer()->seat()->textInput()) {
                    m_waylandShowConnection = connect(t, &TextInputInterface::requestShowInputPanel, this, &InputMethods::show);
                    m_waylandHideConnection = connect(t, &TextInputInterface::requestHideInputPanel, this, &InputMethods::hide);
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
                } else {
                    m_waylandShowConnection = QMetaObject::Connection();
                    m_waylandHideConnection = QMetaObject::Connection();
                    m_waylandHintsConnection = QMetaObject::Connection();
                    m_waylandSurroundingTextConnection = QMetaObject::Connection();
                    m_waylandResetConnection = QMetaObject::Connection();
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
        }
    );
}

void InputMethods::show()
{
    if (m_inputWindow.isNull()) {
        return;
    }
    m_inputWindow->setGeometry(screens()->geometry(screens()->current()));
    qApp->inputMethod()->show();
}

void InputMethods::hide()
{
    if (m_inputWindow.isNull()) {
        return;
    }
    qApp->inputMethod()->hide();
}

bool InputMethods::event(QEvent *e)
{
    if (e->type() == QEvent::InputMethod) {
        QInputMethodEvent *event = static_cast<QInputMethodEvent*>(e);
        if (waylandServer()) {
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
            if (TextInputInterface *ti = waylandServer()->seat()->textInput()) {
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
        if (waylandServer()) {
            ti = waylandServer()->seat()->textInput();
        }
        if (event->queries().testFlag(Qt::ImEnabled)) {
            event->setValue(Qt::ImEnabled, QVariant(ti != nullptr));
        }
        if (event->queries().testFlag(Qt::ImCursorRectangle)) {
        }
        if (event->queries().testFlag(Qt::ImFont)) {
        }
        if (event->queries().testFlag(Qt::ImCursorPosition)) {
            if (ti && ti->version() != TextInputInterface::InterfaceVersion::UnstableV0) {
                event->setValue(Qt::ImCursorPosition, ti->surroundingTextCursorPosition());
            } else {
                event->setValue(Qt::ImCursorPosition, 0);
            }
        }
        if (event->queries().testFlag(Qt::ImSurroundingText)) {
            if (ti && ti->version() != TextInputInterface::InterfaceVersion::UnstableV0) {
                event->setValue(Qt::ImSurroundingText, ti->surroundingText());
            } else {
                event->setValue(Qt::ImSurroundingText, QString());
            }
        }
        if (event->queries().testFlag(Qt::ImCurrentSelection)) {
        }
        if (event->queries().testFlag(Qt::ImMaximumTextLength)) {
        }
        if (event->queries().testFlag(Qt::ImAnchorPosition)) {
            if (ti && ti->version() != TextInputInterface::InterfaceVersion::UnstableV0) {
                event->setValue(Qt::ImAnchorPosition, ti->surroundingTextSelectionAnchor());
            } else {
                event->setValue(Qt::ImAnchorPosition, 0);
            }
        }
        if (event->queries().testFlag(Qt::ImHints)) {
            if (ti) {
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
                if (contentHints.testFlag(TextInputInterface::ContentHint::Lowercase)) {
                    hints |= Qt::ImhPreferLowercase;
                }
                if (contentHints.testFlag(TextInputInterface::ContentHint::Uppercase)) {
                    hints |= Qt::ImhPreferUppercase;
                }
                if (contentHints.testFlag(TextInputInterface::ContentHint::Titlecase)) {
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
                if (contentHints.testFlag(TextInputInterface::ContentHint::Multiline)) {
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
                case TextInputInterface::ContentPurpose::Email:
                    hints |= Qt::ImhEmailCharactersOnly;
                    break;
                case TextInputInterface::ContentPurpose::Date:
                    hints |= Qt::ImhDate;
                    break;
                case TextInputInterface::ContentPurpose::Time:
                    hints |= Qt::ImhTime;
                    break;
                case TextInputInterface::ContentPurpose::Datetime:
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
                event->setValue(Qt::ImHints, QVariant::fromValue(hints));
            } else {
                event->setValue(Qt::ImHints, Qt::ImhNone);
            }
        }
        if (event->queries().testFlag(Qt::ImPreferredLanguage)) {
            // TODO: What does Qt want here?
        }
        if (event->queries().testFlag(Qt::ImPlatformData)) {
        }
        if (event->queries().testFlag(Qt::ImAbsolutePosition)) {
        }
        if (event->queries().testFlag(Qt::ImTextBeforeCursor)) {
            if (ti && ti->version() != TextInputInterface::InterfaceVersion::UnstableV0) {
                event->setValue(Qt::ImTextBeforeCursor, ti->surroundingText().left(ti->surroundingTextCursorPosition()));
            } else {
                event->setValue(Qt::ImTextBeforeCursor, QString());
            }
        }
        if (event->queries().testFlag(Qt::ImTextAfterCursor)) {
            if (ti && ti->version() != TextInputInterface::InterfaceVersion::UnstableV0) {
                event->setValue(Qt::ImTextAfterCursor, ti->surroundingText().mid(ti->surroundingTextCursorPosition()));
            } else {
                event->setValue(Qt::ImTextAfterCursor, QString());
            }
        }
        event->accept();
        return true;
    }
    return QObject::event(e);
}

bool InputMethods::eventFilter(QObject *o, QEvent *e)
{
    if (o != m_inputWindow.data() || !m_inputWindow->isVisible()) {
        return false;
    }
    if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) {
        QKeyEvent *event = static_cast<QKeyEvent*>(e);
        if (event->nativeScanCode() == 0) {
            // this is a key composed by the virtual keyboard - we need to send it to the client
            // TODO: proper xkb support in KWindowSystem needed
            int sym = 0;
            KKeyServer::keyQtToSymX(event->key(), &sym);
            if (sym != 0) {
                if (waylandServer()) {
                    if (auto t = waylandServer()->seat()->textInput()) {
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

QWindow *InputMethods::inputPanel() const
{
    return m_inputWindow.data();
}

}
