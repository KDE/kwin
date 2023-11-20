/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/itemgeometry.h"
#include "effect/globals.h"

#include <QMatrix4x4>

namespace KWin
{

WindowQuad WindowQuad::makeSubQuad(double x1, double y1, double x2, double y2) const
{
    Q_ASSERT(x1 < x2 && y1 < y2 && x1 >= left() && x2 <= right() && y1 >= top() && y2 <= bottom());
    WindowQuad ret(*this);
    // vertices are clockwise starting from topleft
    ret.verts[0].px = x1;
    ret.verts[3].px = x1;
    ret.verts[1].px = x2;
    ret.verts[2].px = x2;
    ret.verts[0].py = y1;
    ret.verts[1].py = y1;
    ret.verts[2].py = y2;
    ret.verts[3].py = y2;

    const double xOrigin = left();
    const double yOrigin = top();

    const double widthReciprocal = 1 / (right() - xOrigin);
    const double heightReciprocal = 1 / (bottom() - yOrigin);

    for (int i = 0; i < 4; ++i) {
        const double w1 = (ret.verts[i].px - xOrigin) * widthReciprocal;
        const double w2 = (ret.verts[i].py - yOrigin) * heightReciprocal;

        // Use bilinear interpolation to compute the texture coords.
        ret.verts[i].tx = (1 - w1) * (1 - w2) * verts[0].tx + w1 * (1 - w2) * verts[1].tx + w1 * w2 * verts[2].tx + (1 - w1) * w2 * verts[3].tx;
        ret.verts[i].ty = (1 - w1) * (1 - w2) * verts[0].ty + w1 * (1 - w2) * verts[1].ty + w1 * w2 * verts[2].ty + (1 - w1) * w2 * verts[3].ty;
    }

    return ret;
}

WindowQuadList WindowQuadList::splitAtX(double x) const
{
    WindowQuadList ret;
    ret.reserve(count());
    for (const WindowQuad &quad : *this) {
        bool wholeleft = true;
        bool wholeright = true;
        for (int i = 0; i < 4; ++i) {
            if (quad[i].x() < x) {
                wholeright = false;
            }
            if (quad[i].x() > x) {
                wholeleft = false;
            }
        }
        if (wholeleft || wholeright) { // is whole in one split part
            ret.append(quad);
            continue;
        }
        if (quad.top() == quad.bottom() || quad.left() == quad.right()) { // quad has no size
            ret.append(quad);
            continue;
        }
        ret.append(quad.makeSubQuad(quad.left(), quad.top(), x, quad.bottom()));
        ret.append(quad.makeSubQuad(x, quad.top(), quad.right(), quad.bottom()));
    }
    return ret;
}

WindowQuadList WindowQuadList::splitAtY(double y) const
{
    WindowQuadList ret;
    ret.reserve(count());
    for (const WindowQuad &quad : *this) {
        bool wholetop = true;
        bool wholebottom = true;
        for (int i = 0; i < 4; ++i) {
            if (quad[i].y() < y) {
                wholebottom = false;
            }
            if (quad[i].y() > y) {
                wholetop = false;
            }
        }
        if (wholetop || wholebottom) { // is whole in one split part
            ret.append(quad);
            continue;
        }
        if (quad.top() == quad.bottom() || quad.left() == quad.right()) { // quad has no size
            ret.append(quad);
            continue;
        }
        ret.append(quad.makeSubQuad(quad.left(), quad.top(), quad.right(), y));
        ret.append(quad.makeSubQuad(quad.left(), y, quad.right(), quad.bottom()));
    }
    return ret;
}

WindowQuadList WindowQuadList::makeGrid(int maxQuadSize) const
{
    if (empty()) {
        return *this;
    }

    // Find the bounding rectangle
    double left = first().left();
    double right = first().right();
    double top = first().top();
    double bottom = first().bottom();

    for (const WindowQuad &quad : std::as_const(*this)) {
        left = std::min(left, quad.left());
        right = std::max(right, quad.right());
        top = std::min(top, quad.top());
        bottom = std::max(bottom, quad.bottom());
    }

    WindowQuadList ret;

    for (const WindowQuad &quad : std::as_const(*this)) {
        const double quadLeft = quad.left();
        const double quadRight = quad.right();
        const double quadTop = quad.top();
        const double quadBottom = quad.bottom();

        // sanity check, see BUG 390953
        if (quadLeft == quadRight || quadTop == quadBottom) {
            ret.append(quad);
            continue;
        }

        // Compute the top-left corner of the first intersecting grid cell
        const double xBegin = left + qFloor((quadLeft - left) / maxQuadSize) * maxQuadSize;
        const double yBegin = top + qFloor((quadTop - top) / maxQuadSize) * maxQuadSize;

        // Loop over all intersecting cells and add sub-quads
        for (double y = yBegin; y < quadBottom; y += maxQuadSize) {
            const double y0 = std::max(y, quadTop);
            const double y1 = std::min(quadBottom, y + maxQuadSize);

            for (double x = xBegin; x < quadRight; x += maxQuadSize) {
                const double x0 = std::max(x, quadLeft);
                const double x1 = std::min(quadRight, x + maxQuadSize);

                ret.append(quad.makeSubQuad(x0, y0, x1, y1));
            }
        }
    }

    return ret;
}

WindowQuadList WindowQuadList::makeRegularGrid(int xSubdivisions, int ySubdivisions) const
{
    if (empty()) {
        return *this;
    }

    // Find the bounding rectangle
    double left = first().left();
    double right = first().right();
    double top = first().top();
    double bottom = first().bottom();

    for (const WindowQuad &quad : *this) {
        left = std::min(left, quad.left());
        right = std::max(right, quad.right());
        top = std::min(top, quad.top());
        bottom = std::max(bottom, quad.bottom());
    }

    double xIncrement = (right - left) / xSubdivisions;
    double yIncrement = (bottom - top) / ySubdivisions;

    WindowQuadList ret;

    for (const WindowQuad &quad : *this) {
        const double quadLeft = quad.left();
        const double quadRight = quad.right();
        const double quadTop = quad.top();
        const double quadBottom = quad.bottom();

        // sanity check, see BUG 390953
        if (quadLeft == quadRight || quadTop == quadBottom) {
            ret.append(quad);
            continue;
        }

        // Compute the top-left corner of the first intersecting grid cell
        const double xBegin = left + qFloor((quadLeft - left) / xIncrement) * xIncrement;
        const double yBegin = top + qFloor((quadTop - top) / yIncrement) * yIncrement;

        // Loop over all intersecting cells and add sub-quads
        for (double y = yBegin; y < quadBottom; y += yIncrement) {
            const double y0 = std::max(y, quadTop);
            const double y1 = std::min(quadBottom, y + yIncrement);

            for (double x = xBegin; x < quadRight; x += xIncrement) {
                const double x0 = std::max(x, quadLeft);
                const double x1 = std::min(quadRight, x + xIncrement);

                ret.append(quad.makeSubQuad(x0, y0, x1, y1));
            }
        }
    }

    return ret;
}

void RenderGeometry::copy(std::span<GLVertex2D> destination)
{
    Q_ASSERT(int(destination.size()) >= size());
    std::copy(cbegin(), cend(), destination.begin());
}

void RenderGeometry::appendWindowVertex(const WindowVertex &windowVertex, qreal deviceScale)
{
    GLVertex2D glVertex;
    switch (m_vertexSnappingMode) {
    case VertexSnappingMode::None:
        glVertex.position = QVector2D(windowVertex.x(), windowVertex.y()) * deviceScale;
        break;
    case VertexSnappingMode::Round:
        glVertex.position = roundVector(QVector2D(windowVertex.x(), windowVertex.y()) * deviceScale);
        break;
    }
    glVertex.texcoord = QVector2D(windowVertex.u(), windowVertex.v());
    append(glVertex);
}

void RenderGeometry::appendWindowQuad(const WindowQuad &quad, qreal deviceScale)
{
    // Geometry assumes we're rendering triangles, so add the quad's
    // vertices as two triangles. Vertex order is top-left, bottom-left,
    // top-right followed by top-right, bottom-left, bottom-right.
    appendWindowVertex(quad[0], deviceScale);
    appendWindowVertex(quad[3], deviceScale);
    appendWindowVertex(quad[1], deviceScale);

    appendWindowVertex(quad[1], deviceScale);
    appendWindowVertex(quad[3], deviceScale);
    appendWindowVertex(quad[2], deviceScale);
}

void RenderGeometry::appendSubQuad(const WindowQuad &quad, const QRectF &subquad, qreal deviceScale)
{
    std::array<GLVertex2D, 4> vertices;
    vertices[0].position = QVector2D(subquad.topLeft());
    vertices[1].position = QVector2D(subquad.topRight());
    vertices[2].position = QVector2D(subquad.bottomRight());
    vertices[3].position = QVector2D(subquad.bottomLeft());

    const auto deviceQuad = QRectF{QPointF(std::round(quad.left() * deviceScale), std::round(quad.top() * deviceScale)),
                                   QPointF(std::round(quad.right() * deviceScale), std::round(quad.bottom() * deviceScale))};

    const QPointF origin = deviceQuad.topLeft();
    const QSizeF size = deviceQuad.size();

#pragma GCC unroll 4
    for (int i = 0; i < 4; ++i) {
        const double weight1 = (vertices[i].position.x() - origin.x()) / size.width();
        const double weight2 = (vertices[i].position.y() - origin.y()) / size.height();
        const double oneMinW1 = 1.0 - weight1;
        const double oneMinW2 = 1.0 - weight2;

        const float u = oneMinW1 * oneMinW2 * quad[0].u() + weight1 * oneMinW2 * quad[1].u()
            + weight1 * weight2 * quad[2].u() + oneMinW1 * weight2 * quad[3].u();
        const float v = oneMinW1 * oneMinW2 * quad[0].v() + weight1 * oneMinW2 * quad[1].v()
            + weight1 * weight2 * quad[2].v() + oneMinW1 * weight2 * quad[3].v();
        vertices[i].texcoord = QVector2D(u, v);
    }

    append(vertices[0]);
    append(vertices[3]);
    append(vertices[1]);

    append(vertices[1]);
    append(vertices[3]);
    append(vertices[2]);
}

void RenderGeometry::postProcessTextureCoordinates(const QMatrix4x4 &textureMatrix)
{
    if (!textureMatrix.isIdentity()) {
        const QVector2D coeff(textureMatrix(0, 0), textureMatrix(1, 1));
        const QVector2D offset(textureMatrix(0, 3), textureMatrix(1, 3));

        for (auto &vertex : (*this)) {
            vertex.texcoord = vertex.texcoord * coeff + offset;
        }
    }
}

} // namespace KWin
