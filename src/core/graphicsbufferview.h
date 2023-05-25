/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kwin_export.h"

#include <QImage>

namespace KWin
{

class GraphicsBuffer;

class KWIN_EXPORT GraphicsBufferView
{
public:
    explicit GraphicsBufferView(GraphicsBuffer *buffer);
    ~GraphicsBufferView();

    QImage *image();
    const QImage *image() const;

private:
    GraphicsBuffer *m_buffer;
    QImage m_image;
};

} // namespace KWin
