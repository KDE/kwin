/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "input.h"
#include "xkb.h"

#include <QObject>
#include <QPointF>
#include <QPointer>

#include <KSharedConfig>

class QWindow;

namespace KWin
{

class Window;
class InputDevice;
class InputRedirection;
class KeyboardLayout;
class ModifiersChangedSpy;
class KeyboardRepeat;
class KeyStateChangedSpy;

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
    void processKeySym(uint32_t sym, KeyboardKeyState state, std::chrono::microseconds time, InputDevice *device = nullptr);

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
    void processKeyInternal(uint32_t key, KeyboardKeyState state, std::chrono::microseconds time, InputDevice *device = nullptr);

    InputRedirection *m_input;
    bool m_inited = false;
    const std::unique_ptr<Xkb> m_xkb;
    QMetaObject::Connection m_activeWindowSurfaceChangedConnection;
    std::unique_ptr<KeyStateChangedSpy> m_keyStateChangedSpy;
    std::unique_ptr<ModifiersChangedSpy> m_modifiersChangedSpy;
    std::unique_ptr<KeyboardLayout> m_keyboardLayout;
    QList<uint32_t> m_pressedKeys;
    QList<uint32_t> m_filteredKeys;
    std::unique_ptr<KeyboardRepeat> m_keyRepeatSpy;
};

}
