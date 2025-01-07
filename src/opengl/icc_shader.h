/*
    SPDX-FileCopyrightText: 2023 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "core/colorspace.h"

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

class KWIN_EXPORT IccShader
{
public:
    explicit IccShader();
    ~IccShader();

    GLShader *shader() const;
    void setUniforms(const std::shared_ptr<IccProfile> &profile, const ColorDescription &inputColor, RenderingIntent intent);

private:
    bool setProfile(const std::shared_ptr<IccProfile> &profile, const ColorDescription &inputColor, RenderingIntent intent);

    std::unique_ptr<GLShader> m_shader;
    std::shared_ptr<IccProfile> m_profile;
    RenderingIntent m_intent = RenderingIntent::RelativeColorimetric;
    ColorDescription m_inputColor = ColorDescription::sRGB;

    QMatrix4x4 m_toXYZD50;
    std::unique_ptr<GlLookUpTable> m_B;
    QMatrix4x4 m_matrix2;
    std::unique_ptr<GlLookUpTable> m_M;
    std::unique_ptr<GlLookUpTable3D> m_C;
    std::unique_ptr<GlLookUpTable> m_A;
    struct Locations
    {
        int src;
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
