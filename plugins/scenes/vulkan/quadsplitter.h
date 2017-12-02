/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright © 2017-2018 Fredrik Höglund <fredrik@kde.org>

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

#ifndef QUADSPLITTER_H
#define QUADSPLITTER_H

#include "kwineffects.h"


namespace KWin {


/**
 * Splits a WindowQuadList into separate fixed size arrays for the shadow, decoration and content quads, respectively.
 *
 * Storage for the arrays is allocated as a single contiguous memory allocation,
 * owned by the QuadSplitter.
 */
class QuadSplitter
{
public:
    /**
     * Vertex is a version of a WindowVertex that can be copied directly into a vertex buffer.
     *
     * It differs from WindowVertex in that the coordinates are stored as 32-bit single-precision
     * floating point values, and it does not have the original position.
     */
    struct Vertex : public GLVertex2D
    {
        Vertex(const WindowVertex &other) {
            position = { float(other.x()), float(other.y())};
            texcoord = { float(other.u()), float(other.v())};
        }

        float x() const { return position.x(); }
        float y() const { return position.y(); }
        float u() const { return texcoord.x(); }
        float v() const { return texcoord.y(); }
    };


    /**
     * Quad is a version of a WindowQuad that can be copied directly into a vertex buffer.
     *
     * It differs from WindowQuad in that it contains 32-bit single-precision floating point
     * vertices, and is missing the type and quad ID.
     */
    class Quad
    {
    public:
        Quad(const WindowQuad &other)
            : m_vertices{other[0], other[1], other[2], other[3]}
        {
        }

        const Vertex &operator [](size_t index) const { return m_vertices[index]; }

    private:
        Vertex m_vertices[4];
    };


    /**
     * Fixed size Quad array
     */
    class Array
    {
    public:
        Array() = default;
        Array(Quad *data, size_t count) : m_data(data), m_count(count) {}

        size_t count() const { return m_count; }
        bool isEmpty() const { return m_count == 0; }

        // For STL compatibility
        size_t size() const { return m_count; }
        bool empty() const { return m_count == 0; }

        const Quad &first() const { return m_data[0]; }
        const Quad &last() const { return m_data[m_count - 1]; }
        const Quad *cbegin() const { return m_data; }
        const Quad *cend() const { return m_data + m_count; }
        const Quad *begin() const { return m_data; }
        const Quad *end() const { return m_data + m_count; }

        Quad &at(size_t index) { return m_data[index]; }
        const Quad &at(size_t index) const { return m_data[index]; }

        Quad &operator [](size_t index) { return m_data[index]; }
        const Quad &operator [](size_t index) const { return m_data[index]; }

        void writeVertices(GLVertex2D *dst) const {
            memcpy(dst, m_data, m_count * sizeof(Quad));
        }

    private:
        Quad *m_data;
        size_t m_count;
    };

    QuadSplitter(const WindowQuadList &quads);
    ~QuadSplitter();

    size_t maxQuadCount() const { return m_maxQuadCount; }

    const Array &shadow() const { return m_shadow; }
    const Array &decoration() const { return m_decoration; }
    const Array &content() const { return m_content; }

private:
    Quad *m_data;
    size_t m_maxQuadCount;
    Array m_shadow;
    Array m_decoration;
    Array m_content;
};


} // namespace KWin

#endif
