/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2006-2007 Rivo Laks <rivolaks@hot.ee>
    SPDX-FileCopyrightText: 2010, 2011 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "colorspace.h"
#include "libkwineffects/kwinglutils_export.h"

#include <QColor>
#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QString>
#include <QVector2D>
#include <QVector3D>
#include <epoxy/gl.h>

namespace KWin
{

class KWINGLUTILS_EXPORT GLShader
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
        return mValid;
    }

    void bindAttributeLocation(const char *name, int index);
    void bindFragDataLocation(const char *name, int index);

    bool link();

    int uniformLocation(const char *name);

    bool setUniform(const char *name, float value);
    bool setUniform(const char *name, int value);
    bool setUniform(const char *name, const QVector2D &value);
    bool setUniform(const char *name, const QVector3D &value);
    bool setUniform(const char *name, const QVector4D &value);
    bool setUniform(const char *name, const QMatrix4x4 &value);
    bool setUniform(const char *name, const QColor &color);

    bool setUniform(int location, float value);
    bool setUniform(int location, int value);
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

    enum MatrixUniform {
        TextureMatrix = 0,
        ProjectionMatrix,
        ModelViewMatrix,
        ModelViewProjectionMatrix,
        WindowTransformation,
        ScreenTransformation,
        ColorimetryTransformation,
        MatrixCount
    };

    enum Vec2Uniform {
        Offset,
        Vec2UniformCount
    };

    enum class Vec3Uniform {
        PrimaryBrightness = 0
    };

    enum Vec4Uniform {
        ModulationConstant,
        Vec4UniformCount
    };

    enum FloatUniform {
        Saturation,
        MaxHdrBrightness,
        FloatUniformCount
    };

    enum IntUniform {
        AlphaToOne, ///< @deprecated no longer used
        TextureWidth,
        TextureHeight,
        SourceNamedTransferFunction,
        DestinationNamedTransferFunction,
        SdrBrightness,
        IntUniformCount
    };

    enum ColorUniform {
        Color,
        ColorUniformCount
    };

    bool setUniform(MatrixUniform uniform, const QMatrix3x3 &value);
    bool setUniform(MatrixUniform uniform, const QMatrix4x4 &matrix);
    bool setUniform(Vec2Uniform uniform, const QVector2D &value);
    bool setUniform(Vec3Uniform uniform, const QVector3D &value);
    bool setUniform(Vec4Uniform uniform, const QVector4D &value);
    bool setUniform(FloatUniform uniform, float value);
    bool setUniform(IntUniform uniform, int value);
    bool setUniform(ColorUniform uniform, const QVector4D &value);
    bool setUniform(ColorUniform uniform, const QColor &value);

    bool setColorspaceUniforms(const ColorDescription &src, const ColorDescription &dst);
    bool setColorspaceUniformsFromSRGB(const ColorDescription &dst);
    bool setColorspaceUniformsToSRGB(const ColorDescription &src);

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
    unsigned int mProgram;
    bool mValid : 1;
    bool mLocationsResolved : 1;
    bool mExplicitLinking : 1;
    int mMatrixLocation[MatrixCount];
    int mVec2Location[Vec2UniformCount];
    QHash<Vec3Uniform, int> m_vec3Locations;
    int mVec4Location[Vec4UniformCount];
    int mFloatLocation[FloatUniformCount];
    int mIntLocation[IntUniformCount];
    int mColorLocation[ColorUniformCount];

    friend class ShaderManager;
};

}
