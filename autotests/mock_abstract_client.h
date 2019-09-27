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
#ifndef KWIN_MOCK_ABSTRACT_CLIENT_H
#define KWIN_MOCK_ABSTRACT_CLIENT_H

#include <QObject>
#include <QRect>

namespace KWin
{

class AbstractClient : public QObject
{
    Q_OBJECT
public:
    explicit AbstractClient(QObject *parent);
    ~AbstractClient() override;

    int screen() const;
    bool isOnScreen(int screen) const;
    bool isActive() const;
    bool isFullScreen() const;
    bool isHiddenInternal() const;
    QRect frameGeometry() const;
    bool keepBelow() const;

    void setActive(bool active);
    void setScreen(int screen);
    void setFullScreen(bool set);
    void setHiddenInternal(bool set);
    void setFrameGeometry(const QRect &rect);
    void setKeepBelow(bool);
    bool isResize() const;
    void setResize(bool set);
    virtual void showOnScreenEdge() = 0;

Q_SIGNALS:
    void geometryChanged();
    void keepBelowChanged();

private:
    bool m_active;
    int m_screen;
    bool m_fullscreen;
    bool m_hiddenInternal;
    bool m_keepBelow;
    QRect m_frameGeometry;
    bool m_resize;
};

}

#endif
