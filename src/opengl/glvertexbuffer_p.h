/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"

#include <epoxy/gl.h>
#include <vector>

namespace KWin
{

class KWIN_EXPORT IndexBuffer
{
public:
    explicit IndexBuffer();
    ~IndexBuffer();

    void accommodate(size_t count);
    void bind();

private:
    GLuint m_buffer;
    size_t m_count = 0;
    std::vector<uint16_t> m_data;
};

}
