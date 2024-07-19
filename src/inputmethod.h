/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "wayland/textinput_v2.h"

#include <utility>
#include <vector>

#include <QObject>

#include "input_event_spy.h"
#include <kwin_export.h>

#include <QPointer>
#include <QTimer>

class QProcess;

namespace KWin
{

class Window;
class InputPanelV1Window;
class InputMethodGrabV1;
class InternalInputMethodContext;

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
    Window *activeWindow() const;

    InputPanelV1Window *panel() const;
    void setPanel(InputPanelV1Window *panel);
    void setInputMethodCommand(const QString &path);

    InputMethodGrabV1 *keyboardGrab();
    bool shouldShowOnActive() const;

    void forwardModifiers(ForwardModifiersForce force);
    bool activeClientSupportsTextInput() const;
    void forceActivate();

    void commitPendingText();

    // for use by the QPA
    InternalInputMethodContext *internalContext() const
    {
        return m_internalContext;
    }

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
    void textInputInterfaceV2StateUpdated(quint32 serial, KWin::TextInputV2Interface::UpdateReason reason);
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
    void installKeyboardGrab(InputMethodGrabV1 *keyboardGrab);
    void updateModifiersMap(const QByteArray &modifiers);

    bool touchEventTriggered() const;
    void resetPendingPreedit();
    void refreshActive();
    void forwardKeyToEffects(bool pressed, int keyCode, int keySym);

    // buffered till the preedit text is set
    struct
    {
        qint32 cursor = 0;
        std::vector<std::pair<quint32, quint32>> highlightRanges;
    } preedit;

    // In some IM cases pre-edit text should be submitted when a user changes focus. In some it should be discarded
    // TextInputV3 does not have a flag for this, so we have to handle it in the compositor
    QString m_pendingText = QString();

    bool m_enabled = true;
    quint32 m_serial = 0;
    QPointer<InputPanelV1Window> m_panel;
    QPointer<Window> m_trackedWindow;
    QPointer<InputMethodGrabV1> m_keyboardGrab;

    QProcess *m_inputMethodProcess = nullptr;
    QTimer m_inputMethodCrashTimer;
    uint m_inputMethodCrashes = 0;
    QString m_inputMethodCommand;

    InternalInputMethodContext *m_internalContext = nullptr;
    bool m_hasPendingModifiers = false;
    bool m_activeClientSupportsTextInput = false;
    bool m_shouldShowPanel = false;
};

}
