/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "wayland/textinput_v2_interface.h"

#include <utility>
#include <vector>

#include <QObject>

#include <kwin_export.h>
#include <kwinglobals.h>

#include <QPointer>
#include <QTimer>

class QProcess;

namespace KWaylandServer
{
class InputMethodGrabV1;
}

namespace KWin
{

class Window;
class InputPanelV1Window;

/**
 * This class implements the zwp_input_method_unstable_v1, which is currently used to provide
 * the Virtual Keyboard using supported input method client (maliit-keyboard e.g.)
 **/
class KWIN_EXPORT InputMethod : public QObject
{
    Q_OBJECT
public:
    enum ForwardModifiersForce {
        NoForce = 0,
        Force = 1,
    };

    InputMethod();
    ~InputMethod() override;

    void init();
    void setEnabled(bool enable);
    bool isEnabled() const
    {
        return m_enabled;
    }
    bool isActive() const;
    void setActive(bool active);
    void hide();
    void show();
    bool isVisible() const;
    bool isAvailable() const;

    InputPanelV1Window *panel() const;
    void setPanel(InputPanelV1Window *panel);
    void setInputMethodCommand(const QString &path);

    KWaylandServer::InputMethodGrabV1 *keyboardGrab();
    bool shouldShowOnActive() const;

    void forwardModifiers(ForwardModifiersForce force);
    bool activeClientSupportsTextInput() const;
    void forceActivate();

Q_SIGNALS:
    void panelChanged();
    void activeChanged(bool active);
    void enabledChanged(bool enabled);
    void visibleChanged();
    void availableChanged();
    void activeClientSupportsTextInputChanged();

private Q_SLOTS:
    // textinput interface slots
    void handleFocusedSurfaceChanged();
    void surroundingTextChanged();
    void contentTypeChanged();
    void textInputInterfaceV1EnabledChanged();
    void textInputInterfaceV2EnabledChanged();
    void textInputInterfaceV3EnabledChanged();
    void stateCommitted(uint32_t serial);
    void textInputInterfaceV1StateUpdated(quint32 serial);
    void textInputInterfaceV1Reset();
    void invokeAction(quint32 button, quint32 index);
    void textInputInterfaceV2StateUpdated(quint32 serial, KWaylandServer::TextInputV2Interface::UpdateReason reason);
    void textInputInterfaceV3EnableRequested();

    // inputcontext slots
    void setPreeditString(uint32_t serial, const QString &text, const QString &commit);
    void setPreeditStyling(quint32 index, quint32 length, quint32 style);
    void setPreeditCursor(qint32 index);
    void key(quint32 serial, quint32 time, quint32 key, bool pressed);
    void modifiers(quint32 serial, quint32 mods_depressed, quint32 mods_latched, quint32 mods_locked, quint32 group);

private:
    void updateInputPanelState();
    void adoptInputMethodContext();
    void commitString(qint32 serial, const QString &text);
    void keysymReceived(quint32 serial, quint32 time, quint32 sym, bool pressed, quint32 modifiers);
    void deleteSurroundingText(int32_t index, uint32_t length);
    void setCursorPosition(qint32 index, qint32 anchor);
    void setLanguage(uint32_t serial, const QString &language);
    void setTextDirection(uint32_t serial, Qt::LayoutDirection direction);
    void startInputMethod();
    void stopInputMethod();
    void setTrackedWindow(Window *trackedWindow);
    void installKeyboardGrab(KWaylandServer::InputMethodGrabV1 *keyboardGrab);
    void updateModifiersMap(const QByteArray &modifiers);

    bool touchEventTriggered() const;
    void resetPendingPreedit();
    void refreshActive();

    struct
    {
        QString text = QString();
        qint32 cursor = 0;
        std::vector<std::pair<quint32, quint32>> highlightRanges;
    } preedit;

    bool m_enabled = true;
    quint32 m_serial = 0;
    QPointer<InputPanelV1Window> m_panel;
    QPointer<Window> m_trackedWindow;
    QPointer<KWaylandServer::InputMethodGrabV1> m_keyboardGrab;

    QProcess *m_inputMethodProcess = nullptr;
    QTimer m_inputMethodCrashTimer;
    uint m_inputMethodCrashes = 0;
    QString m_inputMethodCommand;

    bool m_hasPendingModifiers = false;
    bool m_activeClientSupportsTextInput = false;
    bool m_shouldShowPanel = false;
};

}
