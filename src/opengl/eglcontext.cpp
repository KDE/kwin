/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "eglcontext.h"
#include "core/graphicsbuffer.h"
#include "egldisplay.h"
#include "eglimagetexture.h"
#include "glframebuffer.h"
#include "glplatform.h"
#include "glshader.h"
#include "glshadermanager.h"
#include "glvertexbuffer.h"
#include "glvertexbuffer_p.h"
#include "opengl/egl_context_attribute_builder.h"
#include "opengl/eglutils_p.h"
#include "opengl/glutils.h"
#include "utils/common.h"
#include "utils/drm_format_helper.h"

#include <QOpenGLContext>
#include <drm_fourcc.h>

namespace KWin
{

EglContext *EglContext::s_currentContext = nullptr;

std::unique_ptr<EglContext> EglContext::create(EglDisplay *display, EGLConfig config, ::EGLContext sharedContext)
{
    auto handle = createContext(display, config, sharedContext);
    if (!handle) {
        return nullptr;
    }
    if (!eglMakeCurrent(display->handle(), EGL_NO_SURFACE, EGL_NO_SURFACE, handle)) {
        eglDestroyContext(display->handle(), handle);
        return nullptr;
    }
    auto ret = std::make_unique<EglContext>(display, config, handle);
    s_currentContext = ret.get();
    if (!ret->checkSupported()) {
        return nullptr;
    }
    return ret;
}

static QSet<QByteArray> getExtensions(EglContext *context)
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

static bool checkTextureSwizzleSupport(EglContext *context)
{
    if (context->isOpenGLES()) {
        return context->hasVersion(Version(3, 0));
    } else {
        return context->hasVersion(Version(3, 3)) || context->hasOpenglExtension(QByteArrayLiteral("GL_ARB_texture_swizzle"));
    }
}

static bool checkTextureStorageSupport(EglContext *context)
{
    if (context->isOpenGLES()) {
        return context->hasVersion(Version(3, 0)) || context->hasOpenglExtension(QByteArrayLiteral("GL_EXT_texture_storage"));
    } else {
        return context->hasVersion(Version(4, 2)) || context->hasOpenglExtension(QByteArrayLiteral("GL_ARB_texture_storage"));
    }
}

static bool checkIndexedQuads(EglContext *context)
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

typedef void (*eglFuncPtr)();
static eglFuncPtr getProcAddress(const char *name)
{
    return eglGetProcAddress(name);
}

EglContext::EglContext(EglDisplay *display, EGLConfig config, ::EGLContext context)
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
    , m_glPlatform(std::make_unique<GLPlatform>(m_versionString, m_glslVersionString, m_renderer, m_vendor))
    , m_display(display)
    , m_handle(context)
    , m_config(config)
    , m_shaderManager(std::make_unique<ShaderManager>())
    , m_streamingBuffer(std::make_unique<GLVertexBuffer>(GLVertexBuffer::Stream))
    , m_indexBuffer(std::make_unique<IndexBuffer>())
{
    glResolveFunctions(&getProcAddress);
    initDebugOutput();
    if (haveBufferStorage() && haveSyncFences()) {
        if (qgetenv("KWIN_PERSISTENT_VBO") != QByteArrayLiteral("0")) {
            m_streamingBuffer->setPersistent();
        }
    }
    // It is not legal to not have a vertex array object bound in a core context
    // to make code handling old and new OpenGL versions easier, bind a dummy vao that's used for everything
    if (!isOpenGLES() && hasOpenglExtension(QByteArrayLiteral("GL_ARB_vertex_array_object"))) {
        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);
    }
}

EglContext::~EglContext()
{
    makeCurrent();
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
    }
    m_shaderManager.reset();
    m_streamingBuffer.reset();
    m_indexBuffer.reset();
    doneCurrent();
    eglDestroyContext(m_display->handle(), m_handle);
}

bool EglContext::makeCurrent()
{
    return makeCurrent(EGL_NO_SURFACE);
}

bool EglContext::makeCurrent(EGLSurface surface)
{
    if (QOpenGLContext *context = QOpenGLContext::currentContext()) {
        // Workaround to tell Qt that no QOpenGLContext is current
        context->doneCurrent();
    }
    const bool ret = eglMakeCurrent(m_display->handle(), surface, surface, m_handle) == EGL_TRUE;
    if (ret) {
        s_currentContext = this;
    }
    return ret;
}

void EglContext::doneCurrent() const
{
    eglMakeCurrent(m_display->handle(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    s_currentContext = nullptr;
}

EglDisplay *EglContext::displayObject() const
{
    return m_display;
}

::EGLContext EglContext::handle() const
{
    return m_handle;
}

EGLConfig EglContext::config() const
{
    return m_config;
}

bool EglContext::isValid() const
{
    return m_display != nullptr && m_handle != EGL_NO_CONTEXT;
}

static inline bool shouldUseOpenGLES()
{
    if (qstrcmp(qgetenv("KWIN_COMPOSE"), "O2ES") == 0) {
        return true;
    }
    return QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES;
}

::EGLContext EglContext::createContext(EglDisplay *display, EGLConfig config, ::EGLContext sharedContext)
{
    const bool haveRobustness = display->hasExtension(QByteArrayLiteral("EGL_EXT_create_context_robustness"));
    const bool haveCreateContext = display->hasExtension(QByteArrayLiteral("EGL_KHR_create_context"));
    const bool haveContextPriority = display->hasExtension(QByteArrayLiteral("EGL_IMG_context_priority"));
    const bool haveResetOnVideoMemoryPurge = display->hasExtension(QByteArrayLiteral("EGL_NV_robustness_video_memory_purge"));

    std::vector<std::unique_ptr<AbstractOpenGLContextAttributeBuilder>> candidates;
    if (shouldUseOpenGLES()) {
        if (haveCreateContext && haveRobustness && haveContextPriority && haveResetOnVideoMemoryPurge) {
            auto glesRobustPriority = std::make_unique<EglOpenGLESContextAttributeBuilder>();
            glesRobustPriority->setResetOnVideoMemoryPurge(true);
            glesRobustPriority->setVersion(2);
            glesRobustPriority->setRobust(true);
            glesRobustPriority->setHighPriority(true);
            candidates.push_back(std::move(glesRobustPriority));
        }

        if (haveCreateContext && haveRobustness && haveContextPriority) {
            auto glesRobustPriority = std::make_unique<EglOpenGLESContextAttributeBuilder>();
            glesRobustPriority->setVersion(2);
            glesRobustPriority->setRobust(true);
            glesRobustPriority->setHighPriority(true);
            candidates.push_back(std::move(glesRobustPriority));
        }
        if (haveCreateContext && haveRobustness) {
            auto glesRobust = std::make_unique<EglOpenGLESContextAttributeBuilder>();
            glesRobust->setVersion(2);
            glesRobust->setRobust(true);
            candidates.push_back(std::move(glesRobust));
        }
        if (haveContextPriority) {
            auto glesPriority = std::make_unique<EglOpenGLESContextAttributeBuilder>();
            glesPriority->setVersion(2);
            glesPriority->setHighPriority(true);
            candidates.push_back(std::move(glesPriority));
        }
        auto gles = std::make_unique<EglOpenGLESContextAttributeBuilder>();
        gles->setVersion(2);
        candidates.push_back(std::move(gles));
    } else {
        if (haveCreateContext) {
            if (haveRobustness && haveContextPriority && haveResetOnVideoMemoryPurge) {
                auto robustCorePriority = std::make_unique<EglContextAttributeBuilder>();
                robustCorePriority->setResetOnVideoMemoryPurge(true);
                robustCorePriority->setVersion(3, 1);
                robustCorePriority->setRobust(true);
                robustCorePriority->setHighPriority(true);
                candidates.push_back(std::move(robustCorePriority));
            }
            if (haveRobustness && haveContextPriority) {
                auto robustCorePriority = std::make_unique<EglContextAttributeBuilder>();
                robustCorePriority->setVersion(3, 1);
                robustCorePriority->setRobust(true);
                robustCorePriority->setHighPriority(true);
                candidates.push_back(std::move(robustCorePriority));
            }
            if (haveRobustness) {
                auto robustCore = std::make_unique<EglContextAttributeBuilder>();
                robustCore->setVersion(3, 1);
                robustCore->setRobust(true);
                candidates.push_back(std::move(robustCore));
            }
            if (haveContextPriority) {
                auto corePriority = std::make_unique<EglContextAttributeBuilder>();
                corePriority->setVersion(3, 1);
                corePriority->setHighPriority(true);
                candidates.push_back(std::move(corePriority));
            }
            auto core = std::make_unique<EglContextAttributeBuilder>();
            core->setVersion(3, 1);
            candidates.push_back(std::move(core));
        }
        if (haveRobustness && haveCreateContext && haveContextPriority) {
            auto robustPriority = std::make_unique<EglContextAttributeBuilder>();
            robustPriority->setRobust(true);
            robustPriority->setHighPriority(true);
            candidates.push_back(std::move(robustPriority));
        }
        if (haveRobustness && haveCreateContext) {
            auto robust = std::make_unique<EglContextAttributeBuilder>();
            robust->setRobust(true);
            candidates.push_back(std::move(robust));
        }
        candidates.emplace_back(new EglContextAttributeBuilder);
    }

    for (const auto &candidate : candidates) {
        const auto attribs = candidate->build();
        ::EGLContext ctx = eglCreateContext(display->handle(), config, sharedContext, attribs.data());
        if (ctx != EGL_NO_CONTEXT) {
            qCDebug(KWIN_OPENGL) << "Created EGL context with attributes:" << candidate.get();
            return ctx;
        }
    }
    qCCritical(KWIN_OPENGL) << "Create Context failed" << getEglErrorString();
    return EGL_NO_CONTEXT;
}

std::shared_ptr<GLTexture> EglContext::importDmaBufAsTexture(const DmaBufAttributes &attributes) const
{
    EGLImageKHR image = m_display->importDmaBufAsImage(attributes);
    if (image != EGL_NO_IMAGE_KHR) {
        const auto info = FormatInfo::get(attributes.format);
        return EGLImageTexture::create(m_display, image, info ? info->openglFormat : GL_RGBA8, QSize(attributes.width, attributes.height), m_display->isExternalOnly(attributes.format, attributes.modifier));
    } else {
        qCWarning(KWIN_OPENGL) << "Error creating EGLImageKHR: " << getEglErrorString();
        return nullptr;
    }
}

bool EglContext::checkTimerQuerySupport() const
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

bool EglContext::hasVersion(const Version &version) const
{
    return m_version >= version;
}

QByteArrayView EglContext::openglVersionString() const
{
    return m_versionString;
}

Version EglContext::openglVersion() const
{
    return m_version;
}

QByteArrayView EglContext::glslVersionString() const
{
    return m_glslVersionString;
}

Version EglContext::glslVersion() const
{
    return m_glslVersion;
}

QByteArrayView EglContext::vendor() const
{
    return m_vendor;
}

QByteArrayView EglContext::renderer() const
{
    return m_renderer;
}

bool EglContext::isOpenGLES() const
{
    return m_isOpenglES;
}

bool EglContext::hasOpenglExtension(QByteArrayView name) const
{
    return std::any_of(m_extensions.cbegin(), m_extensions.cend(), [name](const auto &string) {
        return string == name;
    });
}

bool EglContext::isSoftwareRenderer() const
{
    return m_renderer.contains("softpipe") || m_renderer.contains("Software Rasterizer") || m_renderer.contains("llvmpipe");
}

bool EglContext::supportsTimerQueries() const
{
    return m_supportsTimerQueries;
}

bool EglContext::supportsTextureStorage() const
{
    return m_supportsTextureStorage;
}

bool EglContext::supportsTextureSwizzle() const
{
    return m_supportsTextureSwizzle;
}

bool EglContext::supportsARGB32Textures() const
{
    return m_supportsARGB32Textures;
}

bool EglContext::supportsRGTextures() const
{
    return m_supportsRGTextures;
}

bool EglContext::supports16BitTextures() const
{
    return m_supports16BitTextures;
}

bool EglContext::supportsBlits() const
{
    return m_supportsBlits;
}

bool EglContext::supportsGLES24BitDepthBuffers() const
{
    return m_supportsGLES24BitDepthBuffers;
}

bool EglContext::haveBufferStorage() const
{
    return m_haveBufferStorage;
}

bool EglContext::hasMapBufferRange() const
{
    return m_hasMapBufferRange;
}

bool EglContext::haveSyncFences() const
{
    return m_haveSyncFences;
}

bool EglContext::supportsPackInvert() const
{
    return m_supportsPackInvert;
}

ShaderManager *EglContext::shaderManager() const
{
    return m_shaderManager.get();
}

GLVertexBuffer *EglContext::streamingVbo() const
{
    return m_streamingBuffer.get();
}

IndexBuffer *EglContext::indexBuffer() const
{
    return m_indexBuffer.get();
}

GLPlatform *EglContext::glPlatform() const
{
    return m_glPlatform.get();
}

bool EglContext::checkSupported() const
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

QSet<QByteArray> EglContext::openglExtensions() const
{
    return m_extensions;
}

EglContext *EglContext::currentContext()
{
    return s_currentContext;
}

void EglContext::glResolveFunctions(const std::function<resolveFuncPtr(const char *)> &resolveFunction)
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

void EglContext::initDebugOutput()
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

GLenum EglContext::checkGraphicsResetStatus()
{
    if (m_glGetGraphicsResetStatus) {
        return m_glGetGraphicsResetStatus();
    } else {
        return GL_NO_ERROR;
    }
}

void EglContext::glReadnPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, GLvoid *data)
{
    if (m_glReadnPixels) {
        m_glReadnPixels(x, y, width, height, format, type, bufSize, data);
    } else {
        glReadPixels(x, y, width, height, format, type, data);
    }
}

void EglContext::glGetnTexImage(GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels)
{
    if (m_glGetnTexImage) {
        m_glGetnTexImage(target, level, format, type, bufSize, pixels);
    } else {
        glGetTexImage(target, level, format, type, pixels);
    }
}

void EglContext::glGetnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params)
{
    if (m_glGetnUniformfv) {
        m_glGetnUniformfv(program, location, bufSize, params);
    } else {
        glGetUniformfv(program, location, params);
    }
}

void EglContext::pushFramebuffer(GLFramebuffer *fbo)
{
    if (fbo != currentFramebuffer()) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo->handle());
        glViewport(0, 0, fbo->size().width(), fbo->size().height());
    }
    m_fbos.push(fbo);
}

GLFramebuffer *EglContext::popFramebuffer()
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

GLFramebuffer *EglContext::currentFramebuffer()
{
    return m_fbos.empty() ? nullptr : m_fbos.top();
}
}
