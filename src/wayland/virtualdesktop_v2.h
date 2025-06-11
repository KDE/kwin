/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include "kwin_export.h"

#include <QObject>
#include <memory>

struct wl_resource;

namespace KWin
{
class Display;
class VirtualDesktopManagerV2Private;
class VirtualDesktopV2Private;

/**
 * The VirtualDesktopV2 type represents a kde_virtual_desktop_v2 object.
 */
class KWIN_EXPORT VirtualDesktopV2 : public QObject
{
    Q_OBJECT

public:
    VirtualDesktopV2(const QString &id, const QString &name, uint position, QObject *parent = nullptr);

    void introduce(wl_resource *manager);

    void remove();
    void destroy();

    void setName(const QString &name);
    void setActive(bool active);
    void setPosition(uint position);

Q_SIGNALS:
    /**
     * This signal is emitted when a client wants this desktop to become active. The compositor
     * may ignore this request.
     */
    void activateRequested();

    /**
     * This signal is emitted when a client wants this desktop to be removed. The compositor
     * may ignore this request, for example if it's the only available virtual desktop, etc.
     */
    void removeRequested();

private:
    ~VirtualDesktopV2() override;

    std::unique_ptr<VirtualDesktopV2Private> d;
    friend class VirtualDesktopManagerV2;
    friend class VirtualDesktopManagerV2Private;
};

/**
 * The VirtualDesktopManagerV2 type represents a kde_virtual_desktop_manager_v2 global.
 */
class KWIN_EXPORT VirtualDesktopManagerV2 : public QObject
{
    Q_OBJECT

public:
    explicit VirtualDesktopManagerV2(Display *display, QObject *parent = nullptr);
    ~VirtualDesktopManagerV2() override;

    /**
     * Sets the number of rows to @a rows.
     */
    void setRows(uint rows);

    /**
     * Registers a virtual desktop with the specified @a id, @a name, and @a position.
     */
    VirtualDesktopV2 *add(const QString &id, const QString &name, uint position);

    /**
     * Removes a virtual desktop with the specified @a id.
     */
    void remove(const QString &id);

    QHash<QString, VirtualDesktopV2 *> desktops() const;

    /**
     * Schedules the done event to be sent at the next available moment.
     */
    void scheduleDone();

Q_SIGNALS:
    /**
     * This signal is emitted when a client wants to create a virtual desktop with the given
     * @a name and @a position. The compositor may ignore virtual desktop creation requests.
     */
    void createRequested(const QString &name, uint position);

private:
    std::unique_ptr<VirtualDesktopManagerV2Private> d;
};

} // namespace KWin
