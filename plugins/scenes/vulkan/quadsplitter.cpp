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

#include "quadsplitter.h"


namespace KWin {


QuadSplitter::QuadSplitter(const WindowQuadList &quads)
{
    size_t shadowCount = 0;
    size_t decorationCount = 0;
    size_t contentCount = 0;

    // Count the quads
    for (const WindowQuad &quad : qAsConst(quads)) {
        switch (quad.type()) {
        case WindowQuadContents:
            contentCount++;
            continue;

        case WindowQuadDecoration:
            decorationCount++;
            continue;

        case WindowQuadShadow:
            shadowCount++;
            continue;

        default:
            continue;
        }
    }

    m_maxQuadCount = std::max(shadowCount, std::max(decorationCount, contentCount));

    // Allocate space
    size_t count = shadowCount + decorationCount + contentCount;
    m_data = static_cast<WindowQuad *>(std::malloc(count * sizeof(WindowQuad)));

    uintptr_t shadowOffset     = 0;
    uintptr_t decorationOffset = shadowOffset + shadowCount;
    uintptr_t contentOffset    = decorationOffset + decorationCount;

    m_shadow     = { m_data + shadowOffset,     shadowCount     };
    m_decoration = { m_data + decorationOffset, decorationCount };
    m_content    = { m_data + contentOffset,    contentCount    };

    // Split the quads
    for (const WindowQuad &quad : qAsConst(quads)) {
        switch (quad.type()) {
        case WindowQuadContents:
            m_data[contentOffset++] = quad;
            continue;

        case WindowQuadDecoration:
            m_data[decorationOffset++] = quad;
            continue;

        case WindowQuadShadow:
            m_data[shadowOffset++] = quad;
            continue;

        default:
            continue;
        }
    }
}


QuadSplitter::~QuadSplitter()
{
    std::free(m_data);
}


} // namespace KWin
