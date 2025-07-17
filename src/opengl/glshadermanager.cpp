/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "glshadermanager.h"
#include "eglcontext.h"
#include "glplatform.h"
#include "glshader.h"
#include "glvertexbuffer.h"
#include "utils/common.h"

#include <QFile>
#include <QTextStream>

namespace KWin
{

ShaderManager *ShaderManager::instance()
{
    return EglContext::currentContext()->shaderManager();
}

ShaderManager::ShaderManager()
{
}

ShaderManager::~ShaderManager()
{
    while (!m_boundShaders.isEmpty()) {
        popShader();
    }
}

QByteArray ShaderManager::generateVertexSource(ShaderTraits traits) const
{
    QByteArray source;
    QTextStream stream(&source);

    const auto context = EglContext::currentContext();
    QByteArray attribute, varying;

    if (!context->isOpenGLES()) {
        const bool glsl_140 = context->glslVersion() >= Version(1, 40);

        attribute = glsl_140 ? QByteArrayLiteral("in") : QByteArrayLiteral("attribute");
        varying = glsl_140 ? QByteArrayLiteral("out") : QByteArrayLiteral("varying");

        if (glsl_140) {
            stream << "#version 140\n\n";
        }
    } else {
        const bool glsl_es_300 = context->glslVersion() >= Version(3, 0);

        attribute = glsl_es_300 ? QByteArrayLiteral("in") : QByteArrayLiteral("attribute");
        varying = glsl_es_300 ? QByteArrayLiteral("out") : QByteArrayLiteral("varying");

        if (glsl_es_300) {
            stream << "#version 300 es\n\n";
        }
    }

    stream << attribute << " vec4 position;\n";
    if (traits & (ShaderTrait::MapTexture | ShaderTrait::MapExternalTexture | ShaderTrait::MapYUVTexture)) {
        stream << attribute << " vec4 texcoord;\n\n";
        stream << varying << " vec2 texcoord0;\n\n";
    } else {
        stream << "\n";
    }

    if (traits & ShaderTrait::RoundedCorners) {
        stream << varying << " vec2 position0;\n\n";
    }

    stream << "uniform mat4 modelViewProjectionMatrix;\n\n";

    stream << "void main()\n{\n";
    if (traits & (ShaderTrait::MapTexture | ShaderTrait::MapExternalTexture | ShaderTrait::MapYUVTexture)) {
        stream << "    texcoord0 = texcoord.st;\n";
    }

    if (traits & ShaderTrait::RoundedCorners) {
        stream << "    position0 = position.xy;\n";
    }

    stream << "    gl_Position = modelViewProjectionMatrix * position;\n";
    stream << "}\n";

    stream.flush();
    return source;
}

QByteArray ShaderManager::generateFragmentSource(ShaderTraits traits) const
{
    QByteArray source;
    QTextStream stream(&source);

    const auto context = EglContext::currentContext();
    QByteArray varying, output, textureLookup;

    if (!context->isOpenGLES()) {
        const bool glsl_140 = context->glslVersion() >= Version(1, 40);

        if (glsl_140) {
            stream << "#version 140\n\n";
        }

        varying = glsl_140 ? QByteArrayLiteral("in") : QByteArrayLiteral("varying");
        textureLookup = glsl_140 ? QByteArrayLiteral("texture") : QByteArrayLiteral("texture2D");
        output = glsl_140 ? QByteArrayLiteral("fragColor") : QByteArrayLiteral("gl_FragColor");
    } else {
        const bool glsl_es_300 = context->glslVersion() >= Version(3, 0);

        if (glsl_es_300) {
            stream << "#version 300 es\n\n";
        } else {
            if (traits & ShaderTrait::RoundedCorners) {
                stream << "#extension GL_OES_standard_derivatives : enable\n\n";
            }
        }

        // From the GLSL ES specification:
        //
        //     "The fragment language has no default precision qualifier for floating point types."
        stream << "precision highp float;\n\n";

        varying = glsl_es_300 ? QByteArrayLiteral("in") : QByteArrayLiteral("varying");
        textureLookup = glsl_es_300 ? QByteArrayLiteral("texture") : QByteArrayLiteral("texture2D");
        output = glsl_es_300 ? QByteArrayLiteral("fragColor") : QByteArrayLiteral("gl_FragColor");
    }

    if (traits & ShaderTrait::MapTexture) {
        stream << "uniform sampler2D sampler;\n";
        stream << varying << " vec2 texcoord0;\n";
    } else if (traits & ShaderTrait::MapYUVTexture) {
        stream << "uniform sampler2D sampler;\n";
        stream << "uniform sampler2D sampler1;\n";
        stream << "uniform mat4 yuvToRgb;\n";
        stream << varying << " vec2 texcoord0;\n";
    } else if (traits & ShaderTrait::MapExternalTexture) {
        stream << "#extension GL_OES_EGL_image_external : require\n\n";
        stream << "uniform samplerExternalOES sampler;\n";
        stream << varying << " vec2 texcoord0;\n";
    } else if (traits & ShaderTrait::UniformColor) {
        stream << "uniform vec4 geometryColor;\n";
    }

    if (traits & ShaderTrait::Modulate) {
        stream << "uniform vec4 modulation;\n";
    }
    if (traits & ShaderTrait::AdjustSaturation) {
        stream << "#include \"saturation.glsl\"\n";
    }
    if (traits & ShaderTrait::TransformColorspace) {
        stream << "#include \"colormanagement.glsl\"\n";
    }
    if (traits & ShaderTrait::RoundedCorners) {
        stream << "#include \"sdf.glsl\"\n";

        stream << "uniform vec4 box;\n";
        stream << "uniform vec4 cornerRadius;\n";
        stream << varying << " vec2 position0;\n";
    }

    if (output != QByteArrayLiteral("gl_FragColor")) {
        stream << "\nout vec4 " << output << ";\n";
    }

    stream << "\nvoid main(void)\n{\n";
    stream << "    vec4 result;\n";
    if (traits & ShaderTrait::MapTexture) {
        stream << "    result = " << textureLookup << "(sampler, texcoord0);\n";
    } else if (traits & ShaderTrait::MapYUVTexture) {
        stream << "    result = yuvToRgb * vec4(" << textureLookup << "(sampler, texcoord0).x, " << textureLookup << "(sampler1, texcoord0).rg, 1.0);\n";
        stream << "    result.a = 1.0;\n";
    } else if (traits & ShaderTrait::MapExternalTexture) {
        // external textures require texture2D for sampling
        stream << "    result = texture2D(sampler, texcoord0);\n";
    } else if (traits & ShaderTrait::UniformColor) {
        stream << "    result = geometryColor;\n";
    }

    if (traits & ShaderTrait::RoundedCorners) {
        stream << "    float f = sdfRoundedBox(position0, box.xy, box.zw, cornerRadius);\n";
        stream << "    float df = fwidth(f);\n";
        stream << "    result *= 1.0 - clamp(0.5 + f / df, 0.0, 1.0);\n";
    }
    if (traits & ShaderTrait::TransformColorspace) {
        stream << "    result = sourceEncodingToNitsInDestinationColorspace(result);\n";
    }
    if (traits & ShaderTrait::AdjustSaturation) {
        stream << "    result = adjustSaturation(result);\n";
    }
    if (traits & ShaderTrait::Modulate) {
        stream << "    result *= modulation;\n";
    }
    if (traits & ShaderTrait::TransformColorspace) {
        stream << "    result = nitsToDestinationEncoding(result);\n";
    }

    stream << "    " << output << " = result;\n";
    stream << "}";
    stream.flush();
    return source;
}

std::unique_ptr<GLShader> ShaderManager::generateShader(ShaderTraits traits)
{
    return generateCustomShader(traits);
}

std::optional<QByteArray> ShaderManager::preprocess(const QByteArray &src, int recursionDepth) const
{
    recursionDepth++;
    if (recursionDepth > 10) {
        qCWarning(KWIN_OPENGL, "shader has too many recursive includes!");
        return std::nullopt;
    }
    QByteArray ret;
    ret.reserve(src.size());
    const auto split = src.split('\n');
    for (auto it = split.begin(); it != split.end(); it++) {
        const auto &line = *it;
        if (line.startsWith("#include \"") && line.endsWith("\"")) {
            static constexpr ssize_t includeLength = QByteArrayView("#include \"").size();
            const QByteArray path = ":/opengl/" + line.mid(includeLength, line.size() - includeLength - 1);
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                qCWarning(KWIN_OPENGL, "failed to read include line %s", qPrintable(line));
                return std::nullopt;
            }
            const auto processed = preprocess(file.readAll(), recursionDepth);
            if (!processed) {
                return std::nullopt;
            }
            ret.append(*processed);
        } else {
            ret.append(line);
            ret.append('\n');
        }
    }
    return ret;
}

std::unique_ptr<GLShader> ShaderManager::generateCustomShader(ShaderTraits traits, const QByteArray &vertexSource, const QByteArray &fragmentSource)
{
    const auto vertex = preprocess(vertexSource.isEmpty() ? generateVertexSource(traits) : vertexSource);
    const auto fragment = preprocess(fragmentSource.isEmpty() ? generateFragmentSource(traits) : fragmentSource);
    if (!vertex || !fragment) {
        return nullptr;
    }

    std::unique_ptr<GLShader> shader{new GLShader(GLShader::ExplicitLinking)};
    shader->load(*vertex, *fragment);

    shader->bindAttributeLocation("position", VA_Position);
    shader->bindAttributeLocation("texcoord", VA_TexCoord);
    shader->bindFragDataLocation("fragColor", 0);

    shader->link();
    return shader;
}

static QString resolveShaderFilePath(const QString &filePath)
{
    QString suffix;
    QString extension;

    const auto context = EglContext::currentContext();
    const Version coreVersionNumber = context->isOpenGLES() ? Version(3, 0) : Version(1, 40);
    if (context->glslVersion() >= coreVersionNumber) {
        suffix = QStringLiteral("_core");
    }

    if (filePath.endsWith(QStringLiteral(".frag"))) {
        extension = QStringLiteral(".frag");
    } else if (filePath.endsWith(QStringLiteral(".vert"))) {
        extension = QStringLiteral(".vert");
    } else {
        qCWarning(KWIN_OPENGL) << filePath << "must end either with .vert or .frag";
        return QString();
    }

    const QString prefix = filePath.chopped(extension.size());
    return prefix + suffix + extension;
}

std::unique_ptr<GLShader> ShaderManager::generateShaderFromFile(ShaderTraits traits, const QString &vertexFile, const QString &fragmentFile)
{
    auto loadShaderFile = [](const QString &filePath) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            return file.readAll();
        }
        qCCritical(KWIN_OPENGL) << "Failed to read shader " << filePath;
        return QByteArray();
    };
    QByteArray vertexSource;
    QByteArray fragmentSource;
    if (!vertexFile.isEmpty()) {
        vertexSource = loadShaderFile(resolveShaderFilePath(vertexFile));
        if (vertexSource.isEmpty()) {
            return std::unique_ptr<GLShader>(new GLShader());
        }
    }
    if (!fragmentFile.isEmpty()) {
        fragmentSource = loadShaderFile(resolveShaderFilePath(fragmentFile));
        if (fragmentSource.isEmpty()) {
            return std::unique_ptr<GLShader>(new GLShader());
        }
    }
    return generateCustomShader(traits, vertexSource, fragmentSource);
}

GLShader *ShaderManager::shader(ShaderTraits traits)
{
    std::unique_ptr<GLShader> &shader = m_shaderHash[traits];
    if (!shader) {
        shader = generateShader(traits);
    }
    return shader.get();
}

GLShader *ShaderManager::getBoundShader() const
{
    if (m_boundShaders.isEmpty()) {
        return nullptr;
    } else {
        return m_boundShaders.top();
    }
}

bool ShaderManager::isShaderBound() const
{
    return !m_boundShaders.isEmpty();
}

GLShader *ShaderManager::pushShader(ShaderTraits traits)
{
    GLShader *shader = this->shader(traits);
    pushShader(shader);
    return shader;
}

void ShaderManager::pushShader(GLShader *shader)
{
    // only bind shader if it is not already bound
    if (shader != getBoundShader()) {
        shader->bind();
    }
    m_boundShaders.push(shader);
}

void ShaderManager::popShader()
{
    if (m_boundShaders.isEmpty()) {
        return;
    }
    GLShader *shader = m_boundShaders.pop();
    if (m_boundShaders.isEmpty()) {
        // no more shader bound - unbind
        shader->unbind();
    } else if (shader != m_boundShaders.top()) {
        // only rebind if a different shader is on top of stack
        m_boundShaders.top()->bind();
    }
}

void ShaderManager::bindFragDataLocations(GLShader *shader)
{
    shader->bindFragDataLocation("fragColor", 0);
}

void ShaderManager::bindAttributeLocations(GLShader *shader) const
{
    shader->bindAttributeLocation("vertex", VA_Position);
    shader->bindAttributeLocation("texCoord", VA_TexCoord);
}

std::unique_ptr<GLShader> ShaderManager::loadShaderFromCode(const QByteArray &vertexSource, const QByteArray &fragmentSource)
{
    std::unique_ptr<GLShader> shader{new GLShader(GLShader::ExplicitLinking)};
    shader->load(vertexSource, fragmentSource);
    bindAttributeLocations(shader.get());
    bindFragDataLocations(shader.get());
    shader->link();
    return shader;
}

}
