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
#ifndef KWIN_SCREENS_DRM_H
#define KWIN_SCREENS_DRM_H
#include "screens.h"

namespace KWin
{
class DrmBackend;

class DrmScreens : public Screens
{
    Q_OBJECT
public:
    DrmScreens(DrmBackend *backend, QObject *parent = nullptr);
    virtual ~DrmScreens();
    void init() override;
    QRect geometry(int screen) const override;
    int number(const QPoint &pos) const override;
    qreal scale(int screen) const override;
    QSize size(int screen) const override;
    void updateCount() override;
    QString name(int screen) const override;
    float refreshRate(int screen) const override;

    QSizeF physicalSize(int screen) const override;
    bool isInternal(int screen) const override;
    bool supportsTransformations(int screen) const override;
    Qt::ScreenOrientation orientation(int screen) const override;

private:
    DrmBackend *m_backend;
};

}

#endif
