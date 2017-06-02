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

#include "../../tabbox/tabboxhandler.h"

#include <QIcon>

namespace KWin
{
class MockTabBoxClient : public TabBox::TabBoxClient
{
public:
    explicit MockTabBoxClient(QString caption, WId id);
    bool isMinimized() const Q_DECL_OVERRIDE {
        return false;
    }
    QString caption() const Q_DECL_OVERRIDE {
        return m_caption;
    }
    void close() Q_DECL_OVERRIDE;
    int height() const Q_DECL_OVERRIDE {
        return 100;
    }
    virtual QPixmap icon(const QSize &size = QSize(32, 32)) const {
        return QPixmap(size);
    }
    bool isCloseable() const Q_DECL_OVERRIDE {
        return true;
    }
    bool isFirstInTabBox() const Q_DECL_OVERRIDE {
        return false;
    }
    int width() const Q_DECL_OVERRIDE {
        return 100;
    }
    WId window() const Q_DECL_OVERRIDE {
        return m_wId;
    }
    int x() const Q_DECL_OVERRIDE {
        return 0;
    }
    int y() const Q_DECL_OVERRIDE {
        return 0;
    }
    QIcon icon() const Q_DECL_OVERRIDE {
        return QIcon();
    }

private:
    QString m_caption;
    WId m_wId;
};
} // namespace KWin
#endif
