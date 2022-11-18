/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cursorsource.h"

namespace KWin
{

CursorSource::CursorSource(QObject *parent)
    : QObject(parent)
{
}

QImage CursorSource::image() const
{
    return m_image;
}

QPoint CursorSource::hotspot() const
{
    return m_hotspot;
}

ImageCursorSource::ImageCursorSource(QObject *parent)
    : CursorSource(parent)
{
}

void ImageCursorSource::update(const QImage &image, const QPoint &hotspot)
{
    m_image = image;
    m_hotspot = hotspot;
    Q_EMIT changed();
}

} // namespace KWin
