/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input.h"

#include <QObject>
#include <QPointF>
#include <QPointer>

#include <KSharedConfig>

class QWindow;
struct xkb_context;
struct xkb_keymap;
struct xkb_state;
struct xkb_compose_table;
struct xkb_compose_state;
typedef uint32_t xkb_mod_index_t;
typedef uint32_t xkb_led_index_t;
typedef uint32_t xkb_keysym_t;
typedef uint32_t xkb_layout_index_t;

namespace KWin
{

class Window;
class InputDevice;
class InputRedirection;
class KeyboardLayout;
class ModifiersChangedSpy;
class Xkb;

class KWIN_EXPORT KeyboardInputRedirection : public QObject
{
    Q_OBJECT
public:
    explicit KeyboardInputRedirection(InputRedirection *parent);
    ~KeyboardInputRedirection() override;

    void init();
    void reconfigure();

    void update();

    /**
     * @internal
     */
    void processKey(uint32_t key, KeyboardKeyState state, std::chrono::microseconds time, InputDevice *device = nullptr);

    Xkb *xkb() const;
    Qt::KeyboardModifiers modifiers() const;
    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts() const;
    KeyboardLayout *keyboardLayout() const;
    QList<uint32_t> pressedKeys() const;

    /**
     * Keys that have been captured by input event filters and should not be reported to clients.
     */
    QList<uint32_t> filteredKeys() const;
    void addFilteredKey(uint32_t key);

    /**
     * Pressed keys without the filtered keys.
     */
    QList<uint32_t> unfilteredKeys() const;

Q_SIGNALS:
    void ledsChanged(KWin::LEDs);

private:
    Window *pickFocus() const;

    InputRedirection *m_input;
    bool m_inited = false;
    const std::unique_ptr<Xkb> m_xkb;
    QMetaObject::Connection m_activeWindowSurfaceChangedConnection;
    ModifiersChangedSpy *m_modifiersChangedSpy = nullptr;
    KeyboardLayout *m_keyboardLayout = nullptr;
    QList<uint32_t> m_pressedKeys;
    QList<uint32_t> m_filteredKeys;
};

}
