/*
    SPDX-FileCopyrightText: 2026 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QSize>

class QImage;

namespace KWin
{

class GraphicsBuffer;
class Rect;
class Region;

class Texture
{
public:
    virtual ~Texture();

    QSize size() const;
    bool isFloatingPoint() const;

    virtual void attach(GraphicsBuffer *buffer, const Region &region) = 0;
    virtual void upload(const QImage &image, const Rect &region) = 0;

protected:
    Texture();

    QSize m_size;
    bool m_isFloatingPoint = false;
};

} // namespace KWin
