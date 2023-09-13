/*
    SPDX-FileCopyrightText: 2020 Benjamin Port <benjamin.port@enioka.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "seat.h"
#include "surface.h"

#include <QObject>

namespace KWin
{
/**
 * This is an implementation of wayland-protocols/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml
 *
 * This class is just the means to get a @class KeyboardShortcutsInhibitorV1Interface, which is
 * the class that will have all of the information we need.
 */

class KeyboardShortcutsInhibitManagerV1Interface;
class KeyboardShortcutsInhibitorV1InterfacePrivate;
class KeyboardShortcutsInhibitManagerV1InterfacePrivate;

class KWIN_EXPORT KeyboardShortcutsInhibitorV1Interface : public QObject
{
    Q_OBJECT

public:
    ~KeyboardShortcutsInhibitorV1Interface() override;

    SurfaceInterface *surface() const;
    SeatInterface *seat() const;
    void setActive(bool active);
    bool isActive() const;

private:
    friend class KeyboardShortcutsInhibitManagerV1InterfacePrivate;
    explicit KeyboardShortcutsInhibitorV1Interface(SurfaceInterface *surface,
                                                   SeatInterface *seat,
                                                   KeyboardShortcutsInhibitManagerV1Interface *manager,
                                                   wl_resource *resource);
    std::unique_ptr<KeyboardShortcutsInhibitorV1InterfacePrivate> d;
};

/**
 * The KeyboardShortcutsInhibitManagerV1Interface allows clients to inhibit global shortcuts.
 *
 * KeyboardShortcutsInhibitManagerV1Interface correponds to the wayland interface zwp_keyboard_shortcuts_inhibit_manager_v1.
 */
class KWIN_EXPORT KeyboardShortcutsInhibitManagerV1Interface : public QObject
{
    Q_OBJECT

public:
    explicit KeyboardShortcutsInhibitManagerV1Interface(Display *d, QObject *parent = nullptr);
    ~KeyboardShortcutsInhibitManagerV1Interface() override;

    /**
     * return shortucts inhibitor associated with surface and seat, if no shortcut are associated, return nullptr
     */
    KeyboardShortcutsInhibitorV1Interface *findInhibitor(SurfaceInterface *surface, SeatInterface *seat) const;

Q_SIGNALS:
    /**
     * This signal is emitted when a keyboard shortcuts inhibitor @a inhibitor is created.
     */
    void inhibitorCreated(KeyboardShortcutsInhibitorV1Interface *inhibitor);

private:
    friend class KeyboardShortcutsInhibitorV1InterfacePrivate;
    void removeInhibitor(SurfaceInterface *const surface, SeatInterface *const seat);
    std::unique_ptr<KeyboardShortcutsInhibitManagerV1InterfacePrivate> d;
};

}
