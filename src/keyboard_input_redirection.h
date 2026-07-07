/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "config-kwin.h"
#include "core/inputdevice.h"

#include <QObject>

#include <memory>

namespace KWin
{

class InputRedirection;
class KeyboardInput;
class Window;
class Xkb;

class KWIN_EXPORT KeyboardInputRedirection : public QObject
{
    Q_OBJECT

public:
    explicit KeyboardInputRedirection(InputRedirection *input);
    void init();
    void reconfigure();
    bool isInitialized() const;

    /**
     * Returns the currently active keyboard based on last input.
     * It will always have a value
     */
    KeyboardInput *activeKeyboard() const;

    Xkb *xkb() const;
    Qt::KeyboardModifiers modifiers() const;
    Qt::KeyboardModifiers modifiersRelevantForGlobalShortcuts() const;
    QList<uint32_t> pressedKeys() const;
    QList<uint32_t> filteredKeys() const;
    void addFilteredKey(uint32_t key);
    QList<uint32_t> unfilteredKeys() const;
    void setLastKeyboardInputDevice(InputDevice *device, std::chrono::microseconds time);
    void updateKeymap();
    void forwardModifiers();
    Window *pickFocus() const;
    void updateActiveKeyboard();
    void update();

    /**
     * Returns every keybaord used by any input device
     */
    QSet<KeyboardInput *> keyboards() const;

    /**
     * Returns a shared keyboard that is the default shared keybaord for all input devices
     */
    std::shared_ptr<KeyboardInput> globalKeyboard() const;

Q_SIGNALS:
    void layoutChanged();
    void ledsChanged(KWin::LEDs leds);
    void modifierStateChanged();

private:
    InputRedirection *m_input;
    bool m_inited = false;
    std::shared_ptr<KeyboardInput> m_globalKeyboard;
    KeyboardInput *m_trackedKeyboard = nullptr;
    QMetaObject::Connection m_keymapChangedConnection;
    QMetaObject::Connection m_activeWindowSurfaceChangedConnection;
};

}
