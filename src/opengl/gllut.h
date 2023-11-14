/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"

#include <QVector3D>
#include <QVector>
#include <epoxy/gl.h>
#include <functional>
#include <memory>

namespace KWin
{

class KWIN_EXPORT GlLookUpTable
{
public:
    explicit GlLookUpTable(GLuint handle, size_t size);
    ~GlLookUpTable();

    GLuint handle() const;
    size_t size() const;

    void bind();

    static std::unique_ptr<GlLookUpTable> create(const std::function<QVector3D(size_t value)> &func, size_t size);

private:
    const GLuint m_handle;
    const size_t m_size;
};

}
