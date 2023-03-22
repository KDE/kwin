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

Version::Version(uint32_t major, uint32_t minor, uint32_t patch)
    : m_major(major)
    , m_minor(minor)
    , m_patch(patch)
{
}

int Version::operator<=>(const Version &other) const
{
    if (m_major != other.m_major) {
        return m_major - other.m_major;
    }
    if (m_minor != other.m_minor) {
        return m_minor - other.m_minor;
    }
    return m_patch - other.m_patch;
}

bool Version::operator==(const Version &other) const
{
    return other.major() == major() && other.minor() == minor() && other.patch() == patch();
}

bool Version::isValid() const
{
    return m_major > 0 || m_minor > 0 || m_patch > 0;
}

uint32_t Version::major() const
{
    return m_major;
}

uint32_t Version::minor() const
{
    return m_minor;
}

uint32_t Version::patch() const
{
    return m_patch;
}

Version Version::parseString(QByteArrayView versionString)
{
    // Skip any leading non digit
    int start = 0;
    while (start < versionString.length() && !QChar::fromLatin1(versionString[start]).isDigit()) {
        start++;
    }

    // Strip any non digit, non '.' characters from the end
    int end = start;
    while (end < versionString.length() && (versionString[end] == '.' || QChar::fromLatin1(versionString[end]).isDigit())) {
        end++;
    }

    const QByteArray result = versionString.toByteArray().mid(start, end - start);
    const QList<QByteArray> tokens = result.split('.');
    if (tokens.empty()) {
        return Version(0, 0, 0);
    }
    const uint64_t major = tokens.at(0).toInt();
    const uint64_t minor = tokens.count() > 1 ? tokens.at(1).toInt() : 0;
    const uint64_t patch = tokens.count() > 2 ? tokens.at(2).toInt() : 0;

    return Version(major, minor, patch);
}

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

OpenGlContext::OpenGlContext()
    : m_versionString((const char *)glGetString(GL_VERSION))
    , m_version(Version::parseString(m_versionString))
    , m_vendor((const char *)glGetString(GL_VENDOR))
    , m_renderer((const char *)glGetString(GL_RENDERER))
    , m_isOpenglES(m_versionString.startsWith("OpenGL ES"))
    , m_extensions(getExtensions(this))
{
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
}
