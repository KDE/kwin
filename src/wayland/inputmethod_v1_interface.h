/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <QVector>
#include <memory>

#include "textinput.h"

namespace KWaylandServer
{
class OutputInterface;
class SurfaceInterface;
class Display;
class KeyboardInterface;
class InputPanelSurfaceV1Interface;
class InputMethodContextV1Interface;

class InputMethodV1InterfacePrivate;
class InputMethodContextV1InterfacePrivate;
class InputPanelV1InterfacePrivate;
class InputPanelSurfaceV1InterfacePrivate;
class InputMethodGrabV1;
class InputKeyboardV1InterfacePrivate;

enum class KeyboardKeyState : quint32;

// This file's classes implment input_method_unstable_v1

/**
 * Implements zwp_input_method_v1 and allows to activate and deactivate a context
 *
 * When we activate, an @class InputMethodContextV1Interface becomes available
 */
class KWIN_EXPORT InputMethodV1Interface : public QObject
{
    Q_OBJECT
public:
    InputMethodV1Interface(Display *d, QObject *parent);
    ~InputMethodV1Interface() override;

    /**
     * Activates the input method.
     */
    void sendActivate();

    /**
     * Deactivates the input method, probably because we're not on some area
     * where we can write text.
     */
    void sendDeactivate();

    InputMethodContextV1Interface *context() const;

private:
    std::unique_ptr<InputMethodV1InterfacePrivate> d;
};

/**
 * Implements zwp_input_method_context_v1, allows to describe the client's input state
 */
class KWIN_EXPORT InputMethodContextV1Interface : public QObject
{
    Q_OBJECT
public:
    ~InputMethodContextV1Interface() override;

    void sendSurroundingText(const QString &text, quint32 cursor, quint32 anchor);
    void sendReset();
    void sendContentType(KWaylandServer::TextInputContentHints hint, KWaylandServer::TextInputContentPurpose purpose);
    void sendInvokeAction(quint32 button, quint32 index);
    void sendCommitState(quint32 serial);
    void sendPreferredLanguage(const QString &language);

    InputMethodGrabV1 *keyboardGrab() const;

Q_SIGNALS:
    void commitString(quint32 serial, const QString &text);
    void preeditString(quint32 serial, const QString &text, const QString &commit);
    void preeditStyling(quint32 index, quint32 length, quint32 style);
    void preeditCursor(qint32 index);
    void deleteSurroundingText(qint32 index, quint32 length);
    void cursorPosition(qint32 index, qint32 anchor);
    void keysym(quint32 serial, quint32 time, quint32 sym, bool pressed, quint32 modifiers);
    void key(quint32 serial, quint32 time, quint32 key, bool pressed);
    void modifiers(quint32 serial, quint32 mods_depressed, quint32 mods_latched, quint32 mods_locked, quint32 group);
    void language(quint32 serial, const QString &language);
    void textDirection(quint32 serial, Qt::LayoutDirection direction);
    void keyboardGrabRequested(InputMethodGrabV1 *keyboardGrab);
    void modifiersMap(const QByteArray &map);

private:
    friend class InputMethodV1Interface;
    friend class InputMethodV1InterfacePrivate;
    InputMethodContextV1Interface(InputMethodV1Interface *parent);
    std::unique_ptr<InputMethodContextV1InterfacePrivate> d;
};

/**
 * Implements zwp_input_panel_v1, tells us about the InputPanelSurfaceV1Interface that we might get
 */
class KWIN_EXPORT InputPanelV1Interface : public QObject
{
    Q_OBJECT
public:
    InputPanelV1Interface(Display *display, QObject *parent);
    ~InputPanelV1Interface() override;

Q_SIGNALS:
    void inputPanelSurfaceAdded(InputPanelSurfaceV1Interface *surface);

private:
    std::unique_ptr<InputPanelV1InterfacePrivate> d;
};

/**
 * Implements zwp_input_panel_surface_v1, it corresponds to each element shown so it can be placed.
 */
class KWIN_EXPORT InputPanelSurfaceV1Interface : public QObject
{
    Q_OBJECT
public:
    ~InputPanelSurfaceV1Interface() override;

    enum Position {
        CenterBottom = 0,
    };
    Q_ENUM(Position)

    SurfaceInterface *surface() const;

Q_SIGNALS:
    void topLevel(OutputInterface *output, Position position);
    void overlayPanel();

private:
    InputPanelSurfaceV1Interface(SurfaceInterface *surface, quint32 id, QObject *parent);
    friend class InputPanelV1InterfacePrivate;
    std::unique_ptr<InputPanelSurfaceV1InterfacePrivate> d;
};

/**
 * Implements a wl_keyboard tailored for zwp_input_method_v1 use-cases
 */
class KWIN_EXPORT InputMethodGrabV1 : public QObject
{
    Q_OBJECT
public:
    ~InputMethodGrabV1() override;

    void sendKeymap(const QByteArray &content);
    void sendKey(quint32 serial, quint32 timestamp, quint32 key, KeyboardKeyState state);
    void sendModifiers(quint32 serial, quint32 depressed, quint32 latched, quint32 locked, quint32 group);

private:
    InputMethodGrabV1(QObject *parent);
    friend class InputPanelV1InterfacePrivate;
    friend class InputMethodContextV1InterfacePrivate;
    std::unique_ptr<InputKeyboardV1InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::InputMethodV1Interface *)
Q_DECLARE_METATYPE(KWaylandServer::InputMethodGrabV1 *)
