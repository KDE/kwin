/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_SCREENS_XRANDR_H
#define KWIN_SCREENS_XRANDR_H
// kwin
#include "outputscreens.h"
#include "x11eventfilter.h"

namespace KWin
{
class X11StandalonePlatform;

class XRandRScreens : public OutputScreens, public X11EventFilter
{
    Q_OBJECT
public:
    XRandRScreens(X11StandalonePlatform *backend, QObject *parent = nullptr);
    ~XRandRScreens() override;
    void init() override;

    QSize displaySize() const override;

    using QObject::event;
    bool event(xcb_generic_event_t *event) override;

private:
    void updateCount() override;

    X11StandalonePlatform *m_backend;
};

} // namespace

#endif
