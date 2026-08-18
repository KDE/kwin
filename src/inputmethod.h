/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2026 Kristen McWilliam <kristen@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input_event.h"
#include "wayland/textinput_v2.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <utility>
#include <vector>

class QProcess;

namespace KWin
{

class Window;
class InputPanelV1Window;
class InputMethodGrabV1;
class InternalInputMethodContext;

/*!
 * \class KWin::InputMethod
 * \inmodule KWin
 * \inheaderfile inputmethod.h
 *
 * \brief Integration with an input method.
 *
 * Manages the compositor-side integration for zwp_input_method_unstable_v1,
 * enabling an input method to be used for text input in KWin, as well as the
 * ability to display an on-screen keyboard and overlay panels.
 */
class KWIN_EXPORT InputMethod : public QObject
{
    Q_OBJECT

public:
    enum ForwardModifiersForce {
        NoForce = 0,
        Force = 1,
    };

    /*!
     * The visibility mode of the on-screen keyboard.
     *
     * \value Never
     *        The on-screen keyboard is never shown automatically.
     * \value NonMouseInput
     *        The on-screen keyboard is shown automatically for non-mouse
     *        input devices (e.g., touch, stylus) when a text input field is focused.
     * \value AnyInput
     *        The on-screen keyboard is shown automatically when any input
     *        device (e.g., mouse, touch, stylus) focuses a text input field.
     */
    enum class VirtualKeyboardVisibility {
        Never,
        NonMouseInput,
        AnyInput,
    };
    Q_ENUM(VirtualKeyboardVisibility);

    InputMethod();
    ~InputMethod() override;

    void init();

    void setMode(VirtualKeyboardVisibility mode);
    VirtualKeyboardVisibility mode() const
    {
        return m_virtualKeyboardVisibility;
    }

    /*!
     * The input method is considered enabled if the on-screen keyboard is configured to show automatically.
     */
    bool isEnabled() const
    {
        return m_virtualKeyboardVisibility != VirtualKeyboardVisibility::Never;
    }

    /*!
     * Considered active if the focused client has a Wayland text-input
     * enabled and the input method is currently connected to it.
     *
     * This does not necessarily mean that the on-screen keyboard is visible.
     */
    bool isActive() const;

    /*!
     * Instruct the input method to (de)activate itself for the currently focused client.
     */
    void setActive(bool active);

    /*!
     * Hides the on-screen keyboard and any overlay panels, if they are currently visible.
     */
    void hide();

    /*!
     * Show the on-screen keyboard (but only if the input method has something to
     * display).
     *
     * This method is more of a "you are allowed to show yourself" than a "please show yourself" request.
     */
    void show();

    /*!
     * Whether the on-screen keyboard is currently visible.
     */
    bool isVisible() const;

    /*!
     * Whether an input method has been configured and is available for use.
     */
    bool isAvailable() const;

    /*!
     * The window that is currently receiving input from the input method.
     *
     * Returns nullptr if no window is currently connected to the input method.
     */
    Window *activeWindow() const;

    /*!
     * The window for the on-screen keyboard and overlay panels.
     */
    InputPanelV1Window *panel() const;

    /*!
     * Installs a new input method panel window, replacing any existing one.
     */
    void setPanel(InputPanelV1Window *panel);

    /*!
     * Sets the command to launch the input method process.
     *
     * If an input method process is already running, it will be terminated and the new
     * command will be used to launch a new process.
     */
    void setInputMethodCommand(const QString &path);

    /*!
     * Returns the currently installed keyboard grab, if any.
     */
    InputMethodGrabV1 *keyboardGrab();

    /*!
     * Whether the on-screen keyboard should be shown automatically when a text input
     * field is focused, based on the current visibility \l mode and the last input device used.
     */
    bool shouldShowOnActive() const;

    /*!
     * Sync the keyboard modifiers state to the active input method keyboard grab, if any.
     */
    void forwardModifiers(ForwardModifiersForce force);

    /*!
     * Whether the currently focused client supports Wayland's text-input protocol (v2 or v3)
     * and can receive text input from the input method.
     */
    bool activeClientSupportsTextInput() const;

    /*!
     * Request the input method to activate itself, even if the currently focused client
     * does not support text input.
     *
     * Does not guarantee that the input method will actually show itself.
     */
    void forceActivate();

    /**
     * Injects text directly to the focused text-input; meant for sources other than the
     * input method (e.g., libei).
     */
    void sendText(const QString &text);

    /*!
     * Commits any pending preedit text to the focused text input.
     */
    void commitPendingText();

    // for use by the QPA
    InternalInputMethodContext *internalContext() const
    {
        return m_internalContext;
    }

    /*!
     * The location of the text input cursor, in global coordinates, relative to the output geometry.
     */
    RectF cursorRectangle() const;

Q_SIGNALS:

    /*!
     * Emitted when the input method panel window has been added or replaced.
     */
    void panelChanged();

    /*!
     * Emitted when the input method becomes active or inactive.
     */
    void activeChanged(bool active);

    /*!
     * Emitted when the visibility \l InputMethod::mode of the on-screen keyboard changes.
     */
    void modeChanged(VirtualKeyboardVisibility mode);

    /*!
     * Emitted when the on-screen keyboard is shown or hidden.
     */
    void visibleChanged();

    /*!
     * Emitted when the configured input method changes.
     */
    void availableChanged();

    /*!
     * Emitted when the state of whether the currently focused client supports text input changes.
     */
    void activeClientSupportsTextInputChanged();

    /*!
     * Emitted when the location of the text input cursor changes.
     */
    void cursorRectangleChanged();

    /*!
     * Emitted when the active window (which supports text input) changes.
     */
    void activeWindowChanged();

private Q_SLOTS:
    // textinput interface slots
    void handleFocusedSurfaceChanged();
    void surroundingTextChanged();
    void contentTypeChanged();
    void textInputInterfaceV2EnabledChanged();
    void textInputInterfaceV3EnabledChanged();
    void stateCommitted(uint32_t serial);
    void textInputInterfaceV2StateUpdated(quint32 serial, KWin::TextInputV2Interface::UpdateReason reason);
    void textInputInterfaceV3EnableRequested();

    // inputcontext slots
    void setPreeditString(uint32_t serial, const QString &text, const QString &commit);
    void setPreeditStyling(quint32 index, quint32 length, quint32 style);
    void setPreeditCursor(qint32 index);
    void key(quint32 serial, quint32 time, quint32 key, KWin::KeyboardKeyState state);
    void modifiers(quint32 serial, quint32 mods_depressed, quint32 mods_latched, quint32 mods_locked, quint32 group);

private:
    void updateInputPanelState();
    void adoptInputMethodContext();
    void commitString(quint32 serial, const QString &text);
    void keysymReceived(quint32 serial, quint32 time, quint32 sym, KeyboardKeyState state, quint32 modifiers);
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
    void forwardKeyToEffects(KWin::KeyboardKeyState state, int keyCode, int keySym);
    void forwardKeySym(int keySym);

    // buffered till the preedit text is set
    struct
    {
        qint32 cursor = 0;
        std::vector<std::pair<quint32, quint32>> highlightRanges;
    } preedit;

    // In some IM cases pre-edit text should be submitted when a user changes focus. In some it should be discarded
    // TextInputV3 does not have a flag for this, so we have to handle it in the compositor
    QString m_pendingText = QString();

    VirtualKeyboardVisibility m_virtualKeyboardVisibility = VirtualKeyboardVisibility::NonMouseInput;
    quint32 m_serial = 0;
    QPointer<InputPanelV1Window> m_panel;

    /*!
     * The window that is currently receiving input from the input method.
     */
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
