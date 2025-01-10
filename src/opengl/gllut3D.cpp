/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "gllut3D.h"
#include "openglcontext.h"
#include "utils/common.h"

#include <vector>

namespace KWin
{

GlLookUpTable3D::GlLookUpTable3D(GLuint handle, size_t xSize, size_t ySize, size_t zSize)
    : m_handle(handle)
    , m_xSize(xSize)
    , m_ySize(ySize)
    , m_zSize(zSize)
{
}

GlLookUpTable3D::~GlLookUpTable3D()
{
    if (!OpenGlContext::currentContext()) {
        qCWarning(KWIN_OPENGL, "Could not delete 3D LUT because no context is current");
        return;
    }
    glDeleteTextures(1, &m_handle);
}

GLuint GlLookUpTable3D::handle() const
{
    return m_handle;
}

size_t GlLookUpTable3D::xSize() const
{
    return m_xSize;
}

size_t GlLookUpTable3D::ySize() const
{
    return m_ySize;
}

size_t GlLookUpTable3D::zSize() const
{
    return m_zSize;
}

void GlLookUpTable3D::bind()
{
    glBindTexture(GL_TEXTURE_3D, m_handle);
}

std::unique_ptr<GlLookUpTable3D> GlLookUpTable3D::create(const std::function<QVector3D(size_t x, size_t y, size_t z)> &mapping, size_t xSize, size_t ySize, size_t zSize)
{
    GLuint handle = 0;
    glGenTextures(1, &handle);
    if (!handle) {
        return nullptr;
    }
    glBindTexture(GL_TEXTURE_3D, handle);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_LOD, 0);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LOD, 0);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    QVector<float> data;
    data.reserve(4 * xSize * ySize * zSize);
    for (size_t z = 0; z < zSize; z++) {
        for (size_t y = 0; y < ySize; y++) {
            for (size_t x = 0; x < xSize; x++) {
                const auto color = mapping(x, y, z);
                data.push_back(color.x());
                data.push_back(color.y());
                data.push_back(color.z());
                data.push_back(1);
            }
        }
    }
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, xSize, ySize, zSize, 0, GL_RGBA, GL_FLOAT, data.data());
    glBindTexture(GL_TEXTURE_3D, 0);
    return std::make_unique<GlLookUpTable3D>(handle, xSize, ySize, zSize);
}

}
