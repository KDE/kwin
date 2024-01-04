/*
    SPDX-FileCopyrightText: 2006 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2022 Arjen Hiemstra <ahiemstra@heimr.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "opengl/glvertexbuffer.h"

namespace KWin
{

/**
 * @short Vertex class
 *
 * A vertex is one position in a window. WindowQuad consists of four WindowVertex objects
 * and represents one part of a window.
 */
class KWIN_EXPORT WindowVertex
{
public:
    WindowVertex();
    WindowVertex(const QPointF &position, const QPointF &textureCoordinate);
    WindowVertex(double x, double y, double tx, double ty);

    double x() const
    {
        return px;
    }
    double y() const
    {
        return py;
    }
    double u() const
    {
        return tx;
    }
    double v() const
    {
        return ty;
    }
    void move(double x, double y);
    void setX(double x);
    void setY(double y);

private:
    friend class WindowQuad;
    friend class WindowQuadList;
    double px, py; // position
    double tx, ty; // texture coords
};

/**
 * @short Class representing one area of a window.
 *
 * WindowQuads consists of four WindowVertex objects and represents one part of a window.
 */
// NOTE: This class expects the (original) vertices to be in the clockwise order starting from topleft.
class KWIN_EXPORT WindowQuad
{
public:
    WindowQuad();
    WindowQuad makeSubQuad(double x1, double y1, double x2, double y2) const;
    WindowVertex &operator[](int index);
    const WindowVertex &operator[](int index) const;
    double left() const;
    double right() const;
    double top() const;
    double bottom() const;
    QRectF bounds() const;

    static WindowQuad fromRect(const QRectF &position, const QRectF &texturePosition);

private:
    friend class WindowQuadList;
    WindowVertex verts[4];
};

class KWIN_EXPORT WindowQuadList
    : public QList<WindowQuad>
{
public:
    WindowQuadList splitAtX(double x) const;
    WindowQuadList splitAtY(double y) const;
    WindowQuadList makeGrid(int maxquadsize) const;
    WindowQuadList makeRegularGrid(int xSubdivisions, int ySubdivisions) const;
};

/**
 * A helper class for render geometry in device coordinates.
 *
 * This mostly represents a vector of vertices, with some convenience methods
 * for easily converting from WindowQuad and related classes to lists of
 * GLVertex2D. This class assumes rendering happens as unindexed triangles.
 */
class KWIN_EXPORT RenderGeometry : public QList<GLVertex2D>
{
public:
    /**
     * In what way should vertices snap to integer device coordinates?
     *
     * Vertices are converted to device coordinates before being sent to the
     * rendering system. Depending on scaling factors, this may lead to device
     * coordinates with fractional parts. For some cases, this may not be ideal
     * as fractional coordinates need to be interpolated and can lead to
     * "blurry" rendering. To avoid that, we can snap the vertices to integer
     * device coordinates when they are added.
     */
    enum class VertexSnappingMode {
        None, //< No rounding, device coordinates containing fractional parts
              //  are passed directly to the rendering system.
        Round, //< Perform a simple rounding, device coordinates will not have
               //  any fractional parts.
    };

    /**
     * The vertex snapping mode to use for this geometry.
     *
     * By default, this is VertexSnappingMode::Round.
     */
    inline VertexSnappingMode vertexSnappingMode() const
    {
        return m_vertexSnappingMode;
    }
    /**
     * Set the vertex snapping mode to use for this geometry.
     *
     * Note that this doesn't change vertices retroactively, so you should set
     * this before adding any vertices, or clear and rebuild the geometry after
     * setting it.
     *
     * @param mode The new rounding mode.
     */
    void setVertexSnappingMode(VertexSnappingMode mode)
    {
        m_vertexSnappingMode = mode;
    }
    /**
     * Copy geometry data into another buffer.
     *
     * This is primarily intended for copying into a vertex buffer for rendering.
     *
     * @param destination The destination buffer. This needs to be at least large
     *                    enough to contain all elements.
     */
    void copy(std::span<GLVertex2D> destination);
    /**
     * Append a WindowVertex as a geometry vertex.
     *
     * WindowVertex is assumed to be in logical coordinates. It will be converted
     * to device coordinates using the specified device scale and then rounded
     * so it fits correctly on the device pixel grid.
     *
     * @param windowVertex The WindowVertex instance to append.
     * @param deviceScale The scaling factor to use to go from logical to device
     *                    coordinates.
     */
    void appendWindowVertex(const WindowVertex &windowVertex, qreal deviceScale);
    /**
     * Append a WindowQuad as two triangles.
     *
     * This will append the corners of the specified WindowQuad in the right
     * order so they make two triangles that can be rendered by OpenGL. The
     * corners are converted to device coordinates and rounded, just like
     * `appendWindowVertex()` does.
     *
     * @param quad The WindowQuad instance to append.
     * @param deviceScale The scaling factor to use to go from logical to device
     *                    coordinates.
     */
    void appendWindowQuad(const WindowQuad &quad, qreal deviceScale);
    /**
     * Append a sub-quad of a WindowQuad as two triangles.
     *
     * This will append the sub-quad specified by `intersection` as two
     * triangles. The quad is expected to be in logical coordinates, while the
     * intersection is expected to be in device coordinates. The texture
     * coordinates of the resulting vertices are based upon those of the quad,
     * using bilinear interpolation for interpolating how much of the original
     * texture coordinates to use.
     *
     * @param quad The WindowQuad instance to use a sub-quad of.
     * @param subquad The sub-quad to append.
     * @param deviceScale The scaling factor used to convert from logical to
     *                    device coordinates.
     */
    void appendSubQuad(const WindowQuad &quad, const QRectF &subquad, qreal deviceScale);
    /**
     * Modify this geometry's texture coordinates based on a matrix.
     *
     * This is primarily intended to convert from non-normalised to normalised
     * texture coordinates.
     *
     * @param textureMatrix The texture matrix to use for modifying the
     *                      texture coordinates. Note that only the 2D scale and
     *                      translation are used.
     */
    void postProcessTextureCoordinates(const QMatrix4x4 &textureMatrix);

private:
    VertexSnappingMode m_vertexSnappingMode = VertexSnappingMode::Round;
};

inline WindowVertex::WindowVertex()
    : px(0)
    , py(0)
    , tx(0)
    , ty(0)
{
}

inline WindowVertex::WindowVertex(double _x, double _y, double _tx, double _ty)
    : px(_x)
    , py(_y)
    , tx(_tx)
    , ty(_ty)
{
}

inline WindowVertex::WindowVertex(const QPointF &position, const QPointF &texturePosition)
    : px(position.x())
    , py(position.y())
    , tx(texturePosition.x())
    , ty(texturePosition.y())
{
}

inline void WindowVertex::move(double x, double y)
{
    px = x;
    py = y;
}

inline void WindowVertex::setX(double x)
{
    px = x;
}

inline void WindowVertex::setY(double y)
{
    py = y;
}

inline WindowQuad::WindowQuad()
{
}

inline WindowVertex &WindowQuad::operator[](int index)
{
    Q_ASSERT(index >= 0 && index < 4);
    return verts[index];
}

inline const WindowVertex &WindowQuad::operator[](int index) const
{
    Q_ASSERT(index >= 0 && index < 4);
    return verts[index];
}

inline double WindowQuad::left() const
{
    return std::min(verts[0].px, std::min(verts[1].px, std::min(verts[2].px, verts[3].px)));
}

inline double WindowQuad::right() const
{
    return std::max(verts[0].px, std::max(verts[1].px, std::max(verts[2].px, verts[3].px)));
}

inline double WindowQuad::top() const
{
    return std::min(verts[0].py, std::min(verts[1].py, std::min(verts[2].py, verts[3].py)));
}

inline double WindowQuad::bottom() const
{
    return std::max(verts[0].py, std::max(verts[1].py, std::max(verts[2].py, verts[3].py)));
}

inline QRectF WindowQuad::bounds() const
{
    return QRectF(QPointF(left(), top()), QPointF(right(), bottom()));
}

} // namespace KWin
