/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_UNMANAGED_H
#define KWIN_UNMANAGED_H

#include <netwm.h>

#include "toplevel.h"

namespace KWin
{

class KWIN_EXPORT Unmanaged : public Toplevel
{
    Q_OBJECT
public:
    explicit Unmanaged();
    bool windowEvent(xcb_generic_event_t *e);
    bool track(xcb_window_t w);
    bool hasScheduledRelease() const;
    static void deleteUnmanaged(Unmanaged* c);
    QRect bufferGeometry() const override;
    int desktop() const override;
    QStringList activities() const override;
    QVector<VirtualDesktop *> desktops() const override;
    QPoint clientPos() const override;
    QRect transparentRect() const override;
    Layer layer() const override {
        return UnmanagedLayer;
    }
    NET::WindowType windowType(bool direct = false, int supported_types = 0) const override;
    bool isOutline() const override;

    bool setupCompositing() override;

public Q_SLOTS:
    void release(ReleaseReason releaseReason = ReleaseReason::Release);
protected:
    void addDamage(const QRegion &damage) override;
private:
    ~Unmanaged() override; // use release()
    // handlers for X11 events
    void configureNotifyEvent(xcb_configure_notify_event_t *e);
    QWindow *findInternalWindow() const;
    bool m_outline = false;
    bool m_scheduledRelease = false;
};

} // namespace

#endif
