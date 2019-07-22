/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
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
