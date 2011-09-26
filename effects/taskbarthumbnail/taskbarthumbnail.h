/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>
Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_TASKBARTHUMBNAIL_H
#define KWIN_TASKBARTHUMBNAIL_H

#include <kwineffects.h>
#include <QObject>

namespace KWin
{

class TaskbarThumbnailEffect
    : public Effect
{
    Q_OBJECT
public:
    TaskbarThumbnailEffect();
    virtual ~TaskbarThumbnailEffect();
    virtual void prePaintScreen(ScreenPrePaintData& data, int time);
    virtual void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time);
    virtual void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data);
    virtual bool isActive() const;

public Q_SLOTS:
    void slotWindowAdded(EffectWindow *w);
    void slotWindowDeleted(EffectWindow *w);
    void slotWindowDamaged(EffectWindow* w, const QRect& damage);
    void slotPropertyNotify(EffectWindow *w, long atom);
private:
    struct Data {
        Window window; // thumbnail of this window
        QRect rect;
    };
    long atom;
    QMultiHash< EffectWindow*, Data > thumbnails;
    EffectWindowList damagedWindows;
};

} // namespace

#endif
