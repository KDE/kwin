/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "openglcontext.h"
#include "glframebuffer.h"
#include "glplatform.h"
#include "glshader.h"
#include "glshadermanager.h"
#include "glvertexbuffer.h"
#include "utils/common.h"

#include <QByteArray>
#include <QList>

#include <epoxy/gl.h>

namespace KWin
{

OpenGlContext *OpenGlContext::s_currentContext = nullptr;

static QSet<QByteArray> getExtensions(OpenGlContext *context)
{
    QSet<QByteArray> ret;
    if (!context->isOpenGLES() && context->hasVersion(Version(3, 0))) {
        int count;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);

        for (int i = 0; i < count; i++) {
            const char *name = (const char *)glGetStringi(GL_EXTENSIONS, i);
            ret.insert(name);
        }
    } else {
        const QByteArray extensions = (const char *)glGetString(GL_EXTENSIONS);
        QList<QByteArray> extensionsList = extensions.split(' ');
        ret = {extensionsList.constBegin(), extensionsList.constEnd()};
    }
    return ret;
}

static bool checkTextureSwizzleSupport(OpenGlContext *context)
{
    if (context->isOpenGLES()) {
        return context->hasVersion(Version(3, 0));
    } else {
        return context->hasVersion(Version(3, 3)) || context->hasOpenglExtension(QByteArrayLiteral("GL_ARB_texture_swizzle"));
    }
}

static bool checkTextureStorageSupport(OpenGlContext *context)
{
    if (context->isOpenGLES()) {
        return context->hasVersion(Version(3, 0)) || context->hasOpenglExtension(QByteArrayLiteral("GL_EXT_texture_storage"));
    } else {
        return context->hasVersion(Version(4, 2)) || context->hasOpenglExtension(QByteArrayLiteral("GL_ARB_texture_storage"));
    }
}

static bool checkIndexedQuads(OpenGlContext *context)
{
    if (context->isOpenGLES()) {
        const bool haveBaseVertex = context->hasOpenglExtension(QByteArrayLiteral("GL_OES_draw_elements_base_vertex"));
        const bool haveCopyBuffer = context->hasVersion(Version(3, 0));
        return haveBaseVertex && haveCopyBuffer && context->hasMapBufferRange();
    } else {
        bool haveBaseVertex = context->hasVersion(Version(3, 2)) || context->hasOpenglExtension(QByteArrayLiteral("GL_ARB_draw_elements_base_vertex"));
        bool haveCopyBuffer = context->hasVersion(Version(3, 1)) || context->hasOpenglExtension(QByteArrayLiteral("GL_ARB_copy_buffer"));
        return haveBaseVertex && haveCopyBuffer && context->hasMapBufferRange();
    }
}

OpenGlContext::OpenGlContext(bool EGL)
    : m_versionString((const char *)glGetString(GL_VERSION))
    , m_version(Version::parseString(m_versionString))
    , m_glslVersionString((const char *)glGetString(GL_SHADING_LANGUAGE_VERSION))
    , m_glslVersion(Version::parseString(m_glslVersionString))
    , m_vendor((const char *)glGetString(GL_VENDOR))
    , m_renderer((const char *)glGetString(GL_RENDERER))
    , m_isOpenglES(m_versionString.startsWith("OpenGL ES"))
    , m_extensions(getExtensions(this))
    , m_supportsTimerQueries(checkTimerQuerySupport())
    , m_supportsTextureStorage(checkTextureStorageSupport(this))
    , m_supportsTextureSwizzle(checkTextureSwizzleSupport(this))
    , m_supportsARGB32Textures(!m_isOpenglES || hasOpenglExtension(QByteArrayLiteral("GL_EXT_texture_format_BGRA8888")))
    , m_supportsRGTextures(hasVersion(Version(3, 0)) || hasOpenglExtension(QByteArrayLiteral("GL_ARB_texture_rg")) || hasOpenglExtension(QByteArrayLiteral("GL_EXT_texture_rg")))
    , m_supports16BitTextures(!m_isOpenglES || hasOpenglExtension(QByteArrayLiteral("GL_EXT_texture_norm16")))
    , m_supportsBlits(!m_isOpenglES || hasVersion(Version(3, 0)))
    , m_supportsPackedDepthStencil(hasVersion(Version(3, 0)) || hasOpenglExtension(QByteArrayLiteral("GL_OES_packed_depth_stencil")) || hasOpenglExtension(QByteArrayLiteral("GL_ARB_framebuffer_object")) || hasOpenglExtension(QByteArrayLiteral("GL_EXT_packed_depth_stencil")))
    , m_supportsGLES24BitDepthBuffers(m_isOpenglES && (hasVersion(Version(3, 0)) || hasOpenglExtension(QByteArrayLiteral("GL_OES_depth24"))))
    , m_hasMapBufferRange(hasVersion(Version(3, 0)) || hasOpenglExtension(QByteArrayLiteral("GL_EXT_map_buffer_range")) || hasOpenglExtension(QByteArrayLiteral("GL_ARB_map_buffer_range")))
    , m_haveBufferStorage((!m_isOpenglES && hasVersion(Version(4, 4))) || hasOpenglExtension(QByteArrayLiteral("GL_ARB_buffer_storage")) || hasOpenglExtension(QByteArrayLiteral("GL_EXT_buffer_storage")))
    , m_haveSyncFences((m_isOpenglES && hasVersion(Version(3, 0))) || (!m_isOpenglES && hasVersion(Version(3, 2))) || hasOpenglExtension(QByteArrayLiteral("GL_ARB_sync")))
    , m_supportsIndexedQuads(checkIndexedQuads(this))
    , m_supportsPackInvert(hasOpenglExtension(QByteArrayLiteral("GL_MESA_pack_invert")))
    , m_glPlatform(std::make_unique<GLPlatform>(EGL ? EglPlatformInterface : GlxPlatformInterface, m_versionString, m_glslVersionString, m_renderer, m_vendor))
{
}

OpenGlContext::~OpenGlContext()
{
    if (s_currentContext == this) {
        s_currentContext = nullptr;
    }
}

bool OpenGlContext::checkTimerQuerySupport() const
{
    if (qEnvironmentVariableIsSet("KWIN_NO_TIMER_QUERY")) {
        return false;
    }
    if (m_isOpenglES) {
        // 3.0 is required so query functions can be used without "EXT" suffix.
        // Timer queries are still not part of the core OpenGL ES specification.
        return openglVersion() >= Version(3, 0) && hasOpenglExtension("GL_EXT_disjoint_timer_query");
    } else {
        return openglVersion() >= Version(3, 3) || hasOpenglExtension("GL_ARB_timer_query");
    }
}

bool OpenGlContext::hasVersion(const Version &version) const
{
    return m_version >= version;
}

QByteArrayView OpenGlContext::openglVersionString() const
{
    return m_versionString;
}

Version OpenGlContext::openglVersion() const
{
    return m_version;
}

QByteArrayView OpenGlContext::glslVersionString() const
{
    return m_glslVersionString;
}

Version OpenGlContext::glslVersion() const
{
    return m_glslVersion;
}

QByteArrayView OpenGlContext::vendor() const
{
    return m_vendor;
}

QByteArrayView OpenGlContext::renderer() const
{
    return m_renderer;
}

bool OpenGlContext::isOpenGLES() const
{
    return m_isOpenglES;
}

bool OpenGlContext::hasOpenglExtension(QByteArrayView name) const
{
    return std::any_of(m_extensions.cbegin(), m_extensions.cend(), [name](const auto &string) {
        return string == name;
    });
}

bool OpenGlContext::isSoftwareRenderer() const
{
    return m_renderer.contains("softpipe") || m_renderer.contains("Software Rasterizer") || m_renderer.contains("llvmpipe");
}

bool OpenGlContext::supportsTimerQueries() const
{
    return m_supportsTimerQueries;
}

bool OpenGlContext::supportsTextureStorage() const
{
    return m_supportsTextureStorage;
}

bool OpenGlContext::supportsTextureSwizzle() const
{
    return m_supportsTextureSwizzle;
}

bool OpenGlContext::supportsARGB32Textures() const
{
    return m_supportsARGB32Textures;
}

bool OpenGlContext::supportsRGTextures() const
{
    return m_supportsRGTextures;
}

bool OpenGlContext::supports16BitTextures() const
{
    return m_supports16BitTextures;
}

bool OpenGlContext::supportsBlits() const
{
    return m_supportsBlits;
}

bool OpenGlContext::supportsGLES24BitDepthBuffers() const
{
    return m_supportsGLES24BitDepthBuffers;
}

bool OpenGlContext::haveBufferStorage() const
{
    return m_haveBufferStorage;
}

bool OpenGlContext::hasMapBufferRange() const
{
    return m_hasMapBufferRange;
}

bool OpenGlContext::haveSyncFences() const
{
    return m_haveSyncFences;
}

bool OpenGlContext::supportsPackInvert() const
{
    return m_supportsPackInvert;
}

ShaderManager *OpenGlContext::shaderManager() const
{
    return m_shaderManager;
}

GLVertexBuffer *OpenGlContext::streamingVbo() const
{
    return m_streamingBuffer;
}

IndexBuffer *OpenGlContext::indexBuffer() const
{
    return m_indexBuffer;
}

GLPlatform *OpenGlContext::glPlatform() const
{
    return m_glPlatform.get();
}

bool OpenGlContext::checkSupported() const
{
    const bool supportsGLSL = m_isOpenglES || (hasOpenglExtension("GL_ARB_shader_objects") && hasOpenglExtension("GL_ARB_fragment_shader") && hasOpenglExtension("GL_ARB_vertex_shader"));
    const bool supportsNonPowerOfTwoTextures = m_isOpenglES || hasOpenglExtension("GL_ARB_texture_non_power_of_two");
    const bool supports3DTextures = !m_isOpenglES || hasVersion(Version(3, 0)) || hasOpenglExtension("GL_OES_texture_3D");
    const bool supportsFBOs = m_isOpenglES || hasVersion(Version(3, 0)) || hasOpenglExtension("GL_ARB_framebuffer_object") || hasOpenglExtension(QByteArrayLiteral("GL_EXT_framebuffer_object"));
    const bool supportsUnpack = !m_isOpenglES || hasOpenglExtension(QByteArrayLiteral("GL_EXT_unpack_subimage"));

    if (!supportsGLSL || !supportsNonPowerOfTwoTextures || !supports3DTextures || !supportsFBOs || !supportsUnpack) {
        return false;
    }
    // some old hardware only supports very limited shaders. To prevent the shaders KWin uses later on from not working,
    // test a reasonably complex one here and bail out early if it doesn't work
    auto shader = m_shaderManager->shader(ShaderTrait::MapTexture | ShaderTrait::TransformColorspace | ShaderTrait::AdjustSaturation | ShaderTrait::Modulate);
    return shader->isValid();
}

void OpenGlContext::setShaderManager(ShaderManager *manager)
{
    m_shaderManager = manager;
}

void OpenGlContext::setStreamingBuffer(GLVertexBuffer *vbo)
{
    m_streamingBuffer = vbo;
    if (haveBufferStorage() && haveSyncFences()) {
        if (qgetenv("KWIN_PERSISTENT_VBO") != QByteArrayLiteral("0")) {
            vbo->setPersistent();
        }
    }
}

void OpenGlContext::setIndexBuffer(IndexBuffer *buffer)
{
    m_indexBuffer = buffer;
}

QSet<QByteArray> OpenGlContext::openglExtensions() const
{
    return m_extensions;
}

OpenGlContext *OpenGlContext::currentContext()
{
    return s_currentContext;
}

void OpenGlContext::glResolveFunctions(const std::function<resolveFuncPtr(const char *)> &resolveFunction)
{
    const bool haveArbRobustness = hasOpenglExtension(QByteArrayLiteral("GL_ARB_robustness"));
    const bool haveExtRobustness = hasOpenglExtension(QByteArrayLiteral("GL_EXT_robustness"));
    bool robustContext = false;
    if (isOpenGLES()) {
        if (haveExtRobustness) {
            GLint value = 0;
            glGetIntegerv(GL_CONTEXT_ROBUST_ACCESS_EXT, &value);
            robustContext = (value != 0);
        }
    } else {
        if (haveArbRobustness) {
            if (hasVersion(Version(3, 0))) {
                GLint value = 0;
                glGetIntegerv(GL_CONTEXT_FLAGS, &value);
                if (value & GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT_ARB) {
                    robustContext = true;
                }
            } else {
                robustContext = true;
            }
        }
    }
    if (robustContext && haveArbRobustness) {
        // See https://www.opengl.org/registry/specs/ARB/robustness.txt
        m_glGetGraphicsResetStatus = (glGetGraphicsResetStatus_func)resolveFunction("glGetGraphicsResetStatusARB");
        m_glReadnPixels = (glReadnPixels_func)resolveFunction("glReadnPixelsARB");
        m_glGetnTexImage = (glGetnTexImage_func)resolveFunction("glGetnTexImageARB");
        m_glGetnUniformfv = (glGetnUniformfv_func)resolveFunction("glGetnUniformfvARB");
    } else if (robustContext && haveExtRobustness) {
        // See https://www.khronos.org/registry/gles/extensions/EXT/EXT_robustness.txt
        m_glGetGraphicsResetStatus = (glGetGraphicsResetStatus_func)resolveFunction("glGetGraphicsResetStatusEXT");
        m_glReadnPixels = (glReadnPixels_func)resolveFunction("glReadnPixelsEXT");
        m_glGetnUniformfv = (glGetnUniformfv_func)resolveFunction("glGetnUniformfvEXT");
    }
}

void OpenGlContext::initDebugOutput()
{
    const bool have_KHR_debug = hasOpenglExtension(QByteArrayLiteral("GL_KHR_debug"));
    const bool have_ARB_debug = hasOpenglExtension(QByteArrayLiteral("GL_ARB_debug_output"));
    if (!have_KHR_debug && !have_ARB_debug) {
        return;
    }

    if (!have_ARB_debug) {
        // if we don't have ARB debug, but only KHR debug we need to verify whether the context is a debug context
        // it should work without as well, but empirical tests show: no it doesn't
        if (isOpenGLES()) {
            if (!hasVersion(Version(3, 2))) {
                // empirical data shows extension doesn't work
                return;
            }
        } else if (!hasVersion(Version(3, 0))) {
            return;
        }
        // can only be queried with either OpenGL >= 3.0 or OpenGL ES of at least 3.1
        GLint value = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &value);
        if (!(value & GL_CONTEXT_FLAG_DEBUG_BIT)) {
            return;
        }
    }

    // Set the callback function
    auto callback = [](GLenum source, GLenum type, GLuint id,
                       GLenum severity, GLsizei length,
                       const GLchar *message,
                       const GLvoid *userParam) {
        while (length && std::isspace(message[length - 1])) {
            --length;
        }

        switch (type) {
        case GL_DEBUG_TYPE_ERROR:
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            qCWarning(KWIN_OPENGL, "%#x: %.*s", id, length, message);
            break;

        case GL_DEBUG_TYPE_OTHER:
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        case GL_DEBUG_TYPE_PORTABILITY:
        case GL_DEBUG_TYPE_PERFORMANCE:
        default:
            qCDebug(KWIN_OPENGL, "%#x: %.*s", id, length, message);
            break;
        }
    };

    glDebugMessageCallback(callback, nullptr);

    // This state exists only in GL_KHR_debug
    if (have_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
    }

    if (qEnvironmentVariableIntValue("KWIN_GL_DEBUG")) {
        // Enable all debug messages
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        // Insert a test message
        const QByteArray message = QByteArrayLiteral("OpenGL debug output initialized");
        glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 0,
                             GL_DEBUG_SEVERITY_LOW, message.length(), message.constData());
    } else {
        // Only enable error messages
        glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    }
}

GLenum OpenGlContext::checkGraphicsResetStatus()
{
    if (m_glGetGraphicsResetStatus) {
        return m_glGetGraphicsResetStatus();
    } else {
        return GL_NO_ERROR;
    }
}

void OpenGlContext::glReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, GLvoid *data)
{
    if (m_glReadnPixels) {
        m_glReadnPixels(x, y, width, height, format, type, bufSize, data);
    } else {
        glReadPixels(x, y, width, height, format, type, data);
    }
}

void OpenGlContext::glGetnTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels)
{
    if (m_glGetnTexImage) {
        m_glGetnTexImage(target, level, format, type, bufSize, pixels);
    } else {
        glGetTexImage(target, level, format, type, pixels);
    }
}

void OpenGlContext::glGetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params)
{
    if (m_glGetnUniformfv) {
        m_glGetnUniformfv(program, location, bufSize, params);
    } else {
        glGetUniformfv(program, location, params);
    }
}

void OpenGlContext::pushFramebuffer(GLFramebuffer *fbo)
{
    if (fbo != currentFramebuffer()) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo->handle());
        glViewport(0, 0, fbo->size().width(), fbo->size().height());
    }
    m_fbos.push(fbo);
}

GLFramebuffer *OpenGlContext::popFramebuffer()
{
    const auto ret = m_fbos.pop();
    if (const auto fbo = currentFramebuffer(); fbo != ret) {
        if (fbo) {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo->handle());
            glViewport(0, 0, fbo->size().width(), fbo->size().height());
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }
    return ret;
}

GLFramebuffer *OpenGlContext::currentFramebuffer()
{
    return m_fbos.empty() ? nullptr : m_fbos.top();
}
}
