/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_XINPUTINTEGRATION_H
#define KWIN_XINPUTINTEGRATION_H

#include <QObject>
#include <QScopedPointer>

namespace KWin
{

class XInputEventFilter;
class X11Cursor;
class Xkb;

class XInputIntegration : public QObject
{
    Q_OBJECT
public:
    explicit XInputIntegration(QObject *parent);
    virtual ~XInputIntegration();

    void init();
    void startListening();

    bool hasXinput() const {
        return m_hasXInput;
    }
    void setCursor(X11Cursor *cursor);
    void setXkb(Xkb *xkb);

private:

    bool m_hasXInput = false;
    int m_xiOpcode = 0;
    int m_majorVersion = 0;
    int m_minorVersion = 0;

    QScopedPointer<XInputEventFilter> m_xiEventFilter;
};

}

#endif
