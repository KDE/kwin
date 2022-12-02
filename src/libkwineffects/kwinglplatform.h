/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <kwinglobals.h>
#include <kwinglutils_export.h>

#include <QByteArray>
#include <QSet>
#include <memory>

namespace KWin
{
// forward declare method
void cleanupGL();

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
    LimitedNPOT,

    /**
     * Set if the extension GL_MESA_pack_invert is present
     */
    PackInvert,
};

enum Driver {
    Driver_R100, // Technically "Radeon"
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
    Driver_VirtualBox,
    Driver_VMware,
    Driver_Qualcomm,
    Driver_RadeonSI,
    Driver_Virgl,
    Driver_Panfrost,
    Driver_Lima,
    Driver_VC4,
    Driver_V3D,
    Driver_Unknown,
};

// clang-format off
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
    SouthernIslands,        // GL4.5  CL1.2  DX11.1    SM 5.1      2012
    SeaIslands,             // GL4.5  CL2.0  DX12      SM 6.0      2013
    VolcanicIslands,        // GL4.5  CL2.0  DX12      SM 6.0      2015
    ArcticIslands,          // GL4.5  CL2.0  DX12      SM 6.0      2016
    Vega,                   // GL4.6  CL2.0  DX12      SM 6.0      2017
    Navi,                   // GL4.6  CL2.0  DX12.1    SM 6.4      2019
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
    I8XX          = 2000,   //       GL1.3         DX7                   2001
    I915,                   //       GL1.4/1.5     DX9/DX9c  SM 2.0      2004
    I965,                   //       GL2.0/2.1     DX9/DX10  SM 3.0/4.0  2006
    SandyBridge,            // Gen6  GL3.1  CL1.1  DX10.1    SM 4.0      2010
    IvyBridge,              // Gen7  GL4.0  CL1.1  DX11      SM 5.0      2012
    Haswell,                // Gen7  GL4.0  CL1.2  DX11.1    SM 5.0      2013
    BayTrail,               // Gen7  GL4.0  CL1.2  DX11.1    SM 5.0      2013
    Cherryview,             // Gen8  GL4.0  CL1.2  DX11.2    SM 5.0      2013
    Broadwell,              // Gen8  GL4.4  CL2.0  DX11.2    SM 5.0      2014
    ApolloLake,             // Gen9  GL4.6  CL3.0  DX12      SM 6.0      2016
    Skylake,                // Gen9  GL4.6  CL3.0  DX12      SM 6.0      2015
    GeminiLake,             // Gen9  GL4.6  CL3.0  DX12      SM 6.0      2017
    KabyLake,               // Gen9  GL4.6  CL3.0  DX12      SM 6.0      2017
    CoffeeLake,             // Gen9  GL4.6  CL3.0  DX12      SM 6.0      2018
    WhiskeyLake,            // Gen9  GL4.6  GL3.0  DX12      SM 6.0      2018
    CometLake,              // Gen9  GL4.6  GL3.0  DX12      SM 6.0      2019
    CannonLake,             // Gen10 GL4.6  GL3.0  DX12      SM 6.0      2018
    IceLake,                // Gen11 GL4.6  CL3.0  DX12.1    SM 6.0      2019
    TigerLake,              // Gen12 GL4.6  CL3.0  DX12.1    SM 6.0      2020
    UnknownIntel  = 2999,

    // Qualcomm Adreno
    // from https://en.wikipedia.org/wiki/Adreno
    Adreno1XX     = 3000,   // GLES1.1
    Adreno2XX,              // GLES2.0       DX9c
    Adreno3XX,              // GLES3.0 CL1.1 DX11.1
    Adreno4XX,              // GLES3.1 CL1.2 DX11.2
    Adreno5XX,              // GLES3.1 CL2.0 DX11.2
    UnknownAdreno = 3999,

    // Panfrost Mali
    // from https://docs.mesa3d.org/drivers/panfrost.html
    MaliT7XX      = 4000,   // GLES2.0/GLES3.0
    MaliT8XX,               // GLES3.0
    MaliGXX,                // GLES3.0
    UnknownPanfrost = 4999,

    // Lima Mali
    // from https://docs.mesa3d.org/drivers/lima.html
    Mali400       = 5000,
    Mali450,
    Mali470,
    UnknownLima = 5999,

    // Broadcom VideoCore IV (e.g. Raspberry Pi 0 to 3), GLES 2.0/2.1 with caveats
    VC4_2_1       = 6000, // Found in Raspberry Pi 3B+
    UnknownVideoCore4 = 6999,

    // Broadcom VideoCore 3D (e.g. Raspberry Pi 4, Raspberry Pi 400)
    V3D_4_2       = 7000, // Found in Raspberry Pi 400
    UnknownVideoCore3D = 7999,

    UnknownChipClass = 99999,
};
// clang-format on

class KWINGLUTILS_EXPORT GLPlatform
{
public:
    ~GLPlatform();

    /**
     * Runs the detection code using the current OpenGL context.
     */
    void detect(OpenGLPlatformInterface platformInterface);

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
     * @returns @c true if the "GPU" is a VirtualBox GPU, and @c false otherwise.
     * @since 4.10
     */
    bool isVirtualBox() const;

    /**
     * @returns @c true if the "GPU" is a VMWare GPU, and @c false otherwise.
     * @since 4.10
     */
    bool isVMware() const;

    /**
     * @returns @c true if OpenGL is emulated in software.
     * @since 4.7
     */
    bool isSoftwareEmulation() const;

    /**
     * @returns @c true if the driver is known to be from a virtual machine.
     * @since 4.10
     */
    bool isVirtualMachine() const;

    /**
     * @returns @c true if the GPU is a Qualcomm Adreno GPU, and false otherwise
     * @since 5.8
     */
    bool isAdreno() const;

    /**
     * @returns @c true if the "GPU" is a virtio-gpu (Qemu/KVM)
     * @since 5.18
     **/
    bool isVirgl() const;

    /**
     * @returns @c true if the "GPU" is a Panfrost Mali GPU
     * @since 5.21.5
     **/
    bool isPanfrost() const;

     /**
     * @returns @c true if the GPU is a Mali GPU supported by the Lima driver (Mali 400, 450)
     * @since 5.27.1
     **/
    bool isLima() const;

    /**
     * @returns @c true if the GPU is a Broadcom VideoCore IV (e.g. Raspberry Pi 0 to 3)
     * @since 5.27.1
     **/
    bool isVideoCore4() const;

    /**
     * @returns @c true if the GPU is a Broadcom VideoCore 3D (e.g. Raspberry Pi 4, 400)
     * @since 5.27.1
     **/
    bool isVideoCore3D() const;

    /**
     * @returns the GL_VERSION string as provided by the driver.
     * @since 4.9
     */
    const QByteArray &glVersionString() const;
    /**
     * @returns the GL_RENDERER string as provided by the driver.
     * @since 4.9
     */
    const QByteArray &glRendererString() const;
    /**
     * @returns the GL_VENDOR string as provided by the driver.
     * @since 4.9
     */
    const QByteArray &glVendorString() const;
    /**
     * @returns the GL_SHADING_LANGUAGE_VERSION string as provided by the driver.
     * If the driver does not support the OpenGL Shading Language a null bytearray is returned.
     * @since 4.9
     */
    const QByteArray &glShadingLanguageVersionString() const;
    /**
     * @returns Whether the driver supports loose texture binding.
     * @since 4.9
     */
    bool isLooseBinding() const;
    /**
     * @returns Whether OpenGL ES is used
     */
    bool isGLES() const;

    /**
     * @returns The CompositingType recommended by the driver.
     * @since 4.10
     */
    CompositingType recommendedCompositor() const;

    /**
     * Returns true if glMapBufferRange() is likely to perform worse than glBufferSubData()
     * when updating an unused range of a buffer object, and false otherwise.
     *
     * @since 4.11
     */
    bool preferBufferSubData() const;

    /**
     * @returns The OpenGLPlatformInterface currently used
     * @since 5.0
     */
    OpenGLPlatformInterface platformInterface() const;

    /**
     * @returns a human readable form of the @p version as a QString.
     * @since 4.9
     * @see glVersion
     * @see glslVersion
     * @see driverVersion
     * @see mesaVersion
     * @see galliumVersion
     * @see kernelVersion
     * @see serverVersion
     */
    static QString versionToString(qint64 version);
    /**
     * @returns a human readable form of the @p version as a QByteArray.
     * @since 5.5
     * @see glVersion
     * @see glslVersion
     * @see driverVersion
     * @see mesaVersion
     * @see galliumVersion
     * @see kernelVersion
     * @see serverVersion
     */
    static QByteArray versionToString8(qint64 version);

    /**
     * @returns a human readable form for the @p driver as a QString.
     * @since 4.9
     * @see driver
     */
    static QString driverToString(Driver driver);
    /**
     * @returns a human readable form for the @p driver as a QByteArray.
     * @since 5.5
     * @see driver
     */
    static QByteArray driverToString8(Driver driver);

    /**
     * @returns a human readable form for the @p chipClass as a QString.
     * @since 4.9
     * @see chipClass
     */
    static QString chipClassToString(ChipClass chipClass);
    /**
     * @returns a human readable form for the @p chipClass as a QByteArray.
     * @since 5.5
     * @see chipClass
     */
    static QByteArray chipClassToString8(ChipClass chipClass);

private:
    GLPlatform();
    friend void KWin::cleanupGL();
    static void cleanup();

private:
    QByteArray m_renderer;
    QByteArray m_vendor;
    QByteArray m_version;
    QByteArray m_glsl_version;
    QByteArray m_chipset;
    QSet<QByteArray> m_extensions;
    Driver m_driver;
    ChipClass m_chipClass;
    CompositingType m_recommendedCompositor;
    qint64 m_glVersion;
    qint64 m_glslVersion;
    qint64 m_mesaVersion;
    qint64 m_driverVersion;
    qint64 m_galliumVersion;
    qint64 m_serverVersion;
    qint64 m_kernelVersion;
    bool m_looseBinding : 1;
    bool m_supportsGLSL : 1;
    bool m_limitedGLSL : 1;
    bool m_textureNPOT : 1;
    bool m_limitedNPOT : 1;
    bool m_packInvert : 1;
    bool m_virtualMachine : 1;
    bool m_preferBufferSubData : 1;
    OpenGLPlatformInterface m_platformInterface;
    bool m_gles : 1;
    static std::unique_ptr<GLPlatform> s_platform;
};

inline GLPlatform *GLPlatform::instance()
{
    if (!s_platform) {
        s_platform.reset(new GLPlatform());
    }
    return s_platform.get();
}

} // namespace KWin
