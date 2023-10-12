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

class KWIN_EXPORT GlLookUpTable3D
{
public:
    explicit GlLookUpTable3D(GLuint handle, size_t xSize, size_t ySize, size_t zSize);
    ~GlLookUpTable3D();

    GLuint handle() const;
    size_t xSize() const;
    size_t ySize() const;
    size_t zSize() const;

    void bind();

    static std::unique_ptr<GlLookUpTable3D> create(const std::function<QVector3D(size_t x, size_t y, size_t z)> &mapping, size_t xSize, size_t ySize, size_t zSize);

private:
    const GLuint m_handle;
    const size_t m_xSize;
    const size_t m_ySize;
    const size_t m_zSize;
};

}
