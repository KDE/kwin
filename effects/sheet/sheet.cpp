/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Philip Falkner <philip.falkner@gmail.com>
Copyright (C) 2009 Martin Gräßlin <mgraesslin@kde.org>

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
#include "sheetconfig.h"

#include <QTimeLine>
#include <QGraphicsRotation>
#include <QVector3D>

// Effect is based on fade effect by Philip Falkner

namespace KWin
{

SheetEffect::SheetEffect()
{
    initConfig<SheetConfig>();
    reconfigure(ReconfigureAll);
    connect(effects, SIGNAL(windowAdded(KWin::EffectWindow*)), this, SLOT(slotWindowAdded(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowClosed(KWin::EffectWindow*)), this, SLOT(slotWindowClosed(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowDeleted(KWin::EffectWindow*)), this, SLOT(slotWindowDeleted(KWin::EffectWindow*)));
}

bool SheetEffect::supported()
{
    return effects->isOpenGLCompositing() && effects->animationsSupported();
}

void SheetEffect::reconfigure(ReconfigureFlags)
{
    SheetConfig::self()->read();
    duration = animationTime(SheetConfig::animationTime() != 0 ? SheetConfig::animationTime() : 500);
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
    if (info == windows.constEnd()) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    // Perspective projection distorts objects near edges of the viewport
    // in undesired way. To fix this, the center of the window will be
    // moved to the origin, after applying perspective projection, the
    // center is moved back to its "original" projected position. Overall,
    // this is how the window will be transformed:
    //  [move to the origin] -> [scale] -> [rotate] -> [translate] ->
    //    -> [perspective projection] -> [reverse "move to the origin"]
    const QMatrix4x4 oldProjMatrix = data.screenProjectionMatrix();
    const QRectF windowGeo = w->geometry();
    const QVector3D invOffset = oldProjMatrix.map(QVector3D(windowGeo.center()));
    QMatrix4x4 invOffsetMatrix;
    invOffsetMatrix.translate(invOffset.x(), invOffset.y());
    data.setProjectionMatrix(invOffsetMatrix * oldProjMatrix);

    // Move the center of the window to the origin.
    const QRectF screenGeo = effects->virtualScreenGeometry();
    const QPointF offset = screenGeo.center() - windowGeo.center();
    data.translate(offset.x(), offset.y());

    const double progress = info->timeLine->currentValue();
    QGraphicsRotation rot;
    data.setRotationAxis(Qt::XAxis);
    data.setRotationAngle(60.0 * (1.0 - progress));
    data *= QVector3D(1.0, progress, progress);
    data.translate(0.0, - (w->y() - info->parentY) * (1.0 - progress));

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

    w->setData(WindowAddedGrabRole, QVariant::fromValue(static_cast<void*>(this)));

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

    w->setData(WindowClosedGrabRole, QVariant::fromValue(static_cast<void*>(this)));

    w->addRepaintFull();
}

void SheetEffect::slotWindowDeleted(EffectWindow* w)
{
    windows.remove(w);
}

bool SheetEffect::isSheetWindow(EffectWindow* w)
{
    return w->isModal();
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
