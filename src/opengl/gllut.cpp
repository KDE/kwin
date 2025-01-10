/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "gllut.h"
#include "openglcontext.h"
#include "utils/common.h"

#include <vector>

namespace KWin
{

GlLookUpTable::GlLookUpTable(GLuint handle, size_t size)
    : m_handle(handle)
    , m_size(size)
{
}

GlLookUpTable::~GlLookUpTable()
{
    if (!OpenGlContext::currentContext()) {
        qCWarning(KWIN_OPENGL, "Could not delete 1D LUT because no context is current");
        return;
    }
    glDeleteTextures(1, &m_handle);
}

GLuint GlLookUpTable::handle() const
{
    return m_handle;
}

size_t GlLookUpTable::size() const
{
    return m_size;
}

void GlLookUpTable::bind()
{
    glBindTexture(GL_TEXTURE_2D, m_handle);
}

std::unique_ptr<GlLookUpTable> GlLookUpTable::create(const std::function<QVector3D(size_t value)> &func, size_t size)
{
    GLuint handle = 0;
    glGenTextures(1, &handle);
    if (!handle) {
        return nullptr;
    }
    // this uses 2D textures because OpenGL ES doesn't support 1D textures
    glBindTexture(GL_TEXTURE_2D, handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    std::vector<float> data;
    data.reserve(4 * size);
    for (size_t i = 0; i < size; i++) {
        const auto color = func(i);
        data.push_back(color.x());
        data.push_back(color.y());
        data.push_back(color.z());
        data.push_back(1);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, size, 1, 0, GL_RGBA, GL_FLOAT, data.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return std::make_unique<GlLookUpTable>(handle, size);
}

}
