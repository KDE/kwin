/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "inputmethod.h"
#include "abstract_client.h"
#include "virtualkeyboard_dbus.h"
#include "input.h"
#include "keyboard_input.h"
#include "utils.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "screenlockerwatcher.h"
#include "deleted.h"

#include <KWaylandServer/display.h>
#include <KWaylandServer/keyboard_interface.h>
#include <KWaylandServer/seat_interface.h>
#include <KWaylandServer/textinput_v3_interface.h>
#include <KWaylandServer/surface_interface.h>
#include <KWaylandServer/inputmethod_v1_interface.h>

#include <KShell>
#include <KStatusNotifierItem>
#include <KLocalizedString>

#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QDBusMessage>
#include <QMenu>

#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <unistd.h>

using namespace KWaylandServer;

namespace KWin
{

KWIN_SINGLETON_FACTORY(InputMethod)

InputMethod::InputMethod(QObject *parent)
    : QObject(parent)
{
    // this is actually too late. Other processes are started before init,
    // so might miss the availability of text input
    // but without Workspace we don't have the window listed at all
    if (workspace()) {
        init();
    } else {
        connect(kwinApp(), &Application::workspaceCreated, this, &InputMethod::init);
    }
}

InputMethod::~InputMethod() = default;

void InputMethod::init()
{
    // Stop restarting the input method if it starts crashing very frequently
    m_inputMethodCrashTimer.setInterval(20000);
    m_inputMethodCrashTimer.setSingleShot(true);
    connect(&m_inputMethodCrashTimer, &QTimer::timeout, this, [this] {
        m_inputMethodCrashes = 0;
    });
    connect(ScreenLockerWatcher::self(), &ScreenLockerWatcher::aboutToLock, this, &InputMethod::hide);

    if (waylandServer()) {
        m_enabled = !input()->hasAlphaNumericKeyboard();
        qCDebug(KWIN_VIRTUALKEYBOARD) << "enabled by default: " << m_enabled;
        connect(input(), &InputRedirection::hasAlphaNumericKeyboardChanged, this,
            [this] (bool set) {
                qCDebug(KWIN_VIRTUALKEYBOARD) << "AlphaNumeric Keyboard changed:" << set << "toggling virtual keyboard.";
                setEnabled(!set);
            }
        );
    }

    qCDebug(KWIN_VIRTUALKEYBOARD) << "Registering the SNI";

    QMenu *sniMenu = new QMenu;
    sniMenu->addAction(i18n("Configure virtual keyboards..."), this, [] {
        QProcess::startDetached("systemsettings5", {"kcm_virtualkeyboard"});
    });

    m_sni = new KStatusNotifierItem(QStringLiteral("kwin-virtual-keyboard"), this);
    m_sni->setStandardActionsEnabled(false);
    m_sni->setCategory(KStatusNotifierItem::Hardware);
    m_sni->setStatus(KStatusNotifierItem::Passive);
    m_sni->setTitle(i18n("Virtual Keyboard"));
    m_sni->setToolTipTitle(i18n("Whether to show the virtual keyboard on demand."));
    m_sni->setStandardActionsEnabled(false);
    m_sni->setContextMenu(sniMenu);
    updateSni();
    connect(m_sni, &KStatusNotifierItem::activateRequested, this,
        [this] {
            setEnabled(!m_enabled);
        }
    );
    connect(this, &InputMethod::enabledChanged, this, &InputMethod::updateSni);

    new VirtualKeyboardDBus(this);
    qCDebug(KWIN_VIRTUALKEYBOARD) << "Registering the DBus interface";

    if (waylandServer()) {
        new TextInputManagerV2Interface(waylandServer()->display());
        new TextInputManagerV3Interface(waylandServer()->display());

        connect(workspace(), &Workspace::clientAdded, this, &InputMethod::clientAdded);
        connect(waylandServer()->seat(), &SeatInterface::focusedTextInputSurfaceChanged, this, &InputMethod::handleFocusedSurfaceChanged);

        TextInputV2Interface *textInputV2 = waylandServer()->seat()->textInputV2();
        connect(textInputV2, &TextInputV2Interface::requestShowInputPanel, this, &InputMethod::show);
        connect(textInputV2, &TextInputV2Interface::requestHideInputPanel, this, &InputMethod::hide);
        connect(textInputV2, &TextInputV2Interface::surroundingTextChanged, this, &InputMethod::surroundingTextChanged);
        connect(textInputV2, &TextInputV2Interface::contentTypeChanged, this, &InputMethod::contentTypeChanged);
        connect(textInputV2, &TextInputV2Interface::stateUpdated, this, &InputMethod::textInputInterfaceV2StateUpdated);

        TextInputV3Interface *textInputV3 = waylandServer()->seat()->textInputV3();
        connect(textInputV3, &TextInputV3Interface::surroundingTextChanged, this, &InputMethod::surroundingTextChanged);
        connect(textInputV3, &TextInputV3Interface::contentTypeChanged, this, &InputMethod::contentTypeChanged);
        connect(textInputV3, &TextInputV3Interface::stateCommitted, this, &InputMethod::stateCommitted);

        if (m_enabled) {
            connect(textInputV2, &TextInputV2Interface::enabledChanged, this, &InputMethod::textInputInterfaceV2EnabledChanged);
            connect(textInputV3, &TextInputV3Interface::enabledChanged, this, &InputMethod::textInputInterfaceV3EnabledChanged);
        }
    }
}

void InputMethod::show()
{
    setActive(true);
}

void InputMethod::hide()
{
    setActive(false);
}

void InputMethod::setActive(bool active)
{
    bool wasActive = waylandServer()->inputMethod()->context();
    waylandServer()->inputMethod()->sendDeactivate();

    if (active) {
        if (!m_enabled) {
            return;
        }

        waylandServer()->inputMethod()->sendActivate();
        adoptInputMethodContext();
    } else {
        updateInputPanelState();
    }

    if (wasActive != isActive()) {
        Q_EMIT activeChanged(active);
    }
}

void InputMethod::clientAdded(AbstractClient* client)
{
    if (!client->isInputMethod()) {
        return;
    }
    m_inputClient = client;
    connect(client->surface(), &SurfaceInterface::inputChanged, this, &InputMethod::updateInputPanelState);
    connect(client, &QObject::destroyed, this, [this] {
        if (m_trackedClient) {
            m_trackedClient->setVirtualKeyboardGeometry({});
        }
    });
    connect(m_inputClient, &AbstractClient::frameGeometryChanged, this, &InputMethod::updateInputPanelState);
    // Current code have a assumption that InputMethod started by the kwin is virtual keyboard,
    // InputMethod::hide sends out a deactivate signal to input-method client, this is not desired
    // when we support input methods like ibus which can show and hide surfaces/windows as they please
    // and are not exactly Virtual keyboards.
    connect(m_inputClient, &AbstractClient::windowHidden, this, &InputMethod::hide);
    connect(m_inputClient, &AbstractClient::windowClosed, this, &InputMethod::hide);
}

void InputMethod::setTrackedClient(AbstractClient* trackedClient)
{
    // Reset the old client virtual keybaord geom if necessary
    // Old and new clients could be the same if focus moves between subsurfaces
    if (m_trackedClient == trackedClient) {
        return;
    }
    if (m_trackedClient) {
        m_trackedClient->setVirtualKeyboardGeometry(QRect());
        disconnect(m_trackedClient, &AbstractClient::frameGeometryChanged, this, &InputMethod::updateInputPanelState);
    }
    m_trackedClient = trackedClient;
    if (m_trackedClient) {
        connect(m_trackedClient, &AbstractClient::frameGeometryChanged, this, &InputMethod::updateInputPanelState, Qt::QueuedConnection);
    }
    updateInputPanelState();
}

void InputMethod::handleFocusedSurfaceChanged()
{
    SurfaceInterface *focusedSurface = waylandServer()->seat()->focusedTextInputSurface();
    setTrackedClient(waylandServer()->findClient(focusedSurface));
    if (!focusedSurface) {
        setActive(false);
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
}

void InputMethod::contentTypeChanged()
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

void InputMethod::textInputInterfaceV2StateUpdated(quint32 serial, KWaylandServer::TextInputV2Interface::UpdateReason reason)
{
    if (!m_enabled) {
        return;
    }
    Q_UNUSED(serial);

    auto t2 = waylandServer()->seat()->textInputV2();
    auto inputContext = waylandServer()->inputMethod()->context();
    if (!inputContext) {
        return;
    }
    if (!t2 || !t2->isEnabled()) {
        return;
    }
    switch (reason) {
    case KWaylandServer::TextInputV2Interface::UpdateReason::StateChange:
        break;
    case KWaylandServer::TextInputV2Interface::UpdateReason::StateEnter:
    case KWaylandServer::TextInputV2Interface::UpdateReason::StateFull:
        adoptInputMethodContext();
        break;
    case KWaylandServer::TextInputV2Interface::UpdateReason::StateReset:
        inputContext->sendReset();
        break;
    }
}

void InputMethod::textInputInterfaceV2EnabledChanged()
{
    if (!m_enabled) {
        return;
    }

    auto t = waylandServer()->seat()->textInputV2();
    setActive(t->isEnabled());
}

void InputMethod::textInputInterfaceV3EnabledChanged()
{
    if (!m_enabled) {
        return;
    }

    auto t3 = waylandServer()->seat()->textInputV3();
    setActive(t3->isEnabled());
    if (!t3->isEnabled()) {
        // reset value of preedit when textinput is disabled
        preedit.text = QString();
        preedit.begin = 0;
        preedit.end = 0;
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
    setActive(textInputV3->isEnabled());
}

void InputMethod::setEnabled(bool enabled)
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
    if (!m_enabled) {
        hide();
        stopInputMethod();
    } else {
        startInputMethod();
    }
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

void InputMethod::keysymReceived(quint32 serial, quint32 time, quint32 sym, bool pressed, Qt::KeyboardModifiers modifiers)
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
        KWaylandServer::KeyboardKeyState state;
        if (pressed) {
            state = KWaylandServer::KeyboardKeyState::Pressed;
        } else {
            state = KWaylandServer::KeyboardKeyState::Released;
        }
        waylandServer()->seat()->notifyKeyboardKey(keysymToKeycode(sym), state);
        return;
    }
}

void InputMethod::commitString(qint32 serial, const QString &text)
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

void InputMethod::deleteSurroundingText(int32_t index, uint32_t length)
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

void InputMethod::setCursorPosition(qint32 index, qint32 anchor)
{
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->setCursorPosition(index, anchor);
    }
}

void InputMethod::setLanguage(uint32_t serial, const QString &language)
{
    Q_UNUSED(serial)
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->setLanguage(language.toUtf8());
    }
}

void InputMethod::setTextDirection(uint32_t serial, Qt::LayoutDirection direction)
{
    Q_UNUSED(serial)
    auto t2 = waylandServer()->seat()->textInputV2();
    if (t2 && t2->isEnabled()) {
        t2->setTextDirection(direction);
    }
}

void InputMethod::setPreeditCursor(qint32 index)
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


void InputMethod::setPreeditString(uint32_t serial, const QString &text, const QString &commit)
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

void InputMethod::adoptInputMethodContext()
{
    auto inputContext = waylandServer()->inputMethod()->context();

    TextInputV2Interface *t2 = waylandServer()->seat()->textInputV2();
    TextInputV3Interface *t3 = waylandServer()->seat()->textInputV3();

    if (t2 && t2->isEnabled()) {
        inputContext->sendSurroundingText(t2->surroundingText(), t2->surroundingTextCursorPosition(), t2->surroundingTextSelectionAnchor());
        inputContext->sendPreferredLanguage(t2->preferredLanguage());
        inputContext->sendContentType(t2->contentHints(), t2->contentPurpose());
        connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::language, this, &InputMethod::setLanguage);
        connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::textDirection, this, &InputMethod::setTextDirection);
    }

    if (t3 && t3->isEnabled()) {
        inputContext->sendSurroundingText(t3->surroundingText(), t3->surroundingTextCursorPosition(), t3->surroundingTextSelectionAnchor());
        inputContext->sendContentType(t3->contentHints(), t3->contentPurpose());
    }
    inputContext->sendCommitState(m_serial++);

    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::keysym, this, &InputMethod::keysymReceived, Qt::UniqueConnection);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::commitString, this, &InputMethod::commitString, Qt::UniqueConnection);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::deleteSurroundingText, this, &InputMethod::deleteSurroundingText, Qt::UniqueConnection);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::cursorPosition, this, &InputMethod::setCursorPosition, Qt::UniqueConnection);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::preeditString, this, &InputMethod::setPreeditString, Qt::UniqueConnection);
    connect(inputContext, &KWaylandServer::InputMethodContextV1Interface::preeditCursor, this, &InputMethod::setPreeditCursor, Qt::UniqueConnection);
}

void InputMethod::updateSni()
{
    if (!m_sni) {
        return;
    }
    if (m_inputMethodCommand.isEmpty()) {
        m_sni->setIconByName(QStringLiteral("input-keyboard-virtual-on"));
        m_sni->setOverlayIconByName(QStringLiteral("emblem-unavailable"));
        m_sni->setTitle(i18n("No Virtual Keyboard configured"));
    } else if (m_enabled) {
        m_sni->setIconByName(QStringLiteral("input-keyboard-virtual-on"));
        m_sni->setTitle(i18n("Virtual Keyboard: enabled"));
        m_sni->setOverlayIconByName({});
    } else {
        m_sni->setIconByName(QStringLiteral("input-keyboard-virtual-off"));
        m_sni->setTitle(i18n("Virtual Keyboard: disabled"));
        m_sni->setOverlayIconByName({});
    }
}

void InputMethod::updateInputPanelState()
{
    if (!waylandServer()) {
        return;
    }

    auto t = waylandServer()->seat()->textInputV2();

    if (!t) {
        return;
    }

    QRect overlap = QRect(0, 0, 0, 0);
    if (m_trackedClient) {
        m_trackedClient->setVirtualKeyboardGeometry(m_inputClient ? m_inputClient->inputGeometry() : QRect());

        if (m_inputClient) {
            overlap = m_trackedClient->frameGeometry() & m_inputClient->inputGeometry();
            overlap.moveTo(m_trackedClient->mapToLocal(overlap.topLeft()));
        }
    }
    t->setInputPanelState(m_inputClient && m_inputClient->isShown(false), overlap);
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
    updateSni();
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
    if (waylandServer()) {
        waylandServer()->destroyInputMethodConnection();
    }
    m_inputMethodProcess->deleteLater();
    m_inputMethodProcess = nullptr;
}

void InputMethod::startInputMethod()
{
    stopInputMethod();
    if (m_inputMethodCommand.isEmpty() || kwinApp()->isTerminating()) {
        return;
    }

    connect(waylandServer(), &WaylandServer::terminatingInternalClientConnection, this, &InputMethod::stopInputMethod, Qt::UniqueConnection);

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
    environment.insert(QStringLiteral("WAYLAND_SOCKET"), QByteArray::number(socket));
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
    environment.remove("DISPLAY");
    environment.remove("WAYLAND_DISPLAY");
    environment.remove("XAUTHORITY");

    m_inputMethodProcess = new Process(this);
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

}

