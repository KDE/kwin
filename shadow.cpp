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
#include "shadow.h"
// kwin
#include "atoms.h"
#include "effects.h"
#include "toplevel.h"
#include "scene_opengl.h"
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
#include "scene_xrender.h"
#endif

namespace KWin
{

Shadow::Shadow(Toplevel *toplevel)
    : m_topLevel(toplevel)
    , m_cachedSize(toplevel->geometry().size())
{
    connect(m_topLevel, SIGNAL(geometryChanged()), SLOT(geometryChanged()));
}

Shadow::~Shadow()
{
}

Shadow *Shadow::createShadow(Toplevel *toplevel)
{
    if (!effects) {
        return NULL;
    }
    QVector<long> data = Shadow::readX11ShadowProperty(toplevel->window());
    if (!data.isEmpty()) {
        Shadow *shadow = NULL;
        if (effects->compositingType() == OpenGLCompositing) {
            shadow = new SceneOpenGLShadow(toplevel);
        } else if (effects->compositingType() == XRenderCompositing) {
#ifdef KWIN_HAVE_XRENDER_COMPOSITING
            shadow = new SceneXRenderShadow(toplevel);
#endif
        }

        if (shadow) {
            if (!shadow->init(data)) {
                delete shadow;
                return NULL;
            }
            if (toplevel->effectWindow() && toplevel->effectWindow()->sceneWindow()) {
                toplevel->effectWindow()->sceneWindow()->updateShadow(shadow);
            }
        }
        return shadow;
    } else {
        return NULL;
    }
}

QVector< long > Shadow::readX11ShadowProperty(WId id)
{
    QVector<long> ret;
    Atom type;
    int format, status;
    unsigned long nitems = 0;
    unsigned long extra = 0;
    unsigned char *data = 0;
    status = XGetWindowProperty(display(), id, atoms->kde_net_wm_shadow, 0, 12, false, XA_CARDINAL, &type, &format, &nitems, &extra, &data);
    if (status == Success && type == XA_CARDINAL && format == 32 && nitems == 12) {
        long* shadow = reinterpret_cast< long* >(data);
        ret.reserve(12);
        for (int i=0; i<12; ++i) {
            ret << shadow[i];
        }
        XFree(data);
    }
    return ret;
}

bool Shadow::init(const QVector< long > &data)
{
    for (int i=0; i<ShadowElementsCount; ++i) {
        QPixmap pix = QPixmap::fromX11Pixmap(data[i], QPixmap::ExplicitlyShared);
        if (pix.isNull() || pix.depth() != 32) {
            return false;
        }
        m_shadowElements[i] = pix.copy(0, 0, pix.width(), pix.height());
    }
    m_topOffset = data[ShadowElementsCount];
    m_rightOffset = data[ShadowElementsCount+1];
    m_bottomOffset = data[ShadowElementsCount+2];
    m_leftOffset = data[ShadowElementsCount+3];
    updateShadowRegion();
    if (!prepareBackend()) {
        return false;
    }
    buildQuads();
    return true;
}

void Shadow::updateShadowRegion()
{
    const QRect top(0, - m_topOffset, m_topLevel->width(), m_topOffset);
    const QRect right(m_topLevel->width(), - m_topOffset, m_rightOffset, m_topLevel->height() + m_topOffset + m_bottomOffset);
    const QRect bottom(0, m_topLevel->height(), m_topLevel->width(), m_bottomOffset);
    const QRect left(- m_leftOffset, - m_topOffset, m_leftOffset, m_topLevel->height() + m_topOffset + m_bottomOffset);
    m_shadowRegion = QRegion(top).united(right).united(bottom).united(left);
}

void Shadow::buildQuads()
{
    // prepare window quads
    m_shadowQuads.clear();
    const QSize top(m_shadowElements[ShadowElementTop].size());
    const QSize topRight(m_shadowElements[ShadowElementTopRight].size());
    const QSize right(m_shadowElements[ShadowElementRight].size());
    const QSize bottomRight(m_shadowElements[ShadowElementBottomRight].size());
    const QSize bottom(m_shadowElements[ShadowElementBottom].size());
    const QSize bottomLeft(m_shadowElements[ShadowElementBottomLeft].size());
    const QSize left(m_shadowElements[ShadowElementLeft].size());
    const QSize topLeft(m_shadowElements[ShadowElementTopLeft].size());
    if ((left.width() - m_leftOffset > m_topLevel->width()) ||
        (right.width() - m_rightOffset > m_topLevel->width()) ||
        (top.height() - m_topOffset > m_topLevel->height()) ||
        (bottom.height() - m_bottomOffset > m_topLevel->height())) {
        // if our shadow is bigger than the window, we don't render the shadow
        m_shadowRegion = QRegion();
        return;
    }

    const QRect outerRect(QPoint(-m_leftOffset, -m_topOffset), QPoint(m_topLevel->width() + m_rightOffset, m_topLevel->height() + m_bottomOffset));

    WindowQuad topLeftQuad(WindowQuadShadowTopLeft);
    topLeftQuad[ 0 ] = WindowVertex(outerRect.x(),                      outerRect.y(), 0.0, 0.0);
    topLeftQuad[ 1 ] = WindowVertex(outerRect.x() + topLeft.width(),    outerRect.y(), 1.0, 0.0);
    topLeftQuad[ 2 ] = WindowVertex(outerRect.x() + topLeft.width(),    outerRect.y() + topLeft.height(), 1.0, 1.0);
    topLeftQuad[ 3 ] = WindowVertex(outerRect.x(),                      outerRect.y() + topLeft.height(), 0.0, 1.0);
    m_shadowQuads.append(topLeftQuad);

    WindowQuad topQuad(WindowQuadShadowTop);
    topQuad[ 0 ] = WindowVertex(outerRect.x() + topLeft.width(),        outerRect.y(), 0.0, 0.0);
    topQuad[ 1 ] = WindowVertex(outerRect.right() - topRight.width(),   outerRect.y(), 1.0, 0.0);
    topQuad[ 2 ] = WindowVertex(outerRect.right() - topRight.width(),   outerRect.y() + top.height(), 1.0, 1.0);
    topQuad[ 3 ] = WindowVertex(outerRect.x() + topLeft.width(),        outerRect.y() + top.height(), 0.0, 1.0);
    m_shadowQuads.append(topQuad);

    WindowQuad topRightQuad(WindowQuadShadowTopRight);
    topRightQuad[ 0 ] = WindowVertex(outerRect.right() - topRight.width(),  outerRect.y(), 0.0, 0.0);
    topRightQuad[ 1 ] = WindowVertex(outerRect.right(),                     outerRect.y(), 1.0, 0.0);
    topRightQuad[ 2 ] = WindowVertex(outerRect.right(),                     outerRect.y() + topRight.height(), 1.0, 1.0);
    topRightQuad[ 3 ] = WindowVertex(outerRect.right() - topRight.width(),  outerRect.y() + topRight.height(), 0.0, 1.0);
    m_shadowQuads.append(topRightQuad);

    WindowQuad rightQuad(WindowQuadShadowRight);
    rightQuad[ 0 ] = WindowVertex(outerRect.right() - right.width(),    outerRect.y() + topRight.height(), 0.0, 0.0);
    rightQuad[ 1 ] = WindowVertex(outerRect.right(),                    outerRect.y() + topRight.height(), 1.0, 0.0);
    rightQuad[ 2 ] = WindowVertex(outerRect.right(),                    outerRect.bottom() - bottomRight.height(), 1.0, 1.0);
    rightQuad[ 3 ] = WindowVertex(outerRect.right() - right.width(),    outerRect.bottom() - bottomRight.height(), 0.0, 1.0);
    m_shadowQuads.append(rightQuad);

    WindowQuad bottomRightQuad(WindowQuadShadowBottomRight);
    bottomRightQuad[ 0 ] = WindowVertex(outerRect.right() - bottomRight.width(),    outerRect.bottom() - bottomRight.height(), 0.0, 0.0);
    bottomRightQuad[ 1 ] = WindowVertex(outerRect.right(),                          outerRect.bottom() - bottomRight.height(), 1.0, 0.0);
    bottomRightQuad[ 2 ] = WindowVertex(outerRect.right(),                          outerRect.bottom(), 1.0, 1.0);
    bottomRightQuad[ 3 ] = WindowVertex(outerRect.right() - bottomRight.width(),    outerRect.bottom(), 0.0, 1.0);
    m_shadowQuads.append(bottomRightQuad);

    WindowQuad bottomQuad(WindowQuadShadowBottom);
    bottomQuad[ 0 ] = WindowVertex(outerRect.x() + bottomLeft.width(),      outerRect.bottom() - bottom.height(), 0.0, 0.0);
    bottomQuad[ 1 ] = WindowVertex(outerRect.right() - bottomRight.width(), outerRect.bottom() - bottom.height(), 1.0, 0.0);
    bottomQuad[ 2 ] = WindowVertex(outerRect.right() - bottomRight.width(), outerRect.bottom(), 1.0, 1.0);
    bottomQuad[ 3 ] = WindowVertex(outerRect.x() + bottomLeft.width(),      outerRect.bottom(), 0.0, 1.0);
    m_shadowQuads.append(bottomQuad);

    WindowQuad bottomLeftQuad(WindowQuadShadowBottomLeft);
    bottomLeftQuad[ 0 ] = WindowVertex(outerRect.x(),                       outerRect.bottom() - bottomLeft.height(), 0.0, 0.0);
    bottomLeftQuad[ 1 ] = WindowVertex(outerRect.x() + bottomLeft.width(),  outerRect.bottom() - bottomLeft.height(), 1.0, 0.0);
    bottomLeftQuad[ 2 ] = WindowVertex(outerRect.x() + bottomLeft.width(),  outerRect.bottom(), 1.0, 1.0);
    bottomLeftQuad[ 3 ] = WindowVertex(outerRect.x(),                       outerRect.bottom(), 0.0, 1.0);
    m_shadowQuads.append(bottomLeftQuad);

    WindowQuad leftQuad(WindowQuadShadowLeft);
    leftQuad[ 0 ] = WindowVertex(outerRect.x(),                 outerRect.y() + topLeft.height(), 0.0, 0.0);
    leftQuad[ 1 ] = WindowVertex(outerRect.x() + left.width(),  outerRect.y() + topLeft.height(), 1.0, 0.0);
    leftQuad[ 2 ] = WindowVertex(outerRect.x() + left.width(),  outerRect.bottom() - bottomLeft.height(), 1.0, 1.0);
    leftQuad[ 3 ] = WindowVertex(outerRect.x(),                 outerRect.bottom() - bottomLeft.height(), 0.0, 1.0);
    m_shadowQuads.append(leftQuad);
}

bool Shadow::updateShadow()
{
    QVector<long> data = Shadow::readX11ShadowProperty(m_topLevel->window());
    if (data.isEmpty()) {
        if (m_topLevel && m_topLevel->effectWindow() && m_topLevel->effectWindow()->sceneWindow())
            m_topLevel->effectWindow()->sceneWindow()->updateShadow(0);
        deleteLater();
        return false;
    }
    init(data);
    if (m_topLevel && m_topLevel->effectWindow())
        m_topLevel->effectWindow()->buildQuads(true);
    return true;
}

void Shadow::setToplevel(Toplevel *topLevel)
{
    m_topLevel = topLevel;
    connect(m_topLevel, SIGNAL(geometryChanged()), SLOT(geometryChanged()));
}
void Shadow::geometryChanged()
{
    if (m_cachedSize == m_topLevel->geometry().size()) {
        return;
    }
    m_cachedSize = m_topLevel->geometry().size();
    updateShadowRegion();
    buildQuads();
}

} // namespace
