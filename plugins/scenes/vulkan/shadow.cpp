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

#include "shadow.h"

#include <QPainter>

namespace KWin
{


VulkanShadow::VulkanShadow(Toplevel *toplevel, VulkanScene *scene)
    : Shadow(toplevel),
      m_scene(scene)
{
}


VulkanShadow::~VulkanShadow()
{
}


void VulkanShadow::buildQuads()
{
    const QSizeF top         = elementSize(ShadowElementTop);
    const QSizeF topRight    = elementSize(ShadowElementTopRight);
    const QSizeF right       = elementSize(ShadowElementRight);
    const QSizeF bottomRight = elementSize(ShadowElementBottomRight);
    const QSizeF bottom      = elementSize(ShadowElementBottom);
    const QSizeF bottomLeft  = elementSize(ShadowElementBottomLeft);
    const QSizeF left        = elementSize(ShadowElementLeft);
    const QSizeF topLeft     = elementSize(ShadowElementTopLeft);

    const int width = std::max(topLeft  .width(), bottomLeft  .width()) +
                      std::max(top      .width(), bottom      .width()) +
                      std::max(topRight .width(), bottomRight .width());
    const int height = std::max(topLeft    .height(), topRight    .height()) +
                       std::max(left       .height(), right       .height()) +
                       std::max(bottomLeft .height(), bottomRight .height());

    const QRectF src(0, 0, width, height);
    const QRectF dst(QPointF(-leftOffset(), -topOffset()),
                     QPointF(topLevel()->width() + rightOffset(), topLevel()->height() + bottomOffset()));

    const double x[3][4] = {
        { dst.x(), dst.x() + topLeft.width(),    dst.right() - topRight.width(),      dst.right() },  // Top row
        { dst.x(), dst.x() + left.width(),       dst.right() - right.width(),         dst.right() },  // Center row
        { dst.x(), dst.x() + bottomLeft.width(), dst.right() - bottomRight.width(),   dst.right() },  // Bottom row
    };

    const double u[3][4] = {
        { src.x(), src.x() + topLeft.width(),    src.right() - topRight.width(),      src.right() },  // Top row
        { src.x(), src.x() + left.width(),       src.right() - right.width(),         src.right() },  // Center row
        { src.x(), src.x() + bottomLeft.width(), src.right() - bottomRight.width(),   src.right() },  // Bottom row
    };

    const double y[3][4] = {
        { dst.y(), dst.y() + topLeft.height(),   dst.bottom() - bottomLeft.height(),  dst.bottom() }, // Left column
        { dst.y(), dst.y() + top.height(),       dst.bottom() - bottom.height(),      dst.bottom() }, // Center column
        { dst.y(), dst.y() + topRight.height(),  dst.bottom() - bottomRight.height(), dst.bottom() }, // Right column
    };

    const double v[3][4] = {
        { src.y(), src.y() + topLeft.height(),   src.bottom() - bottomLeft.height(),  src.bottom() }, // Left column
        { src.y(), src.y() + top.height(),       src.bottom() - bottom.height(),      src.bottom() }, // Center column
        { src.y(), src.y() + topRight.height(),  src.bottom() - bottomRight.height(), src.bottom() }, // Right column
    };

    m_shadowQuads.clear();

    for (int row = 0; row < 3; row++) {
        for (int column = 0; column < 3; column++) {
            if (row == 1 && column == 1)
                continue;

            WindowQuad quad(WindowQuadShadow);
            quad[0] = WindowVertex(x[row][column + 0], y[column][row + 0], u[row][column + 0], v[column][row + 0]); // Top-left
            quad[1] = WindowVertex(x[row][column + 1], y[column][row + 0], u[row][column + 1], v[column][row + 0]); // Top-right
            quad[2] = WindowVertex(x[row][column + 1], y[column][row + 1], u[row][column + 1], v[column][row + 1]); // Bottom-right
            quad[3] = WindowVertex(x[row][column + 0], y[column][row + 1], u[row][column + 0], v[column][row + 1]); // Bottom-left

            m_shadowQuads.append(quad);
        }
    }
}


// Note that this method gets called between paint passes
bool VulkanShadow::prepareBackend()
{
    QImage image;

    if (!hasDecorationShadow()) {
        const QSize top               = shadowPixmap (ShadowElementTop)         .size();
        const QSize topRight          = shadowPixmap (ShadowElementTopRight)    .size();
        const QSize right             = shadowPixmap (ShadowElementRight)       .size();
        const QSize bottom            = shadowPixmap (ShadowElementBottom)      .size();
        const QSize bottomLeft        = shadowPixmap (ShadowElementBottomLeft)  .size();
        const QSize left              = shadowPixmap (ShadowElementLeft)        .size();
        const QSize topLeft           = shadowPixmap (ShadowElementTopLeft)     .size();
        const QSize bottomRight       = shadowPixmap (ShadowElementBottomRight) .size();

        const int width = std::max(topLeft.width(),  bottomLeft.width()) +
                          std::max(top.width(),      bottom.width()) +
                          std::max(topRight.width(), bottomRight.width());

        const int height = std::max(topRight.height(),   topLeft.height()) +
                           std::max(left.height(),       right.height()) +
                           std::max(bottomLeft.height(), bottomRight.height());

        if (width == 0 || height == 0) {
            return false;
        }

        image = QImage(width, height, QImage::Format_RGBA8888_Premultiplied);
        image.fill(0);

        QPainter p;
        p.begin(&image);

        // Top row
        p.drawPixmap(0,                             0, shadowPixmap(ShadowElementTopLeft));
        p.drawPixmap(topLeft.width(),               0, shadowPixmap(ShadowElementTop));
        p.drawPixmap(topLeft.width() + top.width(), 0, shadowPixmap(ShadowElementTopRight));

        // Center
        p.drawPixmap(0,                     topLeft.height(),  shadowPixmap(ShadowElementLeft));
        p.drawPixmap(width - right.width(), topRight.height(), shadowPixmap(ShadowElementRight));

        // Bottom row
        p.drawPixmap(0,                                   topLeft.height() + left.height(),   shadowPixmap(ShadowElementBottomLeft));
        p.drawPixmap(bottomLeft.width(),                  height - bottom.height(),           shadowPixmap(ShadowElementBottom));
        p.drawPixmap(bottomLeft.width() + bottom.width(), topRight.height() + right.height(), shadowPixmap(ShadowElementBottomRight));

        p.end();
    } else {
        image = decorationShadowImage();
    }

    // Check if the image is alpha-only in practice, and if so convert it to an 8-bpp format
    QImage alphaImage(image.size(), QImage::Format_Alpha8);
    bool alphaOnly = true;

    for (ptrdiff_t y = 0; alphaOnly && y < image.height(); y++) {
        const uint32_t * const src = reinterpret_cast<const uint32_t *>(image.scanLine(y));
        uint8_t * const dst = reinterpret_cast<uint8_t *>(alphaImage.scanLine(y));

        for (ptrdiff_t x = 0; x < image.width(); x++) {
            if (src[x] & 0x00ffffff) {
                alphaOnly = false;
                break;
            }

            dst[x] = qAlpha(src[x]);
        }
    }

    if (alphaOnly) {
        image = alphaImage;
    }

    m_texture = scene()->shadowTextureManager()->lookup(image);
    return true;
}


} // namespace KWin
