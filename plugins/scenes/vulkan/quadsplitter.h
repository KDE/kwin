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
     * Fixed size WindowQuad array
     */
    class Array
    {
    public:
        Array() = default;
        Array(WindowQuad *data, size_t count) : m_data(data), m_count(count) {}

        size_t count() const { return m_count; }
        bool isEmpty() const { return m_count == 0; }

        // STL compatibility
        size_t size() const { return m_count; }
        bool empty() const { return m_count == 0; }

        const WindowQuad &first() const { return m_data[0]; }
        const WindowQuad &last() const { return m_data[m_count - 1]; }
        const WindowQuad *cbegin() const { return m_data; }
        const WindowQuad *cend() const { return m_data + m_count; }
        const WindowQuad *begin() const { return m_data; }
        const WindowQuad *end() const { return m_data + m_count; }

        WindowQuad &at(size_t index) { return m_data[index]; }
        const WindowQuad &at(size_t index) const { return m_data[index]; }

        WindowQuad &operator [](size_t index) { return m_data[index]; }
        const WindowQuad &operator [](size_t index) const { return m_data[index]; }

        void writeVertices(GLVertex2D *dst) const {
            for (const WindowQuad &quad : *this) {
                for (int i = 0; i < 4; i++) {
                    const WindowVertex &vertex = quad[i];

                    *dst++ = GLVertex2D {
                        .position = { float(vertex.x()), float(vertex.y()) },
                        .texcoord = { float(vertex.u()), float(vertex.v()) }
                    };
                }
            }
        }

    private:
        WindowQuad *m_data;
        size_t m_count;
    };

    QuadSplitter(const WindowQuadList &quads);
    ~QuadSplitter();

    size_t maxQuadCount() const { return m_maxQuadCount; }

    const Array &shadow() const { return m_shadow; }
    const Array &decoration() const { return m_decoration; }
    const Array &content() const { return m_content; }

private:
    WindowQuad *m_data;
    size_t m_maxQuadCount;
    Array m_shadow;
    Array m_decoration;
    Array m_content;
};


} // namespace KWin

#endif
