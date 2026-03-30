/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scene/texture.h"
#include "core/syncobjtimeline.h"

namespace KWin
{

Texture::Texture()
{
}

Texture::~Texture()
{
}

QSize Texture::size() const
{
    return m_size;
}

bool Texture::isFloatingPoint() const
{
    return m_isFloatingPoint;
}

std::shared_ptr<SyncReleasePoint> Texture::releasePoint() const
{
    return m_releasePoint;
}

} // namespace KWin
