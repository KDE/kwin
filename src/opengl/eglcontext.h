/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "opengl/gltexture.h"
#include "utils/version.h"

#include <QByteArray>
#include <QList>
#include <QStack>
#include <epoxy/egl.h>

namespace KWin
{

class EglDisplay;
class ShaderManager;
class IndexBuffer;
class GLPlatform;
class GLFramebuffer;
struct DmaBufAttributes;

// GL_ARB_robustness / GL_EXT_robustness
using glGetGraphicsResetStatus_func = GLenum (*)();
using glReadnPixels_func = void (*)(GLint x, GLint y, GLsizei width, GLsizei height,
                                    GLenum format, GLenum type, GLsizei bufSize, GLvoid *data);
using glGetnTexImage_func = void (*)(GLenum target, GLint level, GLenum format, GLenum type,
                                     GLsizei bufSize, void *pixels);
using glGetnUniformfv_func = void (*)(GLuint program, GLint location, GLsizei bufSize, GLfloat *params);

class KWIN_EXPORT EglContext
{
public:
    EglContext(EglDisplay *display, EGLConfig config, ::EGLContext context);
    ~EglContext();

    bool makeCurrent();
    bool makeCurrent(EGLSurface surface);
    void doneCurrent() const;
    std::shared_ptr<GLTexture> importDmaBufAsTexture(const DmaBufAttributes &attributes) const;

    EglDisplay *displayObject() const;
    ::EGLContext handle() const;
    EGLConfig config() const;
    bool isValid() const;

    bool hasVersion(const Version &version) const;

    QByteArrayView openglVersionString() const;
    Version openglVersion() const;
    QByteArrayView glslVersionString() const;
    Version glslVersion() const;
    QByteArrayView vendor() const;
    QByteArrayView renderer() const;
    bool isOpenGLES() const;
    bool hasOpenglExtension(QByteArrayView name) const;
    bool isSoftwareRenderer() const;
    bool supportsTimerQueries() const;
    bool supportsTextureSwizzle() const;
    bool supportsTextureStorage() const;
    bool supportsARGB32Textures() const;
    bool supportsRGTextures() const;
    bool supports16BitTextures() const;
    bool supportsBlits() const;
    bool supportsGLES24BitDepthBuffers() const;
    bool hasMapBufferRange() const;
    bool haveBufferStorage() const;
    bool haveSyncFences() const;
    bool supportsPackInvert() const;
    ShaderManager *shaderManager() const;
    GLVertexBuffer *streamingVbo() const;
    IndexBuffer *indexBuffer() const;
    GLPlatform *glPlatform() const;
    QSet<QByteArray> openglExtensions() const;

    /**
     * checks whether or not this context supports all the features that KWin requires
     */
    bool checkSupported() const;

    GLenum checkGraphicsResetStatus();
    void glReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, GLvoid *data);
    void glGetnTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels);
    void glGetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params);

    void pushFramebuffer(GLFramebuffer *fbo);
    GLFramebuffer *popFramebuffer();
    GLFramebuffer *currentFramebuffer();

    static EglContext *currentContext();
    static std::unique_ptr<EglContext> create(EglDisplay *display, EGLConfig config, ::EGLContext sharedContext);

private:
    static ::EGLContext createContext(EglDisplay *display, EGLConfig config, ::EGLContext sharedContext);
    bool checkTimerQuerySupport() const;
    void setShaderManager(ShaderManager *manager);
    void setStreamingBuffer(GLVertexBuffer *vbo);
    void setIndexBuffer(IndexBuffer *buffer);
    typedef void (*resolveFuncPtr)();
    void glResolveFunctions(const std::function<resolveFuncPtr(const char *)> &resolveFunction);
    void initDebugOutput();

    static EglContext *s_currentContext;

    const QByteArrayView m_versionString;
    const Version m_version;
    const QByteArrayView m_glslVersionString;
    const Version m_glslVersion;
    const QByteArrayView m_vendor;
    const QByteArrayView m_renderer;
    const bool m_isOpenglES;
    const QSet<QByteArray> m_extensions;
    const bool m_supportsTimerQueries;
    const bool m_supportsTextureStorage;
    const bool m_supportsTextureSwizzle;
    const bool m_supportsARGB32Textures;
    const bool m_supportsRGTextures;
    const bool m_supports16BitTextures;
    const bool m_supportsBlits;
    const bool m_supportsPackedDepthStencil;
    const bool m_supportsGLES24BitDepthBuffers;
    const bool m_hasMapBufferRange;
    const bool m_haveBufferStorage;
    const bool m_haveSyncFences;
    const bool m_supportsIndexedQuads;
    const bool m_supportsPackInvert;
    const std::unique_ptr<GLPlatform> m_glPlatform;
    glGetGraphicsResetStatus_func m_glGetGraphicsResetStatus = nullptr;
    glReadnPixels_func m_glReadnPixels = nullptr;
    glGetnTexImage_func m_glGetnTexImage = nullptr;
    glGetnUniformfv_func m_glGetnUniformfv = nullptr;

    EglDisplay *const m_display;
    const ::EGLContext m_handle;
    const EGLConfig m_config;
    std::unique_ptr<ShaderManager> m_shaderManager;
    std::unique_ptr<GLVertexBuffer> m_streamingBuffer;
    std::unique_ptr<IndexBuffer> m_indexBuffer;
    QStack<GLFramebuffer *> m_fbos;
    uint32_t m_vao = 0;
};

}
