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

#include "taskbarthumbnail.h"

#include <kdebug.h>

#ifdef KWIN_HAVE_OPENGL
#include <kwinglutils.h>
#endif

// This effect shows a preview inside a window that has a special property set
// on it that says which window and where to render. It is used by the taskbar
// to show window previews in tooltips.

namespace KWin
{

KWIN_EFFECT(taskbarthumbnail, TaskbarThumbnailEffect)

TaskbarThumbnailEffect::TaskbarThumbnailEffect()
{
    atom = XInternAtom(display(), "_KDE_WINDOW_PREVIEW", False);
    effects->registerPropertyType(atom, true);
    // TODO hackish way to announce support, make better after 4.0
    unsigned char dummy = 0;
    XChangeProperty(display(), rootWindow(), atom, atom, 8, PropModeReplace, &dummy, 1);
    connect(effects, SIGNAL(windowAdded(EffectWindow*)), this, SLOT(slotWindowAdded(EffectWindow*)));
    connect(effects, SIGNAL(windowDeleted(EffectWindow*)), this, SLOT(slotWindowDeleted(EffectWindow*)));
    connect(effects, SIGNAL(windowDamaged(EffectWindow*,QRect)), this, SLOT(slotWindowDamaged(EffectWindow*,QRect)));
    connect(effects, SIGNAL(propertyNotify(EffectWindow*,long)), this, SLOT(slotPropertyNotify(EffectWindow*,long)));
}

TaskbarThumbnailEffect::~TaskbarThumbnailEffect()
{
    XDeleteProperty(display(), rootWindow(), atom);
    effects->registerPropertyType(atom, false);
}

void TaskbarThumbnailEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (thumbnails.count() > 0) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS_WITHOUT_FULL_REPAINTS;
    }
    effects->prePaintScreen(data, time);
}

void TaskbarThumbnailEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    // TODO what if of the windows is translucent and one not? change data.mask?
    effects->prePaintWindow(w, data, time);
}

void TaskbarThumbnailEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    effects->paintWindow(w, mask, region, data);   // paint window first
    if (thumbnails.contains(w)) {
        // paint thumbnails on it
        int mask = PAINT_WINDOW_TRANSFORMED;
        if (data.opacity == 1.0)
            mask |= PAINT_WINDOW_OPAQUE;
        else
            mask |= PAINT_WINDOW_TRANSLUCENT;
        mask |= PAINT_WINDOW_LANCZOS;
        foreach (const Data & thumb, thumbnails.values(w)) {
            EffectWindow* thumbw = effects->findWindow(thumb.window);
            if (thumbw == NULL)
                continue;
            WindowPaintData thumbData(thumbw);
            thumbData.opacity *= data.opacity;
            QRect r, thumbRect(thumb.rect);
            thumbRect.translate(w->pos() + QPoint(data.xTranslate, data.yTranslate));
            thumbRect.setSize(QSize(thumbRect.width() * data.xScale, thumbRect.height() * data.yScale)); // QSize has no vector multiplicator... :-(

#ifdef KWIN_HAVE_OPENGL
            if (effects->compositingType() == KWin::OpenGLCompositing) {
                if (data.shader) {
                    thumbData.shader = data.shader;
                }
            } // if ( effects->compositingType() == KWin::OpenGLCompositing )
#endif
            setPositionTransformations(thumbData, r, thumbw, thumbRect, Qt::KeepAspectRatio);
            effects->drawWindow(thumbw, mask, r, thumbData);
        }
    }
} // End of function

void TaskbarThumbnailEffect::slotWindowDamaged(EffectWindow* w, const QRect& damage)
{
    Q_UNUSED(damage);
    // Update the thumbnail if the window was damaged
    foreach (EffectWindow * window, thumbnails.uniqueKeys())
    foreach (const Data & thumb, thumbnails.values(window))
    if (w == effects->findWindow(thumb.window))
        window->addRepaint(thumb.rect);
}

void TaskbarThumbnailEffect::slotWindowAdded(EffectWindow* w)
{
    slotPropertyNotify(w, atom);   // read initial value
}

void TaskbarThumbnailEffect::slotWindowDeleted(EffectWindow* w)
{
    foreach (EffectWindow *window, thumbnails.uniqueKeys()) {
        foreach (const Data &thumb, thumbnails.values(window)) {
            if (w == effects->findWindow(thumb.window)) {
                window->addRepaint(thumb.rect);
            }
        }
    }
    thumbnails.remove(w);
}

void TaskbarThumbnailEffect::slotPropertyNotify(EffectWindow* w, long a)
{
    if (!w || a != atom)
        return;
    foreach (const Data & thumb, thumbnails.values(w)) {
        w->addRepaintFull();
    }
    thumbnails.remove(w);
    QByteArray data = w->readProperty(atom, atom, 32);
    if (data.length() < 1)
        return;
    long* d = reinterpret_cast< long* >(data.data());
    int len = data.length() / sizeof(d[ 0 ]);
    int pos = 0;
    int cnt = d[ 0 ];
    ++pos;
    for (int i = 0;
            i < cnt;
            ++i) {
        int size = d[ pos ];
        if (len - pos < size)
            return; // format error
        ++pos;
        Data data;
        data.window = d[ pos ];
        data.rect = QRect(d[ pos + 1 ], d[ pos + 2 ], d[ pos + 3 ], d[ pos + 4 ]);
        thumbnails.insert(w, data);
        w->addRepaint(data.rect);
        pos += size;
    }
}

bool TaskbarThumbnailEffect::isActive() const
{
    return !thumbnails.isEmpty();
}

} // namespace
