/*
    SPDX-FileCopyrightText: 2023 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "core/graphicsbuffer.h"

#include <QImage>

namespace KWin
{

class KWIN_EXPORT GraphicsBufferView
{
public:
    GraphicsBufferView();
    explicit GraphicsBufferView(GraphicsBuffer *buffer, GraphicsBuffer::MapFlags accessFlags = GraphicsBuffer::Read);
    ~GraphicsBufferView();

    bool isNull() const;
    QImage *image();
    const QImage *image() const;

private:
    GraphicsBuffer *m_buffer = nullptr;
    QImage m_image;
};

} // namespace KWin
