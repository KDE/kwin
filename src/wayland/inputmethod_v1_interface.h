/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef WAYLAND_SERVER_INPUTMETHOD_INTERFACE_H
#define WAYLAND_SERVER_INPUTMETHOD_INTERFACE_H

#include <KWaylandServer/kwaylandserver_export.h>

#include <QVector>
#include <QObject>

#include "textinput.h"

namespace KWaylandServer
{
class OutputInterface;
class SurfaceInterface;
class Display;
class InputPanelSurfaceV1Interface;
class InputMethodContextV1Interface;

class InputMethodV1InterfacePrivate;
class InputMethodContextV1InterfacePrivate;
class InputPanelV1InterfacePrivate;
class InputPanelSurfaceV1InterfacePrivate;

//This file's classes implment input_method_unstable_v1

/**
 * Implements zwp_input_method_v1 and allows to activate and deactivate a context
 *
 * When we activate, an @class InputMethodContextV1Interface becomes available
 */
class KWAYLANDSERVER_EXPORT InputMethodV1Interface : public QObject
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
    QScopedPointer<InputMethodV1InterfacePrivate> d;
};

/**
 * Implements zwp_input_method_context_v1, allows to describe the client's input state
 */
class KWAYLANDSERVER_EXPORT InputMethodContextV1Interface : public QObject
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

Q_SIGNALS:
    void commitString(quint32 serial, const QString &text);
    void preeditString(quint32 serial, const QString &text, const QString &commit);
    void preeditStyling(quint32 index, quint32 length, quint32 style);
    void preeditCursor(qint32 index);
    void deleteSurroundingText(qint32 index, quint32 length);
    void cursorPosition(qint32 index, qint32 anchor);
    void keysym(quint32 serial, quint32 time, quint32 sym, bool pressed, Qt::KeyboardModifiers modifiers);
    void grabKeyboard(quint32 keyboard);
    void key(quint32 serial, quint32 time, quint32 key, bool pressed);
    void modifiers(quint32 serial, quint32 mods_depressed, quint32 mods_latched, quint32 mods_locked, quint32 group);
    void language(quint32 serial, const QString &language);
    void textDirection(quint32 serial, Qt::LayoutDirection direction);

private:
    friend class InputMethodV1Interface;
    friend class InputMethodV1InterfacePrivate;
    InputMethodContextV1Interface(InputMethodV1Interface *parent);
    QScopedPointer<InputMethodContextV1InterfacePrivate> d;
};

/**
 * Implements zwp_input_panel_v1, tells us about the InputPanelSurfaceV1Interface that we might get
 */
class KWAYLANDSERVER_EXPORT InputPanelV1Interface : public QObject
{
    Q_OBJECT
public:
    InputPanelV1Interface(Display *display, QObject *parent);
    ~InputPanelV1Interface() override;

Q_SIGNALS:
    void inputPanelSurfaceAdded(InputPanelSurfaceV1Interface *surface);

private:
    QScopedPointer<InputPanelV1InterfacePrivate> d;
};

/**
 * Implements zwp_input_panel_surface_v1, it corresponds to each element shown so it can be placed.
 */
class KWAYLANDSERVER_EXPORT InputPanelSurfaceV1Interface : public QObject
{
    Q_OBJECT
public:
    ~InputPanelSurfaceV1Interface() override;

    enum Position {
        CenterBottom = 0
    };
    Q_ENUM(Position)

    quint32 id() const;
    SurfaceInterface *surface() const;

Q_SIGNALS:
    void topLevel(OutputInterface *output, Position position);
    void overlayPanel();

private:
    InputPanelSurfaceV1Interface(SurfaceInterface *surface, quint32 id, QObject *parent);
    friend class InputPanelV1InterfacePrivate;
    QScopedPointer<InputPanelSurfaceV1InterfacePrivate> d;
};

}

Q_DECLARE_METATYPE(KWaylandServer::InputMethodV1Interface *)

#endif
