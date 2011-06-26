/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2007 Christian Nitschkowski <christian.nitschkowski@kdemail.net>

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

#include "magnifier.h"

#include <kwinconfig.h>

#include <kaction.h>
#include <kactioncollection.h>
#include <kconfiggroup.h>
#include <kstandardaction.h>

#include <kwinglutils.h>

namespace KWin
{

KWIN_EFFECT(magnifier, MagnifierEffect)
KWIN_EFFECT_SUPPORTED(magnifier, MagnifierEffect::supported())

const int FRAME_WIDTH = 5;

MagnifierEffect::MagnifierEffect()
    : zoom(1)
    , target_zoom(1)
    , polling(false)
{
    KActionCollection* actionCollection = new KActionCollection(this);
    KAction* a;
    a = static_cast< KAction* >(actionCollection->addAction(KStandardAction::ZoomIn, this, SLOT(zoomIn())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Equal));
    a = static_cast< KAction* >(actionCollection->addAction(KStandardAction::ZoomOut, this, SLOT(zoomOut())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_Minus));
    a = static_cast< KAction* >(actionCollection->addAction(KStandardAction::ActualSize, this, SLOT(toggle())));
    a->setGlobalShortcut(KShortcut(Qt::META + Qt::Key_0));
    connect(effects, SIGNAL(mouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)),
            this, SLOT(slotMouseChanged(QPoint,QPoint,Qt::MouseButtons,Qt::MouseButtons,Qt::KeyboardModifiers,Qt::KeyboardModifiers)));
    reconfigure(ReconfigureAll);
}

bool MagnifierEffect::supported()
{
    return effects->compositingType() == OpenGLCompositing;
}

void MagnifierEffect::reconfigure(ReconfigureFlags)
{
    KConfigGroup conf = EffectsHandler::effectConfig("Magnifier");
    int width, height;
    width = conf.readEntry("Width", 200);
    height = conf.readEntry("Height", 200);
    magnifier_size = QSize(width, height);
}

void MagnifierEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (zoom != target_zoom) {
        double diff = time / animationTime(500.0);
        if (target_zoom > zoom)
            zoom = qMin(zoom * qMax(1 + diff, 1.2), target_zoom);
        else
            zoom = qMax(zoom * qMin(1 - diff, 0.8), target_zoom);
    }
    effects->prePaintScreen(data, time);
    if (zoom != 1.0)
        data.paint |= magnifierArea().adjusted(-FRAME_WIDTH, -FRAME_WIDTH, FRAME_WIDTH, FRAME_WIDTH);
}

void MagnifierEffect::paintScreen(int mask, QRegion region, ScreenPaintData& data)
{
    ScreenPaintData data2 = data;
    effects->paintScreen(mask, region, data);   // paint normal screen
    if (zoom != 1.0) {
        // paint magnifier
        QRect area = magnifierArea();
        PaintClipper::push(area);   // don't allow any painting outside of the area
        mask |= PAINT_SCREEN_TRANSFORMED;
        data2.xScale *= zoom;
        data2.yScale *= zoom;
        QPoint cursor = cursorPos();
        // set the position so that the cursor is in the same position in the scaled view
        data2.xTranslate = - int(cursor.x() * (zoom - 1));
        data2.yTranslate = - int(cursor.y() * (zoom - 1));
        effects->paintScreen(mask, region, data2);
        PaintClipper::pop(area);
        QVector<float> verts;
        GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
        vbo->reset();
        vbo->setColor(QColor(0, 0, 0));
        // top frame
        verts << area.right() + FRAME_WIDTH << area.top() - FRAME_WIDTH;
        verts << area.left() - FRAME_WIDTH << area.top() - FRAME_WIDTH;
        verts << area.left() - FRAME_WIDTH << area.top() - 1;
        verts << area.left() - FRAME_WIDTH << area.top() - 1;
        verts << area.right() + FRAME_WIDTH << area.top() - 1;
        verts << area.right() + FRAME_WIDTH << area.top() - FRAME_WIDTH;
        // left frame
        verts << area.left() - 1 << area.top() - FRAME_WIDTH;
        verts << area.left() - FRAME_WIDTH << area.top() - FRAME_WIDTH;
        verts << area.left() - FRAME_WIDTH << area.bottom() + FRAME_WIDTH;
        verts << area.left() - FRAME_WIDTH << area.bottom() + FRAME_WIDTH;
        verts << area.left() - 1 << area.bottom() + FRAME_WIDTH;
        verts << area.left() - 1 << area.top() - FRAME_WIDTH;
        // right frame
        verts << area.right() + FRAME_WIDTH << area.top() - FRAME_WIDTH;
        verts << area.right() + 1 << area.top() - FRAME_WIDTH;
        verts << area.right() + 1 << area.bottom() + FRAME_WIDTH;
        verts << area.right() + 1 << area.bottom() + FRAME_WIDTH;
        verts << area.right() + FRAME_WIDTH << area.bottom() + FRAME_WIDTH;
        verts << area.right() + FRAME_WIDTH << area.top() - FRAME_WIDTH;
        // bottom frame
        verts << area.right() + FRAME_WIDTH << area.bottom() + 1;
        verts << area.left() - FRAME_WIDTH << area.bottom() + 1;
        verts << area.left() - FRAME_WIDTH << area.bottom() + FRAME_WIDTH;
        verts << area.left() - FRAME_WIDTH << area.bottom() + FRAME_WIDTH;
        verts << area.right() + FRAME_WIDTH << area.bottom() + FRAME_WIDTH;
        verts << area.right() + FRAME_WIDTH << area.bottom() + 1;
        vbo->setData(verts.size() / 2, 2, verts.constData(), NULL);
        if (ShaderManager::instance()->isValid()) {
            ShaderManager::instance()->pushShader(ShaderManager::ColorShader);
        }
        vbo->render(GL_TRIANGLES);
        if (ShaderManager::instance()->isValid()) {
            ShaderManager::instance()->popShader();
        }
    }
}

void MagnifierEffect::postPaintScreen()
{
    if (zoom != target_zoom) {
        QRect framedarea = magnifierArea().adjusted(-FRAME_WIDTH, -FRAME_WIDTH, FRAME_WIDTH, FRAME_WIDTH);
        effects->addRepaint(framedarea);
    }
    effects->postPaintScreen();
}

QRect MagnifierEffect::magnifierArea(QPoint pos) const
{
    return QRect(pos.x() - magnifier_size.width() / 2, pos.y() - magnifier_size.height() / 2,
                 magnifier_size.width(), magnifier_size.height());
}

void MagnifierEffect::zoomIn()
{
    target_zoom *= 1.2;
    if (!polling) {
        polling = true;
        effects->startMousePolling();
    }
    effects->addRepaint(magnifierArea().adjusted(-FRAME_WIDTH, -FRAME_WIDTH, FRAME_WIDTH, FRAME_WIDTH));
}

void MagnifierEffect::zoomOut()
{
    target_zoom /= 1.2;
    if (target_zoom < 1) {
        target_zoom = 1;
        if (polling) {
            polling = false;
            effects->stopMousePolling();
        }
    }
    effects->addRepaint(magnifierArea().adjusted(-FRAME_WIDTH, -FRAME_WIDTH, FRAME_WIDTH, FRAME_WIDTH));
}

void MagnifierEffect::toggle()
{
    if (target_zoom == 1.0) {
        target_zoom = 2;
        if (!polling) {
            polling = true;
            effects->startMousePolling();
        }
    } else {
        target_zoom = 1;
        if (polling) {
            polling = false;
            effects->stopMousePolling();
        }
    }
    effects->addRepaint(magnifierArea().adjusted(-FRAME_WIDTH, -FRAME_WIDTH, FRAME_WIDTH, FRAME_WIDTH));
}

void MagnifierEffect::slotMouseChanged(const QPoint& pos, const QPoint& old,
                                   Qt::MouseButtons, Qt::MouseButtons, Qt::KeyboardModifiers, Qt::KeyboardModifiers)
{
    if (pos != old && zoom != 1)
        // need full repaint as we might lose some change events on fast mouse movements
        // see Bug 187658
        effects->addRepaintFull();
}

} // namespace

#include "magnifier.moc"
