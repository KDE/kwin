/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "kwin_export.h"
#include "utils/version.h"

#include <stdint.h>
#include <string_view>

#include <QByteArray>
#include <QSet>

namespace KWin
{

class ShaderManager;
class GLFramebuffer;
class GLVertexBuffer;
class IndexBuffer;

class KWIN_EXPORT OpenGlContext
{
public:
    explicit OpenGlContext();
    virtual ~OpenGlContext() = default;

    bool hasVersion(const Version &version) const;

    QByteArrayView openglVersionString() const;
    Version openglVersion() const;
    QByteArrayView vendor() const;
    QByteArrayView renderer() const;
    bool isOpenglES() const;
    bool hasOpenglExtension(QByteArrayView name) const;
    bool isSoftwareRenderer() const;
    bool supportsTimerQueries() const;
    bool supportsTextureSwizzle() const;
    bool supportsTextureStorage() const;
    bool supportsARGB32Textures() const;
    bool supportsTextureUnpack() const;
    bool supportsRGTextures() const;
    bool supports16BitTextures() const;
    bool supportsBlits() const;
    bool supportsGLES24BitDepthBuffers() const;
    bool hasMapBufferRange() const;
    bool haveBufferStorage() const;
    bool haveSyncFences() const;
    ShaderManager *shaderManager() const;
    GLVertexBuffer *streamingVbo() const;
    IndexBuffer *indexBuffer() const;
    QSet<QByteArray> openglExtensions() const;

    /**
     * checks whether or not this context supports all the features that KWin requires
     */
    bool checkSupported() const;

    static OpenGlContext *currentContext();

protected:
    bool checkTimerQuerySupport() const;
    void setShaderManager(ShaderManager *manager);
    void setStreamingBuffer(GLVertexBuffer *vbo);
    void setIndexBuffer(IndexBuffer *buffer);

    static OpenGlContext *s_currentContext;

    const QByteArrayView m_versionString;
    const Version m_version;
    const QByteArrayView m_vendor;
    const QByteArrayView m_renderer;
    const bool m_isOpenglES;
    const QSet<QByteArray> m_extensions;
    const bool m_supportsTimerQueries;
    const bool m_supportsTextureStorage;
    const bool m_supportsTextureSwizzle;
    const bool m_supportsARGB32Textures;
    const bool m_supportsTextureUnpack;
    const bool m_supportsRGTextures;
    const bool m_supports16BitTextures;
    const bool m_supportsBlits;
    const bool m_supportsPackedDepthStencil;
    const bool m_supportsGLES24BitDepthBuffers;
    const bool m_hasMapBufferRange;
    const bool m_haveBufferStorage;
    const bool m_haveSyncFences;
    const bool m_supportsIndexedQuads;
    ShaderManager *m_shaderManager = nullptr;
    GLVertexBuffer *m_streamingBuffer = nullptr;
    IndexBuffer *m_indexBuffer = nullptr;
};

}
