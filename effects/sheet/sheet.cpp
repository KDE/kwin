/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>
Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

#include "sheet.h"

#include <kconfiggroup.h>
#include <QtCore/QTimeLine>

// Effect is based on fade effect by Philip Falkner

namespace KWin
{

KWIN_EFFECT(sheet, SheetEffect)
KWIN_EFFECT_SUPPORTED(sheet, SheetEffect::supported())

static const int IsSheetWindow = 0x22A982D5;

SheetEffect::SheetEffect()
{
    reconfigure(ReconfigureAll);
    connect(effects, SIGNAL(windowAdded(KWin::EffectWindow*)), this, SLOT(slotWindowAdded(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowClosed(KWin::EffectWindow*)), this, SLOT(slotWindowClosed(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowDeleted(KWin::EffectWindow*)), this, SLOT(slotWindowDeleted(KWin::EffectWindow*)));
}

bool SheetEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing;
}

void SheetEffect::reconfigure(ReconfigureFlags)
{
    KConfigGroup conf = effects->effectConfig("Sheet");
    duration = animationTime(conf, "AnimationTime", 500);
}

void SheetEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (!windows.isEmpty()) {
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
        screenTime = time;
    }
    effects->prePaintScreen(data, time);
}

void SheetEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    InfoMap::iterator info = windows.find(w);
    if (info != windows.end()) {
        data.setTransformed();
        if (info->added)
            info->timeLine->setCurrentTime(info->timeLine->currentTime() + screenTime);
        else if (info->closed) {
            info->timeLine->setCurrentTime(info->timeLine->currentTime() - screenTime);
            if (info->deleted)
                w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DELETE);
        }
    }

    effects->prePaintWindow(w, data, time);

    // if the window isn't to be painted, then let's make sure
    // to track its progress
    if (info != windows.end() && !w->isPaintingEnabled() && !effects->activeFullScreenEffect())
        w->addRepaintFull();
}

void SheetEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    InfoMap::const_iterator info = windows.constFind(w);
    if (info != windows.constEnd()) {
        const double progress = info->timeLine->currentValue();
        RotationData rot;
        rot.axis = RotationData::XAxis;
        rot.angle = 60.0 * (1.0 - progress);
        data.rotation = &rot;
        data.yScale *= progress;
        data.zScale *= progress;
        data.yTranslate -= (w->y() - info->parentY) * (1.0 - progress);
    }
    effects->paintWindow(w, mask, region, data);
}

void SheetEffect::postPaintWindow(EffectWindow* w)
{
    InfoMap::iterator info = windows.find(w);
    if (info != windows.end()) {
        if (info->added && info->timeLine->currentValue() == 1.0) {
            windows.remove(w);
            effects->addRepaintFull();
        } else if (info->closed && info->timeLine->currentValue() == 0.0) {
            info->closed = false;
            if (info->deleted) {
                windows.remove(w);
                w->unrefWindow();
            }
            effects->addRepaintFull();
        }
        if (info->added || info->closed)
            w->addRepaintFull();
    }
    effects->postPaintWindow(w);
}

void SheetEffect::slotWindowAdded(EffectWindow* w)
{
    if (!isSheetWindow(w))
        return;
    w->setData(IsSheetWindow, true);

    InfoMap::iterator it = windows.find(w);
    WindowInfo *info = (it == windows.end()) ? &windows[w] : &it.value();
    info->added = true;
    info->closed = false;
    info->deleted = false;
    delete info->timeLine;
    info->timeLine = new QTimeLine(duration);
    const EffectWindowList stack = effects->stackingOrder();
    // find parent
    foreach (EffectWindow * window, stack) {
        if (window->findModal() == w) {
            info->parentY = window->y();
            break;
        }
    }
    w->addRepaintFull();
}

void SheetEffect::slotWindowClosed(EffectWindow* w)
{
    if (!isSheetWindow(w))
        return;

    w->refWindow();

    InfoMap::iterator it = windows.find(w);
    WindowInfo *info = (it == windows.end()) ? &windows[w] : &it.value();
    info->added = false;
    info->closed = true;
    info->deleted = true;
    delete info->timeLine;
    info->timeLine = new QTimeLine(duration);
    info->timeLine->setCurrentTime(duration);

    bool found = false;
    // find parent
    const EffectWindowList stack = effects->stackingOrder();
    foreach (EffectWindow * window, stack) {
        if (window->findModal() == w) {
            info->parentY = window->y();
            found = true;
            break;
        }
    }
    if (!found)
        info->parentY = 0;
    w->addRepaintFull();
}

void SheetEffect::slotWindowDeleted(EffectWindow* w)
{
    windows.remove(w);
}

bool SheetEffect::isSheetWindow(EffectWindow* w)
{
    return (w->isModal() || w->data(IsSheetWindow).toBool());
}

bool SheetEffect::isActive() const
{
    return !windows.isEmpty();
}

SheetEffect::WindowInfo::WindowInfo()
    : deleted(false)
    , added(false)
    , closed(false)
    , timeLine(0)
    , parentY(0)
{
}

SheetEffect::WindowInfo::~WindowInfo()
{
    delete timeLine;
}

} // namespace
