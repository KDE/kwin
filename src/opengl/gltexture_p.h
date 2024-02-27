/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2011 Philipp Knechtges <philipp-dev@knechtges.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "opengl/glutils.h"

#include <QImage>
#include <QMatrix4x4>
#include <QSharedData>
#include <QSize>
#include <epoxy/gl.h>

namespace KWin
{
// forward declarations
class GLVertexBuffer;

class KWIN_EXPORT GLTexturePrivate
    : public QSharedData
{
public:
    GLTexturePrivate();
    virtual ~GLTexturePrivate();

    void updateMatrix();

    GLuint m_texture;
    GLenum m_target;
    GLenum m_internalFormat;
    GLenum m_filter;
    GLenum m_wrapMode;
    QSize m_size;
    QSizeF m_scale; // to un-normalize GL_TEXTURE_2D
    QMatrix4x4 m_matrix[2];
    OutputTransform m_textureToBufferTransform;
    bool m_canUseMipmaps;
    bool m_markedDirty;
    bool m_filterChanged;
    bool m_wrapModeChanged;
    bool m_owning;
    int m_mipLevels;

    int m_unnormalizeActive; // 0 - no, otherwise refcount
    int m_normalizeActive; // 0 - no, otherwise refcount
    std::unique_ptr<GLVertexBuffer> m_vbo;
    QSizeF m_cachedSize;
    QRectF m_cachedSource;
    OutputTransform m_cachedContentTransform;

    Q_DISABLE_COPY(GLTexturePrivate)
};

} // namespace
