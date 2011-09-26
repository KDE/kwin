/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Fredrik Höglund <fredrik@kde.org>

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

#ifndef KWIN_GLPLATFORM_H
#define KWIN_GLPLATFORM_H

#include <kwinglutils_funcs.h>

#include <QSet>

namespace KWin
{

inline qint64 kVersionNumber(qint64 major, qint64 minor, qint64 patch = 0)
{
    return ((major & 0xffff) << 32) | ((minor & 0xffff) << 16) | (patch & 0xffff);
}

enum GLFeature {
    /**
     * Set when a texture bound to a pixmap uses the same storage as the pixmap,
     * and thus doesn't need to be rebound when the contents of the pixmap
     * has changed.
     */
    LooseBinding,

    /**
     * Set if the driver supports the following extensions:
     * - GL_ARB_shader_objects
     * - GL_ARB_fragment_shader
     * - GL_ARB_vertex_shader
     * - GL_ARB_shading_language_100
     */
    GLSL,

    /**
     * If set, assume the following:
     * - No flow control or branches
     * - No loops, unless the loops have a fixed iteration count and can be unrolled
     * - No functions, unless they can be inlined
     * - No indirect indexing of arrays
     * - No support for gl_ClipVertex or gl_FrontFacing
     * - No texture fetches in vertex shaders
     * - Max 32 texture fetches in fragment shaders
     * - Max 4 texture indirections
     */
    LimitedGLSL,

    /**
     * Set when the driver supports GL_ARB_texture_non_power_of_two.
     */
    TextureNPOT,

    /**
     * If set, the driver supports GL_ARB_texture_non_power_of_two with the
     * GL_ARB_texture_rectangle limitations.
     *
     * This means no support for mipmap filters, and that only the following
     * wrap modes are supported:
     * - GL_CLAMP
     * - GL_CLAMP_TO_EDGE
     * - GL_CLAMP_TO_BORDER
     */
    LimitedNPOT
};

enum Driver {
    Driver_R100,  // Technically "Radeon"
    Driver_R200,
    Driver_R300C,
    Driver_R300G,
    Driver_R600C,
    Driver_R600G,
    Driver_Nouveau,
    Driver_Intel,
    Driver_NVidia,
    Driver_Catalyst,
    Driver_Swrast,
    Driver_Softpipe,
    Driver_Llvmpipe,
    Driver_Unknown
};

enum ChipClass {
    // Radeon
    R100          = 0,      // GL1.3         DX7                   2000
    R200,                   // GL1.4         DX8.1     SM 1.4      2001
    R300,                   // GL2.0         DX9       SM 2.0      2002
    R400,                   // GL2.0         DX9b      SM 2.0b     2004
    R500,                   // GL2.0         DX9c      SM 3.0      2005
    R600,                   // GL3.3         DX10      SM 4.0      2006
    R700,                   // GL3.3         DX10.1    SM 4.1      2008
    Evergreen,              // GL4.0  CL1.0  DX11      SM 5.0      2009
    NorthernIslands,        // GL4.0  CL1.1  DX11      SM 5.0      2010
    UnknownRadeon = 999,

    // NVIDIA
    NV10          = 1000,   // GL1.2         DX7                   1999
    NV20,                   // GL1.3         DX8       SM 1.1      2001
    NV30,                   // GL1.5         DX9a      SM 2.0      2003
    NV40,                   // GL2.1         DX9c      SM 3.0      2004
    G80,                    // GL3.3         DX10      SM 4.0      2006
    GF100,                  // GL4.1  CL1.1  DX11      SM 5.0      2010
    UnknownNVidia = 1999,

    // Intel
    I8XX          = 2000,   // GL1.3         DX7                   2001
    I915,                   // GL1.4/1.5     DX9/DX9c  SM 2.0      2004
    I965,                   // GL2.0/2.1     DX9/DX10  SM 3.0/4.0  2006
    SandyBridge,            // GL3.1  CL1.1  DX10.1    SM 4.0      2010
    UnknownIntel  = 2999,

    UnknownChipClass = 99999
};


class KWIN_EXPORT GLPlatform
{
public:
    ~GLPlatform();

    /**
     * Runs the detection code using the current OpenGL context.
     */
    void detect();

    /**
     * Prints the results of the detection code.
     */
    void printResults() const;

    /**
     * Returns a pointer to the GLPlatform instance.
     */
    static GLPlatform *instance();

    /**
     * Returns true if the driver support the given feature, and false otherwise.
     */
    bool supports(GLFeature feature) const;

    /**
     * Returns the OpenGL version.
     */
    qint64 glVersion() const;

    /**
     * Returns the GLSL version if the driver supports GLSL, and 0 otherwise.
     */
    qint64 glslVersion() const;

    /**
     * Returns the Mesa version if the driver is a Mesa driver, and 0 otherwise.
     */
    qint64 mesaVersion() const;

    /**
     * Returns the Gallium version if the driver is a Gallium driver, and 0 otherwise.
     */
    qint64 galliumVersion() const;

    /**
     * Returns the X server version.
     *
     * Note that the version number changed from 7.2 to 1.3 in the first release
     * following the doupling of the X server from the katamari.
     *
     * For non X.org servers, this method returns 0.
     */
    qint64 serverVersion() const;

    /**
     * Returns the Linux kernel version.
     *
     * If the kernel is not a Linux kernel, this method returns 0.
     */
    qint64 kernelVersion() const;

    /**
     * Returns the driver version.
     *
     * For Mesa drivers, this is the same as the Mesa version number.
     */
    qint64 driverVersion() const;

    /**
     * Returns the driver.
     */
    Driver driver() const;

    /**
     * Returns the chip class.
     */
    ChipClass chipClass() const;

    /**
     * Returns true if the driver is a Mesa driver, and false otherwise.
     */
    bool isMesaDriver() const;

    /**
     * Returns true if the driver is a Gallium driver, and false otherwise.
     */
    bool isGalliumDriver() const;

    /**
     * Returns true if the GPU is a Radeon GPU, and false otherwise.
     */
    bool isRadeon() const;

    /**
     * Returns true if the GPU is an NVIDIA GPU, and false otherwise.
     */
    bool isNvidia() const;

    /**
     * Returns true if the GPU is an Intel GPU, and false otherwise.
     */
    bool isIntel() const;

    /**
     * @returns @c true if OpenGL is emulated in software.
     * @since 4.7
     **/
    bool isSoftwareEmulation() const;

private:
    GLPlatform();

private:
    QByteArray m_renderer;
    QByteArray m_vendor;
    QByteArray m_version;
    QByteArray m_glsl_version;
    QByteArray m_chipset;
    QSet<QByteArray> m_extensions;
    Driver m_driver;
    ChipClass m_chipClass;
    qint64 m_glVersion;
    qint64 m_glslVersion;
    qint64 m_mesaVersion;
    qint64 m_driverVersion;
    qint64 m_galliumVersion;
    qint64 m_serverVersion;
    qint64 m_kernelVersion;
    bool m_looseBinding: 1;
    bool m_directRendering: 1;
    bool m_supportsGLSL: 1;
    bool m_limitedGLSL: 1;
    bool m_textureNPOT: 1;
    bool m_limitedNPOT: 1;
    static GLPlatform *s_platform;
};

inline GLPlatform *GLPlatform::instance()
{
    if (!s_platform)
        s_platform = new GLPlatform;

    return s_platform;
}

} // namespace KWin

#endif // KWIN_GLPLATFORM_H

