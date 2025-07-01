/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/colorspace.h"

#include <QColor>
#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QString>
#include <QVector2D>
#include <QVector3D>
#include <epoxy/gl.h>

namespace KWin
{

class KWIN_EXPORT GLShader
{
public:
    enum Flags {
        NoFlags = 0,
        ExplicitLinking = (1 << 0)
    };

    GLShader(const QString &vertexfile, const QString &fragmentfile, unsigned int flags = NoFlags);
    ~GLShader();

    bool isValid() const
    {
        return m_valid;
    }

    void bindAttributeLocation(const char *name, int index);
    void bindFragDataLocation(const char *name, int index);

    bool link();

    int uniformLocation(const char *name);

    bool setUniform(const char *name, float value);
    bool setUniform(const char *name, double value);
    bool setUniform(const char *name, int value);
    bool setUniform(const char *name, const QVector2D &value);
    bool setUniform(const char *name, const QVector3D &value);
    bool setUniform(const char *name, const QVector4D &value);
    bool setUniform(const char *name, const QMatrix3x3 &value);
    bool setUniform(const char *name, const QMatrix4x4 &value);
    bool setUniform(const char *name, const QColor &color);

    bool setUniform(int location, float value);
    bool setUniform(int location, double value);
    bool setUniform(int location, int value);
    bool setUniform(int location, int xValue, int yValue, int zValue);
    bool setUniform(int location, const QVector2D &value);
    bool setUniform(int location, const QVector3D &value);
    bool setUniform(int location, const QVector4D &value);
    bool setUniform(int location, const QMatrix3x3 &value);
    bool setUniform(int location, const QMatrix4x4 &value);
    bool setUniform(int location, const QColor &value);

    int attributeLocation(const char *name);
    bool setAttribute(const char *name, float value);
    /**
     * @return The value of the uniform as a matrix
     * @since 4.7
     */
    QMatrix4x4 getUniformMatrix4x4(const char *name);

    enum class Mat3Uniform {
    };

    enum class Mat4Uniform {
        TextureMatrix = 0,
        ProjectionMatrix,
        ModelViewMatrix,
        ModelViewProjectionMatrix,
        WindowTransformation,
        ScreenTransformation,
        ColorimetryTransformation,
        DestinationToLMS,
        LMSToDestination,
        YuvToRgb,
        MatrixCount
    };

    enum class Vec2Uniform {
        Offset,
        SourceTransferFunctionParams,
        DestinationTransferFunctionParams,
        Vec2UniformCount
    };

    enum class Vec3Uniform {
        PrimaryBrightness = 0
    };

    enum class Vec4Uniform {
        ModulationConstant,
        Box,
        CornerRadius,
        Vec4UniformCount
    };

    enum class FloatUniform {
        Saturation,
        MaxDestinationLuminance,
        SourceReferenceLuminance,
        DestinationReferenceLuminance,
        MaxTonemappingLuminance,
        FloatUniformCount
    };

    enum class IntUniform {
        AlphaToOne, ///< @deprecated no longer used
        TextureWidth,
        TextureHeight,
        SourceNamedTransferFunction,
        DestinationNamedTransferFunction,
        Sampler,
        Sampler1,
        IntUniformCount
    };

    enum class ColorUniform {
        Color,
        ColorUniformCount
    };

    bool setUniform(Mat3Uniform uniform, const QMatrix3x3 &value);
    bool setUniform(Mat4Uniform uniform, const QMatrix4x4 &matrix);
    bool setUniform(Vec2Uniform uniform, const QVector2D &value);
    bool setUniform(Vec3Uniform uniform, const QVector3D &value);
    bool setUniform(Vec4Uniform uniform, const QVector4D &value);
    bool setUniform(FloatUniform uniform, float value);
    bool setUniform(IntUniform uniform, int value);
    bool setUniform(ColorUniform uniform, const QVector4D &value);
    bool setUniform(ColorUniform uniform, const QColor &value);

    void setColorspaceUniforms(const ColorDescription &src, const ColorDescription &dst, RenderingIntent intent);

protected:
    GLShader(unsigned int flags = NoFlags);
    bool loadFromFiles(const QString &vertexfile, const QString &fragmentfile);
    bool load(const QByteArray &vertexSource, const QByteArray &fragmentSource);
    const QByteArray prepareSource(GLenum shaderType, const QByteArray &sourceCode) const;
    bool compile(GLuint program, GLenum shaderType, const QByteArray &sourceCode) const;
    void bind();
    void unbind();
    void resolveLocations();

private:
    unsigned int m_program;
    bool m_valid : 1;
    bool m_locationsResolved : 1;
    bool m_explicitLinking : 1;
    QHash<Mat3Uniform, int> m_matrix3Locations;
    QHash<Mat4Uniform, int> m_matrix4Locations;
    QHash<Vec2Uniform, int> m_vec2Locations;
    QHash<Vec3Uniform, int> m_vec3Locations;
    QHash<Vec4Uniform, int> m_vec4Locations;
    QHash<FloatUniform, int> m_floatLocations;
    QHash<IntUniform, int> m_intLocations;
    QHash<ColorUniform, int> m_colorLocations;

    friend class ShaderManager;
};

}
