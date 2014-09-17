/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_MOCK_CLIENT_H
#define KWIN_MOCK_CLIENT_H

#include <QObject>

namespace KWin
{

class Client : public QObject
{
    Q_OBJECT
public:
    explicit Client(QObject *parent);
    virtual ~Client();

    int screen() const;
    bool isOnScreen(int screen) const;
    bool isActive() const;

    void setActive(bool active);
    void setScreen(int screen);

private:
    bool m_active;
    int m_screen;
};

}

#endif
