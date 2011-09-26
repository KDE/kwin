/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef KWIN_OUTLINE_H
#define KWIN_OUTLINE_H

#include <kwineffects.h>

namespace KWin
{

class OutlineEffect : public Effect
{
    Q_OBJECT
public:
    OutlineEffect();
    virtual ~OutlineEffect();

    virtual void paintScreen(int mask, QRegion region, ScreenPaintData& data);
    virtual bool provides(Feature feature);
    virtual bool isActive() const;

public Q_SLOTS:
    void slotShowOutline(const QRect &geometry);
    void slotHideOutline();

private:
    QRect m_geometry;
    bool m_active;
    EffectFrame *m_outline;
};

} // namespace

#endif // KWIN_OUTLINE_H
