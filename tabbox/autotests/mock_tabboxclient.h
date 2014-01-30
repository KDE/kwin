/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
#ifndef KWIN_MOCK_TABBOX_CLIENT_H
#define KWIN_MOCK_TABBOX_CLIENT_H

#include "../tabboxhandler.h"
namespace KWin
{
class MockTabBoxClient : public TabBox::TabBoxClient
{
public:
    explicit MockTabBoxClient(QString caption, WId id);
    virtual bool isMinimized() const {
        return false;
    }
    virtual QString caption() const {
        return m_caption;
    }
    virtual void close();
    virtual int height() const {
        return 100;
    }
    virtual QPixmap icon(const QSize &size = QSize(32, 32)) const {
        return QPixmap(size);
    }
    virtual bool isCloseable() const {
        return true;
    }
    virtual bool isFirstInTabBox() const {
        return false;
    }
    virtual int width() const {
        return 100;
    }
    virtual WId window() const {
        return m_wId;
    }
    virtual int x() const {
        return 0;
    }
    virtual int y() const {
        return 0;
    }

private:
    QString m_caption;
    WId m_wId;
};
} // namespace KWin
#endif
