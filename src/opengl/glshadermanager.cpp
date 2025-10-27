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

static QByteArray listDefines(ShaderTraits traits, const ColorPipeline &colorPipeline)
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
    ret += QByteArrayLiteral("#define TRAIT_COLORPIPELINE ") + (colorPipeline.isIdentity() ? "0" : "1") + "\n";
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

QByteArray ShaderManager::generateFragmentSource(ShaderTraits traits, const ColorPipeline &pipeline) const
{
    QFile file(":/opengl/base.frag");
    if (!file.open(QIODevice::ReadOnly)) {
        qCCritical(KWIN_OPENGL) << "Failed to read base shader";
        return QByteArray();
    }
    return generateColorPipelineShader(pipeline) + file.readAll();
}

std::unique_ptr<GLShader> ShaderManager::generateShader(ShaderTraits traits)
{
    return generateCustomShader(traits);
}

std::unique_ptr<GLShader> ShaderManager::generateCustomShader(ShaderTraits traits, const QByteArray &vertexSource, const QByteArray &fragmentSource, const ColorPipeline &colorPipeline)
{
    const auto defines = listDefines(traits, colorPipeline);
    const auto vertex = defines + (vertexSource.isEmpty() ? generateVertexSource(traits) : vertexSource);
    const auto fragment = defines + (fragmentSource.isEmpty() ? generateFragmentSource(traits, colorPipeline) : fragmentSource);

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
    if (!shader) {
        return nullptr;
    }
    pushShader(shader);
    return shader;
}

void ShaderManager::pushShader(GLShader *shader)
{
    Q_ASSERT(shader);
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

GLShader *ShaderManager::pushShader(ShaderTraits traits, const ColorPipeline &pipeline)
{
    if (pipeline.isIdentity()) {
        return pushShader(traits);
    }
    const size_t hash = generateColorPipelineHash(traits, pipeline);
    auto &shader = m_colorPipelineShaders[hash];
    if (!shader) {
        shader = generateCustomShader(traits, {}, {}, pipeline);
    }
    shader->bind();
    return shader.get();
}

size_t ShaderManager::generateColorPipelineHash(ShaderTraits traits, const ColorPipeline &pipeline)
{
    size_t ret = qHash(traits);
    for (const auto &op : pipeline.ops) {
        ret = qHash(op.operation.index(), ret);
    }
    return ret;
}

QByteArray ShaderManager::generateColorPipelineShader(const ColorPipeline &pipeline)
{
    if (pipeline.isIdentity()) {
        return {};
    }
    QByteArray ret = QByteArrayLiteral(R"(
#include "color_helpers.glsl"

vec4 applyColorPipeline(vec4 color)
{
    vec4 ret = color;
)");
    uint32_t matrixCount = 0;
    uint32_t multiplierCount = 0;
    uint32_t transferCount = 0;
    uint32_t inverseTransferCount = 0;
    uint32_t toneMapCount = 0;
    uint32_t lut1DCount = 0;
    uint32_t lut3DCount = 0;
    uint32_t clampCount = 0;

    // TODO combine consecutive TFs and luts and tonemapper in here?

    for (const auto &op : pipeline.ops) {
        const auto &operation = op.operation;
        if (std::holds_alternative<ColorMatrix>(operation)) {
            ret += std::format("    ret.rgb = (colorPipelineMatrix{} * vec4(ret.rgb, 1.0)).rgb;\n", matrixCount);
            matrixCount++;
        } else if (std::holds_alternative<ColorMultiplier>(operation)) {
            ret += std::format("    ret.rgb *= colorPipelineMultiplier{};\n", multiplierCount);
            multiplierCount++;
        } else if (std::holds_alternative<ColorTransferFunction>(operation)) {
            ret += std::format("    ret = encodingToNits(ret, colorPipelineTF{}_type, colorPipelineTF{}_params.x, colorPipelineTF{}_params.y);\n",
                               transferCount, transferCount, transferCount);
            transferCount++;
        } else if (std::holds_alternative<InverseColorTransferFunction>(operation)) {
            ret += std::format("    ret = nitsToEncoding(ret, colorPipelineInvTF{}_type, colorPipelineInvTF{}_params.x, colorPipelineInvTF{}_params.y);\n",
                               inverseTransferCount, inverseTransferCount, inverseTransferCount);
            inverseTransferCount++;
        } else if (std::holds_alternative<ColorTonemapper>(operation)) {
            ret += std::format("    ret.r = applyTonemapper(ret.r, colorPipelineTonemapper{}_reference, colorPipelineTonemapper{}_v);\n", toneMapCount, toneMapCount);
            toneMapCount++;
        } else if (std::holds_alternative<std::shared_ptr<ColorTransformation>>(operation)) {
            ret += std::format("    ret.rgb = sample1DLut(ret.rgb, colorPipeline1DLut{}_sampler, colorPipeline1DLut{}_size);\n", lut1DCount, lut1DCount);
            lut1DCount++;
        } else if (std::holds_alternative<std::shared_ptr<ColorLUT3D>>(operation)) {
            ret += std::format("    ret.rgb = sample3DLut(ret.rgb, colorPipeline3DLut{}_sampler, colorPipeline3DLut{}_size);\n", lut3DCount, lut3DCount);
            lut3DCount++;
        } else if (std::holds_alternative<ColorClamp>(operation)) {
            ret += std::format("    ret.rgb = clamp(ret.rgb, vec3(colorPipelineClamp{}.x), vec3(colorPipelineClamp{}.y));\n", clampCount, clampCount);
            clampCount++;
        } else {
            Q_UNREACHABLE();
        }
    }
    ret += "    return ret;\n}\n";

    QByteArray uniforms;
    for (uint32_t i = 0; i < matrixCount; i++) {
        uniforms += std::format("uniform mat4 colorPipelineMatrix{};\n", i);
    }
    for (uint32_t i = 0; i < multiplierCount; i++) {
        uniforms += std::format("uniform vec3 colorPipelineMultiplier{};\n", i);
    }
    for (uint32_t i = 0; i < transferCount; i++) {
        uniforms += std::format("uniform int colorPipelineTF{}_type;\n", i);
        uniforms += std::format("uniform vec2 colorPipelineTF{}_params;\n", i);
    }
    for (uint32_t i = 0; i < inverseTransferCount; i++) {
        uniforms += std::format("uniform int colorPipelineInvTF{}_type;\n", i);
        uniforms += std::format("uniform vec2 colorPipelineInvTF{}_params;\n", i);
    }
    for (uint32_t i = 0; i < toneMapCount; i++) {
        uniforms += std::format("uniform float colorPipelineTonemapper{}_reference;\n", i);
        uniforms += std::format("uniform float colorPipelineTonemapper{}_v;\n", i);
    }
    for (uint32_t i = 0; i < lut1DCount; i++) {
        uniforms += std::format("uniform sampler2D colorPipeline1DLut{}_sampler;\n", i);
        uniforms += std::format("uniform int colorPipeline1DLut{}_size;\n", i);
    }
    for (uint32_t i = 0; i < lut3DCount; i++) {
        uniforms += std::format("uniform sampler3D colorPipeline3DLut{}_sampler;\n", i);
        uniforms += std::format("uniform int colorPipeline3DLut{}_size;\n", i);
    }
    for (uint32_t i = 0; i < clampCount; i++) {
        uniforms += std::format("uniform vec2 colorPipelineClamp{};\n", i);
    }
    return uniforms + ret;
}

}
