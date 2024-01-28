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
    void setUniforms(const std::shared_ptr<IccProfile> &profile, float sdrBrightness, const QVector3D &channelFactors);

private:
    bool setProfile(const std::shared_ptr<IccProfile> &profile);

    std::unique_ptr<GLShader> m_shader;
    std::shared_ptr<IccProfile> m_profile;

    QMatrix4x4 m_toXYZD50;
    std::unique_ptr<GlLookUpTable> m_B;
    QMatrix4x4 m_matrix2;
    std::unique_ptr<GlLookUpTable> m_M;
    std::unique_ptr<GlLookUpTable3D> m_C;
    std::unique_ptr<GlLookUpTable> m_A;
    struct Locations
    {
        int src;
        int sdrBrightness;
        int toXYZD50;
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
