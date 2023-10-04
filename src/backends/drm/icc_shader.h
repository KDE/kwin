/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QMatrix4x4>
#include <QSizeF>
#include <memory>

namespace KWin
{

class IccProfile;
class GLShader;
class GlLookUpTable;
class GlLookUpTable3D;
class GLTexture;

class IccShader
{
public:
    explicit IccShader();
    ~IccShader();

    GLShader *shader() const;
    void setUniforms(const std::shared_ptr<IccProfile> &profile, float sdrBrightness);

private:
    bool setProfile(const std::shared_ptr<IccProfile> &profile);

    std::unique_ptr<GLShader> m_shader;
    std::shared_ptr<IccProfile> m_profile;

    QMatrix3x3 m_matrix1;
    std::unique_ptr<GlLookUpTable> m_B;
    QMatrix4x4 m_matrix2;
    std::unique_ptr<GlLookUpTable> m_M;
    std::unique_ptr<GlLookUpTable3D> m_C;
    std::unique_ptr<GlLookUpTable> m_A;
    struct Locations
    {
        int src;
        int sdrBrightness;
        int matrix1;
        int bsize;
        int bsampler;
        int matrix2;
        int msize;
        int msampler;
        int csize;
        int csampler;
        int asize;
        int asampler;
    };
    Locations m_locations;
};

}
