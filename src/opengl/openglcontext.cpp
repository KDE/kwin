/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "openglcontext.h"

#include <QByteArray>
#include <QList>

#include <epoxy/gl.h>

namespace KWin
{

OpenGlContext *OpenGlContext::s_currentContext = nullptr;

static QSet<QByteArray> getExtensions(OpenGlContext *context)
{
    QSet<QByteArray> ret;
    if (!context->isOpenglES() && context->hasVersion(Version(3, 0))) {
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
    if (context->isOpenglES()) {
        return context->hasVersion(Version(3, 0));
    } else {
        return context->hasVersion(Version(3, 3)) || context->hasOpenglExtension(QByteArrayLiteral("GL_ARB_texture_swizzle"));
    }
}

static bool checkTextureStorageSupport(OpenGlContext *context)
{
    if (context->isOpenglES()) {
        return context->hasVersion(Version(3, 0)) || context->hasOpenglExtension(QByteArrayLiteral("GL_EXT_texture_storage"));
    } else {
        return context->hasVersion(Version(4, 2)) || context->hasOpenglExtension(QByteArrayLiteral("GL_ARB_texture_storage"));
    }
}

OpenGlContext::OpenGlContext()
    : m_versionString((const char *)glGetString(GL_VERSION))
    , m_version(Version::parseString(m_versionString))
    , m_vendor((const char *)glGetString(GL_VENDOR))
    , m_renderer((const char *)glGetString(GL_RENDERER))
    , m_isOpenglES(m_versionString.startsWith("OpenGL ES"))
    , m_extensions(getExtensions(this))
    , m_supportsTimerQueries(checkTimerQuerySupport())
    , m_supportsTextureStorage(checkTextureStorageSupport(this))
    , m_supportsTextureSwizzle(checkTextureSwizzleSupport(this))
    , m_supportsARGB32Textures(!m_isOpenglES || hasOpenglExtension(QByteArrayLiteral("GL_EXT_texture_format_BGRA8888")))
    , m_supportsTextureUnpack(!m_isOpenglES || hasOpenglExtension(QByteArrayLiteral("GL_EXT_unpack_subimage")))
    , m_supportsRGTextures(hasVersion(Version(3, 0)) || hasOpenglExtension(QByteArrayLiteral("GL_ARB_texture_rg")) || hasOpenglExtension(QByteArrayLiteral("GL_EXT_texture_rg")))
    , m_supports16BitTextures(!m_isOpenglES || hasOpenglExtension(QByteArrayLiteral("GL_EXT_texture_norm16")))
{
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

QByteArrayView OpenGlContext::vendor() const
{
    return m_vendor;
}

QByteArrayView OpenGlContext::renderer() const
{
    return m_renderer;
}

bool OpenGlContext::isOpenglES() const
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

bool OpenGlContext::supportsTextureUnpack() const
{
    return m_supportsTextureUnpack;
}

bool OpenGlContext::supportsRGTextures() const
{
    return m_supportsRGTextures;
}

bool OpenGlContext::supports16BitTextures() const
{
    return m_supports16BitTextures;
}

ShaderManager *OpenGlContext::shaderManager() const
{
    return m_shaderManager;
}

bool OpenGlContext::checkSupported() const
{
    const bool supportsGLSL = m_isOpenglES || (hasOpenglExtension("GL_ARB_shader_objects") && hasOpenglExtension("GL_ARB_fragment_shader") && hasOpenglExtension("GL_ARB_vertex_shader"));
    const bool supportsNonPowerOfTwoTextures = m_isOpenglES || hasOpenglExtension("GL_ARB_texture_non_power_of_two");
    const bool supports3DTextures = !m_isOpenglES || hasVersion(Version(3, 0)) || hasOpenglExtension("GL_OES_texture_3D");
    const bool supportsFBOs = m_isOpenglES || hasVersion(Version(3, 0)) || hasOpenglExtension("GL_ARB_framebuffer_object") || hasOpenglExtension(QByteArrayLiteral("GL_EXT_framebuffer_object"));
    return supportsGLSL && supportsNonPowerOfTwoTextures && supports3DTextures && supportsFBOs;
}

void OpenGlContext::setShaderManager(ShaderManager *manager)
{
    m_shaderManager = manager;
}

OpenGlContext *OpenGlContext::currentContext()
{
    return s_currentContext;
}
}
