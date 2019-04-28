/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2019 Vitaliy Filippov <vitalif@yourcmc.ru>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "icc.h"
#include "iccconfig.h"
#include "kwinglplatform.h"

#include "lcms2.h"

#include <QAction>
#include <QFile>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <QStandardPaths>

static const int LUT_POINTS = 64;

namespace KWin
{

ICCEffect::ICCEffect()
    :   m_valid(false),
        m_shader(NULL),
        m_texture(0),
        m_clut(NULL)
{
    initConfig<ICCConfig>();
    reconfigure(ReconfigureAll);
}

ICCEffect::~ICCEffect()
{
    if (m_shader)
        delete m_shader;
    if (m_clut)
        delete[] m_clut;
    if (m_texture != 0)
        glDeleteTextures(1, &m_texture);
}

bool ICCEffect::supported()
{
    return effects->compositingType() == OpenGL2Compositing;
}

void ICCEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)

    ICCConfig::self()->read();

    m_sourceICC = ICCConfig::self()->sourceICC().trimmed();
    m_targetICC = ICCConfig::self()->targetICC().trimmed();

    loadData();

    effects->addRepaintFull();
}

bool ICCEffect::loadData()
{
    m_valid = false;

    if (m_shader)
        delete m_shader;
    if (m_clut)
        delete[] m_clut;
    if (m_texture != 0)
        glDeleteTextures(1, &m_texture);

    m_shader = ShaderManager::instance()->generateShaderFromResources(ShaderTrait::MapTexture, QString(), QStringLiteral("icc.frag"));
    if (!m_shader->isValid())
    {
        qCCritical(KWINEFFECTS) << "The shader failed to load!";
        return false;
    }
    ShaderManager::instance()->pushShader(m_shader);
    m_shader->setUniform("clut", 3); // GL_TEXTURE3
    ShaderManager::instance()->popShader();

    m_clut = makeCLUT(m_sourceICC.trimmed().isEmpty() ? NULL : m_sourceICC.toLocal8Bit().data(), m_targetICC.toLocal8Bit().data());

    if (m_clut)
    {
        m_texture = setupCCTexture(m_clut);

        if (m_texture)
        {
            m_valid = true;
            return true;
        }
    }

    return false;
}

uint8_t *ICCEffect::makeCLUT(const char* source_icc, const char* target_icc)
{
    uint8_t *clut = NULL, *clut_source = NULL;
    cmsHPROFILE source, target;
    cmsHTRANSFORM transform;
    source = source_icc ? cmsOpenProfileFromFile(source_icc, "r") : cmsCreate_sRGBProfile();
    if (!source)
        goto free_nothing;
    target = cmsOpenProfileFromFile(target_icc, "r");
    if (!target)
        goto free_source;
    transform = cmsCreateTransform(
        source, TYPE_RGB_8, target, TYPE_RGB_8,
        INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_BLACKPOINTCOMPENSATION
    );
    if (!transform)
        goto free_target;
    clut_source = new uint8_t[LUT_POINTS*LUT_POINTS*LUT_POINTS*3];
    if (!clut_source)
        goto free_transform;
    for (int b = 0, addr = 0; b < LUT_POINTS; b++)
    {
        for (int g = 0; g < LUT_POINTS; g++)
        {
            for (int r = 0; r < LUT_POINTS; r++, addr += 3)
            {
                clut_source[addr] = 255*r/(LUT_POINTS-1);
                clut_source[addr+1] = 255*g/(LUT_POINTS-1);
                clut_source[addr+2] = 255*b/(LUT_POINTS-1);
            }
        }
    }
    clut = new uint8_t[LUT_POINTS*LUT_POINTS*LUT_POINTS*3];
    if (!clut)
        goto free_clut_source;
    cmsDoTransform(transform, clut_source, clut, LUT_POINTS*LUT_POINTS*LUT_POINTS);
    /*for (int b = 0, addr = 0; b < LUT_POINTS; b++)
    {
        for (int g = 0; g < LUT_POINTS; g++)
        {
            for (int r = 0; r < LUT_POINTS; r++, addr += 3)
            {
                printf("cLUT %02x%02x%02x -> %02x%02x%02x\n", 255*r/(LUT_POINTS-1), 255*g/(LUT_POINTS-1), 255*b/(LUT_POINTS-1), clut[addr], clut[addr+1], clut[addr+2]);
            }
        }
    }*/
free_clut_source:
    delete[] clut_source;
free_transform:
    cmsDeleteTransform(transform);
free_target:
    cmsCloseProfile(target);
free_source:
    cmsCloseProfile(source);
free_nothing:
    return clut;
}

GLuint ICCEffect::setupCCTexture(uint8_t *clut)
{
    GLenum err = glGetError();
    GLuint texture;

    glGenTextures(1, &texture);

    glBindTexture(GL_TEXTURE_3D, texture);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB8,
                 LUT_POINTS, LUT_POINTS, LUT_POINTS,
                 0, GL_RGB, GL_UNSIGNED_BYTE, clut);

    if ((err = glGetError()) != GL_NO_ERROR)
    {
        glDeleteTextures(1, &texture);
        return 0;
    }

    return texture;
}

void ICCEffect::drawWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (m_valid)
    {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_3D, m_texture);
        glActiveTexture(GL_TEXTURE0);
        ShaderManager *shaderManager = ShaderManager::instance();
        shaderManager->pushShader(m_shader);
        data.shader = m_shader;
    }

    effects->drawWindow(w, mask, region, data);

    if (m_valid)
    {
        ShaderManager::instance()->popShader();
    }
}

void ICCEffect::paintEffectFrame(KWin::EffectFrame* frame, QRegion region, double opacity, double frameOpacity)
{
    if (m_valid)
    {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_3D, m_texture);
        glActiveTexture(GL_TEXTURE0);
        frame->setShader(m_shader);
        ShaderBinder binder(m_shader);
        effects->paintEffectFrame(frame, region, opacity, frameOpacity);
    }
    else
    {
        effects->paintEffectFrame(frame, region, opacity, frameOpacity);
    }
}

bool ICCEffect::isActive() const
{
    return m_valid;
}

} // namespace
