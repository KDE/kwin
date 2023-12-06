/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "glshadermanager.h"
#include "glplatform.h"
#include "glshader.h"
#include "glvertexbuffer.h"
#include "utils/common.h"

#include <QFile>
#include <QTextStream>

namespace KWin
{

std::unique_ptr<ShaderManager> ShaderManager::s_shaderManager;

ShaderManager *ShaderManager::instance()
{
    if (!s_shaderManager) {
        s_shaderManager = std::make_unique<ShaderManager>();
    }
    return s_shaderManager.get();
}

void ShaderManager::cleanup()
{
    s_shaderManager.reset();
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

    GLPlatform *const gl = GLPlatform::instance();
    QByteArray attribute, varying;

    if (!gl->isGLES()) {
        const bool glsl_140 = gl->glslVersion() >= Version(1, 40);

        attribute = glsl_140 ? QByteArrayLiteral("in") : QByteArrayLiteral("attribute");
        varying = glsl_140 ? QByteArrayLiteral("out") : QByteArrayLiteral("varying");

        if (glsl_140) {
            stream << "#version 140\n\n";
        }
    } else {
        const bool glsl_es_300 = gl->glslVersion() >= Version(3, 0);

        attribute = glsl_es_300 ? QByteArrayLiteral("in") : QByteArrayLiteral("attribute");
        varying = glsl_es_300 ? QByteArrayLiteral("out") : QByteArrayLiteral("varying");

        if (glsl_es_300) {
            stream << "#version 300 es\n\n";
        }
    }

    stream << attribute << " vec4 position;\n";
    if (traits & (ShaderTrait::MapTexture | ShaderTrait::MapExternalTexture)) {
        stream << attribute << " vec4 texcoord;\n\n";
        stream << varying << " vec2 texcoord0;\n\n";
    } else {
        stream << "\n";
    }

    stream << "uniform mat4 modelViewProjectionMatrix;\n\n";

    stream << "void main()\n{\n";
    if (traits & (ShaderTrait::MapTexture | ShaderTrait::MapExternalTexture)) {
        stream << "    texcoord0 = texcoord.st;\n";
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

    GLPlatform *const gl = GLPlatform::instance();
    QByteArray varying, output, textureLookup;

    if (!gl->isGLES()) {
        const bool glsl_140 = gl->glslVersion() >= Version(1, 40);

        if (glsl_140) {
            stream << "#version 140\n\n";
        }

        varying = glsl_140 ? QByteArrayLiteral("in") : QByteArrayLiteral("varying");
        textureLookup = glsl_140 ? QByteArrayLiteral("texture") : QByteArrayLiteral("texture2D");
        output = glsl_140 ? QByteArrayLiteral("fragColor") : QByteArrayLiteral("gl_FragColor");
    } else {
        const bool glsl_es_300 = GLPlatform::instance()->glslVersion() >= Version(3, 0);

        if (glsl_es_300) {
            stream << "#version 300 es\n\n";
        }

        // From the GLSL ES specification:
        //
        //     "The fragment language has no default precision qualifier for floating point types."
        stream << "precision highp float;\n\n";

        varying = glsl_es_300 ? QByteArrayLiteral("in") : QByteArrayLiteral("varying");
        textureLookup = glsl_es_300 ? QByteArrayLiteral("texture") : QByteArrayLiteral("texture2D");
        output = glsl_es_300 ? QByteArrayLiteral("fragColor") : QByteArrayLiteral("gl_FragColor");
    }
    if (!gl->isGLES() || gl->glslVersion() >= Version(3, 0)) {
        stream << "\n";
        stream << "vec3 doMix(vec3 left, vec3 right, bvec3 rightFactor) {\n";
        stream << "    return mix(left, right, rightFactor);\n";
        stream << "}\n";
    } else {
        // GLES 2 doesn't natively support mix with bvec3
        stream << "\n";
        stream << "vec3 doMix(vec3 left, vec3 right, bvec3 rightFactor) {\n";
        stream << "    return mix(left, right, vec3(rightFactor.r ? 1.0 : 0.0, rightFactor.g ? 1.0 : 0.0, rightFactor.b ? 1.0 : 0.0));\n";
        stream << "}\n";
    }

    if (traits & ShaderTrait::MapTexture) {
        stream << "uniform sampler2D sampler;\n";
        stream << "uniform sampler2D sampler1;\n";
        stream << "uniform int converter;\n";
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
        stream << "uniform float saturation;\n";
        stream << "uniform vec3 primaryBrightness;\n";
    }
    if (traits & ShaderTrait::TransformColorspace) {
        stream << "const int sRGB_EOTF = 0;\n";
        stream << "const int linear_EOTF = 1;\n";
        stream << "const int PQ_EOTF = 2;\n";
        stream << "const int scRGB_EOTF = 3;\n";
        stream << "const int gamma22_EOTF = 4;\n";
        stream << "\n";
        stream << "uniform mat3 colorimetryTransform;\n";
        stream << "uniform int sourceNamedTransferFunction;\n";
        stream << "uniform int destinationNamedTransferFunction;\n";
        stream << "uniform float sdrBrightness;// in nits\n";
        stream << "uniform float maxHdrBrightness; // in nits\n";
        stream << "\n";
        stream << "vec3 nitsToPq(vec3 nits) {\n";
        stream << "    vec3 normalized = clamp(nits / 10000.0, vec3(0), vec3(1));\n";
        stream << "    const float c1 = 0.8359375;\n";
        stream << "    const float c2 = 18.8515625;\n";
        stream << "    const float c3 = 18.6875;\n";
        stream << "    const float m1 = 0.1593017578125;\n";
        stream << "    const float m2 = 78.84375;\n";
        stream << "    vec3 powed = pow(normalized, vec3(m1));\n";
        stream << "    vec3 num = vec3(c1) + c2 * powed;\n";
        stream << "    vec3 denum = vec3(1.0) + c3 * powed;\n";
        stream << "    return pow(num / denum, vec3(m2));\n";
        stream << "}\n";
        stream << "vec3 pqToNits(vec3 pq) {\n";
        stream << "    const float c1 = 0.8359375;\n";
        stream << "    const float c2 = 18.8515625;\n";
        stream << "    const float c3 = 18.6875;\n";
        stream << "    const float m1_inv = 1.0 / 0.1593017578125;\n";
        stream << "    const float m2_inv = 1.0 / 78.84375;\n";
        stream << "    vec3 powed = pow(pq, vec3(m2_inv));\n";
        stream << "    vec3 num = max(powed - c1, vec3(0.0));\n";
        stream << "    vec3 den = c2 - c3 * powed;\n";
        stream << "    return 10000.0 * pow(num / den, vec3(m1_inv));\n";
        stream << "}\n";
        stream << "vec3 srgbToLinear(vec3 color) {\n";
        stream << "    bvec3 isLow = lessThanEqual(color, vec3(0.04045f));\n";
        stream << "    vec3 loPart = color / 12.92f;\n";
        stream << "    vec3 hiPart = pow((color + 0.055f) / 1.055f, vec3(12.0f / 5.0f));\n";
        stream << "    return doMix(hiPart, loPart, isLow);\n";
        stream << "}\n";
        stream << "\n";
        stream << "vec3 linearToSrgb(vec3 color) {\n";
        stream << "    bvec3 isLow = lessThanEqual(color, vec3(0.0031308f));\n";
        stream << "    vec3 loPart = color * 12.92f;\n";
        stream << "    vec3 hiPart = pow(color, vec3(5.0f / 12.0f)) * 1.055f - 0.055f;\n";
        stream << "    return doMix(hiPart, loPart, isLow);\n";
        stream << "}\n";
        stream << "\n";
        stream << "vec3 doTonemapping(vec3 color, float maxBrightness) {\n";
        stream << "    // colorimetric 'tonemapping': just clip to the output color space\n";
        stream << "    return clamp(color, vec3(0.0), vec3(maxBrightness));\n";
        stream << "}\n";
        stream << "\n";
    }

    if (output != QByteArrayLiteral("gl_FragColor")) {
        stream << "\nout vec4 " << output << ";\n";
    }

    if (traits & ShaderTrait::MapTexture) {
        // limited range BT601 in -> full range BT709 out
        stream << "vec4 transformY_UV(sampler2D tex0, sampler2D tex1, vec2 texcoord0) {\n";
        stream << "    float y = 1.16438356 * (texture2D(tex0, texcoord0).x - 0.0625);\n";
        stream << "    float u = texture2D(tex1, texcoord0).r - 0.5;\n";
        stream << "    float v = texture2D(tex1, texcoord0).g - 0.5;\n";
        stream << "    return vec4(y + 1.59602678 * v"
                  "              , y - 0.39176229 * u - 0.81296764 * v"
                  "              , y + 2.01723214 * u"
                  "              , 1);\n";
        stream << "}\n";
        stream << "\n";
    }

    stream << "\nvoid main(void)\n{\n";
    stream << "    vec4 result;\n";
    if (traits & ShaderTrait::MapTexture) {
        stream << "    if (converter == 0) {\n";
        stream << "        result = " << textureLookup << "(sampler, texcoord0);\n";
        stream << "    } else {\n";
        stream << "        result = transformY_UV(sampler, sampler1, texcoord0);\n";
        stream << "    }\n";
    } else if (traits & ShaderTrait::MapExternalTexture) {
        // external textures require texture2D for sampling
        stream << "    result = texture2D(sampler, texcoord0);\n";
    } else if (traits & ShaderTrait::UniformColor) {
        stream << "    result = geometryColor;\n";
    }
    if (traits & ShaderTrait::TransformColorspace) {
        stream << "    if (sourceNamedTransferFunction == sRGB_EOTF) {\n";
        stream << "        result.rgb /= max(result.a, 0.001);\n";
        stream << "        result.rgb = sdrBrightness * srgbToLinear(result.rgb);\n";
        stream << "        result.rgb *= result.a;\n";
        stream << "    } else if (sourceNamedTransferFunction == PQ_EOTF) {\n";
        stream << "        result.rgb /= max(result.a, 0.001);\n";
        stream << "        result.rgb = pqToNits(result.rgb);\n";
        stream << "        result.rgb *= result.a;\n";
        stream << "    } else if (sourceNamedTransferFunction == scRGB_EOTF) {\n";
        stream << "        result.rgb *= 80.0;\n";
        stream << "    } else if (sourceNamedTransferFunction == gamma22_EOTF) {\n";
        stream << "        result.rgb /= max(result.a, 0.001);\n";
        stream << "        result.rgb = sdrBrightness * pow(result.rgb, vec3(2.2));\n";
        stream << "        result.rgb *= result.a;\n";
        stream << "    }\n";
        stream << "    result.rgb = doTonemapping(colorimetryTransform * result.rgb, maxHdrBrightness);\n";
    }
    if (traits & ShaderTrait::AdjustSaturation) {
        // this calculates the Y component of the XYZ color representation for the color,
        // which roughly corresponds to the brightness of the RGB tuple
        stream << "    float Y = dot(result.rgb, primaryBrightness);\n";
        stream << "    result.rgb = mix(vec3(Y), result.rgb, saturation);\n";
    }
    if (traits & ShaderTrait::Modulate) {
        stream << "    result *= modulation;\n";
    }
    if (traits & ShaderTrait::TransformColorspace) {
        stream << "    if (destinationNamedTransferFunction == sRGB_EOTF) {\n";
        stream << "        result.rgb /= max(result.a, 0.001);\n";
        stream << "        result.rgb = linearToSrgb(doTonemapping(result.rgb, sdrBrightness) / sdrBrightness);\n";
        stream << "        result.rgb *= result.a;\n";
        stream << "    } else if (destinationNamedTransferFunction == PQ_EOTF) {\n";
        stream << "        result.rgb /= max(result.a, 0.001);\n";
        stream << "        result.rgb = nitsToPq(result.rgb);\n";
        stream << "        result.rgb *= result.a;\n";
        stream << "    } else if (destinationNamedTransferFunction == scRGB_EOTF) {\n";
        stream << "        result.rgb /= 80.0;\n";
        stream << "    } else if (destinationNamedTransferFunction == gamma22_EOTF) {\n";
        stream << "        result.rgb /= max(result.a, 0.001);\n";
        stream << "        result.rgb = pow(result.rgb / sdrBrightness, vec3(1.0 / 2.2));\n";
        stream << "        result.rgb *= result.a;\n";
        stream << "    }\n";
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

std::unique_ptr<GLShader> ShaderManager::generateCustomShader(ShaderTraits traits, const QByteArray &vertexSource, const QByteArray &fragmentSource)
{
    const QByteArray vertex = vertexSource.isEmpty() ? generateVertexSource(traits) : vertexSource;
    const QByteArray fragment = fragmentSource.isEmpty() ? generateFragmentSource(traits) : fragmentSource;

#if 0
    qCDebug(KWIN_OPENGL) << "**************";
    qCDebug(KWIN_OPENGL) << vertex;
    qCDebug(KWIN_OPENGL) << "**************";
    qCDebug(KWIN_OPENGL) << fragment;
    qCDebug(KWIN_OPENGL) << "**************";
#endif

    std::unique_ptr<GLShader> shader{new GLShader(GLShader::ExplicitLinking)};
    shader->load(vertex, fragment);

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

    const Version coreVersionNumber = GLPlatform::instance()->isGLES() ? Version(3, 0) : Version(1, 40);
    if (GLPlatform::instance()->glslVersion() >= coreVersionNumber) {
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
