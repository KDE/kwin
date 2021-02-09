/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_VIRTUAL_TERMINAL_H
#define KWIN_VIRTUAL_TERMINAL_H
#include <kwinglobals.h>

#include <QObject>

class QSocketNotifier;

namespace KWin
{

class KWIN_EXPORT VirtualTerminal : public QObject
{
    Q_OBJECT
public:
    ~VirtualTerminal() override;

    void init();
    void activate(int vt);
    bool isActive() const {
        return m_active;
    }

Q_SIGNALS:
    void activeChanged(bool);

private:
    void setup(int vtNr);
    void closeFd();
    bool createSignalHandler();
    void setActive(bool active);
    int m_vt = -1;
    QSocketNotifier *m_notifier = nullptr;
    bool m_active = false;
    int m_vtNumber = 0;

    KWIN_SINGLETON(VirtualTerminal)
};

}

#endif
