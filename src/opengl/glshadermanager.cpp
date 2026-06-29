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

static QByteArray listDefines(ShaderTraits traits)
{
    QByteArray ret;
    ret += QByteArrayLiteral("#define TRAIT_MAP_TEXTURE ") + (traits & ShaderTrait::MapTexture ? "1" : "0") + "\n";
    ret += QByteArrayLiteral("#define TRAIT_UNIFORM_COLOR ") + (traits & ShaderTrait::UniformColor ? "1" : "0") + "\n";
    ret += QByteArrayLiteral("#define TRAIT_MODULATE ") + (traits & ShaderTrait::Modulate ? "1" : "0") + "\n";
    ret += QByteArrayLiteral("#define TRAIT_ADJUST_SATURATION ") + (traits & ShaderTrait::AdjustSaturation ? "1" : "0") + "\n";
    ret += QByteArrayLiteral("#define TRAIT_TRANSFORM_COLORSPACE ") + (traits & ShaderTrait::TransformColorspace ? "1" : "0") + "\n";
    ret += QByteArrayLiteral("#define TRAIT_MAP_EXTERNAL_TEXTURE ") + (traits & ShaderTrait::MapExternalTexture ? "1" : "0") + "\n";
    ret += QByteArrayLiteral("#define TRAIT_MAP_MULTI_PLANE_TEXTURE ") + (traits & ShaderTrait::MapMultiPlaneTexture ? "1" : "0") + "\n";
    ret += QByteArrayLiteral("#define TRAIT_ROUNDED_CORNERS ") + (traits & ShaderTrait::RoundedCorners ? "1" : "0") + "\n";
    ret += QByteArrayLiteral("#define TRAIT_BORDER ") + (traits & ShaderTrait::Border ? "1" : "0") + "\n";
    ret += QByteArrayLiteral("#define TRAIT_YUV_CONVERSION ") + (traits & ShaderTrait::YuvConversion ? "1" : "0") + "\n";
    return ret;
}

QByteArray ShaderManager::generateVertexSource(ShaderTraits traits) const
{
    QFile file(":/opengl/base.vert");
    if (!file.open(QIODevice::ReadOnly)) {
        qCCritical(KWIN_OPENGL) << "Failed to read base shader";
        return QByteArray();
    }
    return file.readAll();
}

QByteArray ShaderManager::generateFragmentSource(ShaderTraits traits) const
{
    QFile file(":/opengl/base.frag");
    if (!file.open(QIODevice::ReadOnly)) {
        qCCritical(KWIN_OPENGL) << "Failed to read base shader";
        return QByteArray();
    }
    return file.readAll();
}

std::unique_ptr<GLShader> ShaderManager::generateShader(ShaderTraits traits)
{
    return generateCustomShader(traits);
}

std::unique_ptr<GLShader> ShaderManager::generateCustomShader(ShaderTraits traits, const QByteArray &vertexSource, const QByteArray &fragmentSource)
{
    const auto defines = listDefines(traits);
    const auto vertex = defines + (vertexSource.isEmpty() ? generateVertexSource(traits) : vertexSource);
    const auto fragment = defines + (fragmentSource.isEmpty() ? generateFragmentSource(traits) : fragmentSource);

    auto shader = std::make_unique<GLShader>();
    if (!shader->load(vertex, fragment)) {
        return nullptr;
    }

    shader->bindAttributeLocation("position", VA_Position);
    shader->bindAttributeLocation("texcoord", VA_TexCoord);

    if (!shader->link()) {
        return nullptr;
    }

    return shader;
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
        vertexSource = loadShaderFile(vertexFile);
        if (vertexSource.isEmpty()) {
            return nullptr;
        }
    }
    if (!fragmentFile.isEmpty()) {
        fragmentSource = loadShaderFile(fragmentFile);
        if (fragmentSource.isEmpty()) {
            return nullptr;
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

void ShaderManager::bindAttributeLocations(GLShader *shader) const
{
    shader->bindAttributeLocation("vertex", VA_Position);
    shader->bindAttributeLocation("texCoord", VA_TexCoord);
}

std::unique_ptr<GLShader> ShaderManager::loadShaderFromCode(const QByteArray &vertexSource, const QByteArray &fragmentSource)
{
    auto shader = std::make_unique<GLShader>();
    if (!shader->load(vertexSource, fragmentSource)) {
        return nullptr;
    }

    bindAttributeLocations(shader.get());

    if (!shader->link()) {
        return nullptr;
    }

    return shader;
}

}
