/*
    SPDX-FileCopyrightText: 2020 Benjamin Port <benjamin.port@enioka.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef WAYLAND_SERVER_KEYBOARD_SHORTCUTS_INHIBIT_INTERFACE_H
#define WAYLAND_SERVER_KEYBOARD_SHORTCUTS_INHIBIT_INTERFACE_H

#include <KWaylandServer/kwaylandserver_export.h>
#include <QObject>
#include "seat_interface.h"
#include "surface_interface.h"

namespace KWaylandServer
{
/**
 * This is an implementation of wayland-protocols/unstable/keyboard-shortcuts-inhibit/keyboard-shortcuts-inhibit-unstable-v1.xml
 *
 * This class is just the means to get a @class KeyboardShortcutsInhibitorInterface, which is
 * the class that will have all of the information we need.
 */

class KeyboardShortcutsInhibitManagerInterface;
class KeyboardShortcutsInhibitorInterfacePrivate;
class KeyboardShortcutsInhibitManagerInterfacePrivate;


class KWAYLANDSERVER_EXPORT KeyboardShortcutsInhibitorInterface : public QObject
{
    Q_OBJECT

public:
    ~KeyboardShortcutsInhibitorInterface() override;

    SurfaceInterface *surface() const;
    SeatInterface *seat() const;
    void setActive(bool active);
    bool isActive() const;
private:
    friend class KeyboardShortcutsInhibitManagerInterfacePrivate;
    explicit KeyboardShortcutsInhibitorInterface(SurfaceInterface *surface, SeatInterface *seat, KeyboardShortcutsInhibitManagerInterface *manager, wl_resource *resource);
    QScopedPointer<KeyboardShortcutsInhibitorInterfacePrivate> d;
};

/**
 * The KeyboardShortcutsInhibitManagerInterface allows clients to inhibit global shortcuts.
 *
 * KeyboardShortcutsInhibitManagerInterface correponds to the wayland interface zwp_keyboard_shortcuts_inhibit_manager_v1.
 *
 * @since 5.20
 */
class KWAYLANDSERVER_EXPORT KeyboardShortcutsInhibitManagerInterface : public QObject
{
    Q_OBJECT

public:
    ~KeyboardShortcutsInhibitManagerInterface() override;

    /**
     * return shortucts inhibitor associated with surface and seat, if no shortcut are associated, return nullptr
     */
    KeyboardShortcutsInhibitorInterface *findInhibitor(SurfaceInterface *surface, SeatInterface *seat) const;

Q_SIGNALS:
    /**
     * This signal is emitted when a keyboard shortcuts inhibitor @a inhibitor is created.
     */
    void inhibitorCreated(KeyboardShortcutsInhibitorInterface *inhibitor);

private:
    friend class Display;
    friend class KeyboardShortcutsInhibitorInterfacePrivate;
    explicit KeyboardShortcutsInhibitManagerInterface(Display *d, QObject *parent = nullptr);
    void removeInhibitor(SurfaceInterface *const surface, SeatInterface *const seat);
    QScopedPointer<KeyboardShortcutsInhibitManagerInterfacePrivate> d;
};

}

#endif
