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
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
#include "scene_opengl.h"
#endif
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
#ifdef KWIN_HAVE_OPENGL_COMPOSITING
            shadow = new SceneOpenGLShadow(toplevel);
#endif
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
    const QRect topRect(0, - m_topOffset, m_topLevel->width(), m_topOffset);
    const QRect rightRect(m_topLevel->width(), - m_topOffset, m_rightOffset, m_topLevel->height() + m_topOffset + m_bottomOffset);
    const QRect bottomRect(0, m_topLevel->height(), m_topLevel->width(), m_bottomOffset);
    const QRect leftRect(- m_leftOffset, - m_topOffset, m_leftOffset, m_topLevel->height() + m_topOffset + m_bottomOffset);
    m_shadowRegion = QRegion(topRect).united(rightRect).united(bottomRect).united(leftRect);
}

void Shadow::buildQuads()
{
    // prepare window quads
    m_shadowQuads.clear();
    const QRect topRect(QPoint(0, 0), m_shadowElements[ShadowElementTop].size());
    const QRect topRightRect(QPoint(0, 0), m_shadowElements[ShadowElementTopRight].size());
    const QRect rightRect(QPoint(0, 0), m_shadowElements[ShadowElementRight].size());
    const QRect bottomRightRect(QPoint(0, 0), m_shadowElements[ShadowElementBottomRight].size());
    const QRect bottomRect(QPoint(0, 0), m_shadowElements[ShadowElementBottom].size());
    const QRect bottomLeftRect(QPoint(0, 0), m_shadowElements[ShadowElementBottomLeft].size());
    const QRect leftRect(QPoint(0, 0), m_shadowElements[ShadowElementLeft].size());
    const QRect topLeftRect(QPoint(0, 0), m_shadowElements[ShadowElementTopLeft].size());
    if ((leftRect.width() - m_leftOffset > m_topLevel->width()) ||
        (rightRect.width() - m_rightOffset > m_topLevel->width()) ||
        (topRect.height() - m_topOffset > m_topLevel->height()) ||
        (bottomRect.height() - m_bottomOffset > m_topLevel->height())) {
        // if our shadow is bigger than the window, we don't render the shadow
        m_shadowRegion = QRegion();
        return;
    }
    WindowQuad topLeftQuad(WindowQuadShadowTopLeft);
    topLeftQuad[ 0 ] = WindowVertex(-m_leftOffset, -m_topOffset, 0.0, 0.0);
    topLeftQuad[ 1 ] = WindowVertex(-m_leftOffset + topLeftRect.width(), -m_topOffset, 1.0, 0.0);
    topLeftQuad[ 2 ] = WindowVertex(-m_leftOffset + topLeftRect.width(), -m_topOffset + topLeftRect.height(), 1.0, 1.0);
    topLeftQuad[ 3 ] = WindowVertex(-m_leftOffset, -m_topOffset + topLeftRect.height(), 0.0, 1.0);
    m_shadowQuads.append(topLeftQuad);
    WindowQuad topQuad(WindowQuadShadowTop);
    topQuad[ 0 ] = WindowVertex(-m_leftOffset + topLeftRect.width(), -m_topOffset, 0.0, 0.0);
    topQuad[ 1 ] = WindowVertex(m_topLevel->width() + m_rightOffset - topRightRect.width(), -m_topOffset, 1.0, 0.0);
    topQuad[ 2 ] = WindowVertex(m_topLevel->width() + m_rightOffset - topRightRect.width(), -m_topOffset + topRect.height(), 1.0, 1.0);
    topQuad[ 3 ] = WindowVertex(-m_leftOffset + topLeftRect.width(), -m_topOffset + topRect.height(), 0.0, 1.0);
    m_shadowQuads.append(topQuad);
    WindowQuad topRightQuad(WindowQuadShadowTopRight);
    topRightQuad[ 0 ] = WindowVertex(m_topLevel->width() + m_rightOffset - topRightRect.width(), -m_topOffset, 0.0, 0.0);
    topRightQuad[ 1 ] = WindowVertex(m_topLevel->width() + m_rightOffset, -m_topOffset, 1.0, 0.0);
    topRightQuad[ 2 ] = WindowVertex(m_topLevel->width() + m_rightOffset, -m_topOffset + topRightRect.height(), 1.0, 1.0);
    topRightQuad[ 3 ] = WindowVertex(m_topLevel->width() + m_rightOffset - topRightRect.width(), -m_topOffset + topRightRect.height(), 0.0, 1.0);
    m_shadowQuads.append(topRightQuad);
    WindowQuad rightQuad(WindowQuadShadowRight);
    rightQuad[ 0 ] = WindowVertex(m_topLevel->width() + m_rightOffset - rightRect.width(), -m_topOffset + topRightRect.height(), 0.0, 0.0);
    rightQuad[ 1 ] = WindowVertex(m_topLevel->width() + m_rightOffset, -m_topOffset + topRightRect.height(), 1.0, 0.0);
    rightQuad[ 2 ] = WindowVertex(m_topLevel->width() + m_rightOffset, m_topLevel->height() + m_bottomOffset - bottomRightRect.height(), 1.0, 1.0);
    rightQuad[ 3 ] = WindowVertex(m_topLevel->width() + m_rightOffset - rightRect.width(), m_topLevel->height() + m_bottomOffset - bottomRightRect.height(), 0.0, 1.0);
    m_shadowQuads.append(rightQuad);
    WindowQuad bottomRightQuad(WindowQuadShadowBottomRight);
    bottomRightQuad[ 0 ] = WindowVertex(m_topLevel->width() + m_rightOffset - bottomRightRect.width(), m_topLevel->height() + m_bottomOffset - bottomRightRect.height(), 0.0, 0.0);
    bottomRightQuad[ 1 ] = WindowVertex(m_topLevel->width() + m_rightOffset, m_topLevel->height() + m_bottomOffset - bottomRightRect.height(), 1.0, 0.0);
    bottomRightQuad[ 2 ] = WindowVertex(m_topLevel->width() + m_rightOffset, m_topLevel->height() + m_bottomOffset, 1.0, 1.0);
    bottomRightQuad[ 3 ] = WindowVertex(m_topLevel->width() + m_rightOffset - bottomRightRect.width(), m_topLevel->height() + m_bottomOffset, 0.0, 1.0);
    m_shadowQuads.append(bottomRightQuad);
    WindowQuad bottomQuad(WindowQuadShadowBottom);
    bottomQuad[ 0 ] = WindowVertex(-m_leftOffset + bottomLeftRect.width(), m_topLevel->height() + m_bottomOffset - bottomRect.height(), 0.0, 0.0);
    bottomQuad[ 1 ] = WindowVertex(m_topLevel->width() + m_rightOffset - bottomRightRect.width(), m_topLevel->height() + m_bottomOffset - bottomRect.height(), 1.0, 0.0);
    bottomQuad[ 2 ] = WindowVertex(m_topLevel->width() + m_rightOffset - bottomRightRect.width(), m_topLevel->height() + m_bottomOffset, 1.0, 1.0);
    bottomQuad[ 3 ] = WindowVertex(-m_leftOffset + bottomLeftRect.width(), m_topLevel->height() + m_bottomOffset, 0.0, 1.0);
    m_shadowQuads.append(bottomQuad);
    WindowQuad bottomLeftQuad(WindowQuadShadowBottomLeft);
    bottomLeftQuad[ 0 ] = WindowVertex(-m_leftOffset, m_topLevel->height() + m_bottomOffset - bottomLeftRect.height(), 0.0, 0.0);
    bottomLeftQuad[ 1 ] = WindowVertex(-m_leftOffset + bottomLeftRect.width(), m_topLevel->height() + m_bottomOffset - bottomLeftRect.height(), 1.0, 0.0);
    bottomLeftQuad[ 2 ] = WindowVertex(-m_leftOffset + bottomLeftRect.width(), m_topLevel->height() + m_bottomOffset, 1.0, 1.0);
    bottomLeftQuad[ 3 ] = WindowVertex(-m_leftOffset, m_topLevel->height() + m_bottomOffset, 0.0, 1.0);
    m_shadowQuads.append(bottomLeftQuad);
    WindowQuad leftQuad(WindowQuadShadowLeft);
    leftQuad[ 0 ] = WindowVertex(-m_leftOffset, -m_topOffset + topLeftRect.height(), 0.0, 0.0);
    leftQuad[ 1 ] = WindowVertex(-m_leftOffset + leftRect.width(), -m_topOffset + topLeftRect.height(), 1.0, 0.0);
    leftQuad[ 2 ] = WindowVertex(-m_leftOffset + leftRect.width(), m_topLevel->height() + m_bottomOffset - bottomLeftRect.height(), 1.0, 1.0);
    leftQuad[ 3 ] = WindowVertex(-m_leftOffset, m_topLevel->height() + m_bottomOffset - bottomLeftRect.height(), 0.0, 1.0);
    m_shadowQuads.append(leftQuad);
}

bool Shadow::updateShadow()
{
    QVector<long> data = Shadow::readX11ShadowProperty(m_topLevel->window());
    if (data.isEmpty()) {
        return false;
    }
    init(data);
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
