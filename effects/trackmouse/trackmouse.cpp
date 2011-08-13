/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2010 Jorge Mata <matamax123@gmail.com>

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

#include "trackmouse.h"

#include <QTime>

#include <kwinconfig.h>
#include <kwinglutils.h>

#include <kglobal.h>
#include <kstandarddirs.h>

#include <kaction.h>
#include <kactioncollection.h>
#include <KDE/KConfigGroup>
#include <KDE/KLocale>

#include <math.h>


#include <kdebug.h>

namespace KWin
{

KWIN_EFFECT(trackmouse, TrackMouseEffect)

const int STARS = 5;
const int DIST = 50;

TrackMouseEffect::TrackMouseEffect()
    : active(false)
    , angle(0)
    , texture(NULL)
{
    mousePolling = false;
    actionCollection = new KActionCollection(this);
    action = static_cast< KAction* >(actionCollection->addAction("TrackMouse"));
    action->setText(i18n("Track mouse"));
    action->setGlobalShortcut(KShortcut());

    connect(action, SIGNAL(triggered(bool)), this, SLOT(toggle()));
    connect(effects, SIGNAL(mouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)),
            this, SLOT(slotMouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)));
    reconfigure(ReconfigureAll);
}

TrackMouseEffect::~TrackMouseEffect()
{
    if (mousePolling)
        effects->stopMousePolling();
    delete texture;
}

void TrackMouseEffect::reconfigure(ReconfigureFlags)
{
    KConfigGroup conf = effects->effectConfig("TrackMouse");
    bool shift = conf.readEntry("Shift", false);
    bool alt = conf.readEntry("Alt", false);
    bool control = conf.readEntry("Control", true);
    bool meta = conf.readEntry("Meta", true);
    modifier = 0;
    if (meta)
        modifier |= Qt::MetaModifier;
    if (control)
        modifier |= Qt::ControlModifier;
    if (alt)
        modifier |= Qt::AltModifier;
    if (shift)
        modifier |= Qt::ShiftModifier;
    if (modifier != 0 && action != NULL) {
        if (!mousePolling)
            effects->startMousePolling();
        mousePolling = true;
    } else if (action != NULL) {
        if (mousePolling)
            effects->stopMousePolling();
        mousePolling = false;
    }
}

void TrackMouseEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (active) {
        QTime t = QTime::currentTime();
        angle = ((t.second() % 4) * 90.0) + (t.msec() / 1000.0 * 90.0);
    }
    effects->prePaintScreen(data, time);
}

void TrackMouseEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    effects->paintScreen(mask, region, data);   // paint normal screen
    if (!active)
        return;
    if (texture) {
#ifndef KWIN_HAVE_OPENGLES
        glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT);
#endif
        bool useShader = false;
        if (ShaderManager::instance()->isValid()) {
            useShader = true;
            ShaderManager::instance()->pushShader(ShaderManager::SimpleShader);
        }
        texture->bind();
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (int i = 0;
                i < STARS;
                ++i) {
            QRect r = starRect(i);
            texture->render(region, r);
        }
        texture->unbind();
        glDisable(GL_BLEND);
        if (ShaderManager::instance()->isValid()) {
            ShaderManager::instance()->popShader();
        }
#ifndef KWIN_HAVE_OPENGLES
        glPopAttrib();
#endif
    }
}

void TrackMouseEffect::postPaintScreen()
{
    if (active) {
        for (int i = 0;
                i < STARS;
                ++i)
            effects->addRepaint(starRect(i));
    }
    effects->postPaintScreen();
}

void TrackMouseEffect::toggle()
{
    if (mousePolling)
        return;
    if (!active) {
        if (texture == NULL)
            loadTexture();
        if (texture == NULL)
            return;
        active = true;
        angle = 0;
    } else
        active = false;
    for (int i = 0; i < STARS; ++i)
        effects->addRepaint(starRect(i));
}

void TrackMouseEffect::slotMouseChanged(const QPoint&, const QPoint&, Qt::MouseButtons,
                                    Qt::MouseButtons, Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers)
{
    if (modifier != 0 && modifiers == modifier) {
        if (!active) {
            if (texture == NULL)
                loadTexture();
            if (texture == NULL)
                return;
            active = true;
            angle = 0;
        }
        for (int i = 0; i < STARS; ++i)
            effects->addRepaint(starRect(i));
    } else if (active) {
        for (int i = 0; i < STARS; ++i)
            effects->addRepaint(starRect(i));
        active = false;
    }
}

QRect TrackMouseEffect::starRect(int num) const
{
    int a = angle + 360 / STARS * num;
    int x = cursorPos().x() + int(DIST * cos(a * (2 * M_PI / 360)));
    int y = cursorPos().y() + int(DIST * sin(a * (2 * M_PI / 360)));
    return QRect(QPoint(x - textureSize.width() / 2,
                        y - textureSize.height() / 2), textureSize);
}

void TrackMouseEffect::loadTexture()
{
    QString file = KGlobal::dirs()->findResource("appdata", "trackmouse.png");
    if (file.isEmpty())
        return;
    QImage im(file);
    texture = new GLTexture(im);
    textureSize = im.size();
}

} // namespace
