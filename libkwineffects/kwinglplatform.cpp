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

#include "kwinglplatform.h"
#include "kwinglutils.h"

#include <QRegExp>
#include <QStringList>
#include <QDebug>

#include <sys/utsname.h>

#include <iostream>
#include <iomanip>
#include <ios>

namespace KWin
{

GLPlatform *GLPlatform::s_platform = 0;

static qint64 parseVersionString(const QByteArray &version)
{
    // Skip any leading non digit
    int start = 0;
    while (start < version.length() && !QChar::fromLatin1(version[start]).isDigit())
        start++;

    // Strip any non digit, non '.' characters from the end
    int end = start;
    while (end < version.length() && (version[end] == '.' || QChar::fromLatin1(version[end]).isDigit()))
        end++;

    const QByteArray result = version.mid(start, end-start);
    const QList<QByteArray> tokens = result.split('.');
    const qint64 major = tokens.at(0).toInt();
    const qint64 minor = tokens.count() > 1 ? tokens.at(1).toInt() : 0;
    const qint64 patch = tokens.count() > 2 ? tokens.at(2).toInt() : 0;

    return kVersionNumber(major, minor, patch);
}

static qint64 getXServerVersion()
{
    qint64 major, minor, patch;

    Display *dpy = display();
    if (strstr(ServerVendor(dpy), "X.Org")) {
        const int release  = VendorRelease(dpy);
        major = (release / 10000000);
        minor = (release /   100000) % 100;
        patch = (release /     1000) % 100;
    } else {
        major = 0;
        minor = 0;
        patch = 0;
    }

    return kVersionNumber(major, minor, patch);
}

static qint64 getKernelVersion()
{
    struct utsname name;
    uname(&name);

    if (QByteArray(name.sysname) == "Linux")
        return parseVersionString(name.release);

    return 0;
}

// Extracts the portion of a string that matches a regular expression
static QString extract(const QString &string, const QString &match, int offset = 0)
{
    QString result;
    QRegExp rx(match);
    int pos = rx.indexIn(string, offset);
    if (pos != -1)
        result = string.mid(pos, rx.matchedLength());
    return result;
}

static ChipClass detectRadeonClass(const QString &chipset)
{
    if (chipset.isEmpty())
        return UnknownRadeon;

    if (chipset.contains(QStringLiteral("R100"))  ||
        chipset.contains(QStringLiteral("RV100")) ||
        chipset.contains(QStringLiteral("RS100")))
        return R100;

    if (chipset.contains(QStringLiteral("RV200")) ||
        chipset.contains(QStringLiteral("RS200")) ||
        chipset.contains(QStringLiteral("R200"))  ||
        chipset.contains(QStringLiteral("RV250")) ||
        chipset.contains(QStringLiteral("RS300")) ||
        chipset.contains(QStringLiteral("RV280")))
        return R200;

    if (chipset.contains(QStringLiteral("R300"))  ||
        chipset.contains(QStringLiteral("R350"))  ||
        chipset.contains(QStringLiteral("R360"))  ||
        chipset.contains(QStringLiteral("RV350")) ||
        chipset.contains(QStringLiteral("RV370")) ||
        chipset.contains(QStringLiteral("RV380")))
        return R300;

    if (chipset.contains(QStringLiteral("R420"))  ||
        chipset.contains(QStringLiteral("R423"))  ||
        chipset.contains(QStringLiteral("R430"))  ||
        chipset.contains(QStringLiteral("R480"))  ||
        chipset.contains(QStringLiteral("R481"))  ||
        chipset.contains(QStringLiteral("RV410")) ||
        chipset.contains(QStringLiteral("RS400")) ||
        chipset.contains(QStringLiteral("RC410")) ||
        chipset.contains(QStringLiteral("RS480")) ||
        chipset.contains(QStringLiteral("RS482")) ||
        chipset.contains(QStringLiteral("RS600")) ||
        chipset.contains(QStringLiteral("RS690")) ||
        chipset.contains(QStringLiteral("RS740")))
        return R400;

    if (chipset.contains(QStringLiteral("RV515")) ||
        chipset.contains(QStringLiteral("R520"))  ||
        chipset.contains(QStringLiteral("RV530")) ||
        chipset.contains(QStringLiteral("R580"))  ||
        chipset.contains(QStringLiteral("RV560")) ||
        chipset.contains(QStringLiteral("RV570")))
        return R500;

    if (chipset.contains(QStringLiteral("R600"))  ||
        chipset.contains(QStringLiteral("RV610")) ||
        chipset.contains(QStringLiteral("RV630")) ||
        chipset.contains(QStringLiteral("RV670")) ||
        chipset.contains(QStringLiteral("RV620")) ||
        chipset.contains(QStringLiteral("RV635")) ||
        chipset.contains(QStringLiteral("RS780")) ||
        chipset.contains(QStringLiteral("RS880")))
        return R600;

    if (chipset.contains(QStringLiteral("R700"))  ||
        chipset.contains(QStringLiteral("RV770")) ||
        chipset.contains(QStringLiteral("RV730")) ||
        chipset.contains(QStringLiteral("RV710")) ||
        chipset.contains(QStringLiteral("RV740")))
        return R700;

    if (chipset.contains(QStringLiteral("EVERGREEN")) ||  // Not an actual chipset, but returned by R600G in 7.9
        chipset.contains(QStringLiteral("CEDAR"))     ||
        chipset.contains(QStringLiteral("REDWOOD"))   ||
        chipset.contains(QStringLiteral("JUNIPER"))   ||
        chipset.contains(QStringLiteral("CYPRESS"))   ||
        chipset.contains(QStringLiteral("HEMLOCK"))   ||
        chipset.contains(QStringLiteral("PALM")))
        return Evergreen;

    if (chipset.contains(QStringLiteral("SUMO"))   ||
        chipset.contains(QStringLiteral("SUMO2"))  ||
        chipset.contains(QStringLiteral("BARTS"))  ||
        chipset.contains(QStringLiteral("TURKS"))  ||
        chipset.contains(QStringLiteral("CAICOS")) ||
        chipset.contains(QStringLiteral("CAYMAN")))
        return NorthernIslands;

    QString name = extract(chipset, QStringLiteral("HD [0-9]{4}")); // HD followed by a space and 4 digits
    if (!name.isEmpty()) {
        const int id = name.right(4).toInt();
        if (id == 6250 || id == 6310)   // Palm
            return Evergreen;

        if (id >= 6000 && id < 7000)
            return NorthernIslands;    // HD 6xxx

        if (id >= 5000 && id < 6000)
            return Evergreen;          // HD 5xxx

        if (id >= 4000 && id < 5000)
            return R700;               // HD 4xxx

        if (id >= 2000 && id < 4000)    // HD 2xxx/3xxx
            return R600;

        return UnknownRadeon;
    }

    name = extract(chipset, QStringLiteral("X[0-9]{3,4}")); // X followed by 3-4 digits
    if (!name.isEmpty()) {
        const int id = name.mid(1, -1).toInt();

        // X1xxx
        if (id >= 1300)
            return R500;

        // X7xx, X8xx, X12xx, 2100
        if ((id >= 700 && id < 1000) || id >= 1200)
            return R400;

        // X200, X3xx, X5xx, X6xx, X10xx, X11xx
        if ((id >= 300 && id < 700) || (id >= 1000 && id < 1200))
            return R300;

        return UnknownRadeon;
    }

    name = extract(chipset, QStringLiteral("\\b[0-9]{4}\\b")); // A group of 4 digits
    if (!name.isEmpty()) {
        const int id = name.toInt();

        // 7xxx
        if (id >= 7000 && id < 8000)
            return R100;

        // 8xxx, 9xxx
        if (id >= 8000 && id < 9500)
            return R200;

        // 9xxx
        if (id >= 9500)
            return R300;

        if (id == 2100)
            return R400;
    }

    return UnknownRadeon;
}

static ChipClass detectNVidiaClass(const QString &chipset)
{
    QString name = extract(chipset, QStringLiteral("\\bNV[0-9,A-F]{2}\\b")); // NV followed by two hexadecimal digits
    if (!name.isEmpty()) {
        const int id = chipset.mid(2, -1).toInt(0, 16); // Strip the 'NV' from the id

        switch(id & 0xf0) {
        case 0x00:
        case 0x10:
            return NV10;

        case 0x20:
            return NV20;

        case 0x30:
            return NV30;

        case 0x40:
        case 0x60:
            return NV40;

        case 0x50:
        case 0x80:
        case 0x90:
        case 0xA0:
            return G80;

        default:
            return UnknownNVidia;
        }
    }

    if (chipset.contains(QStringLiteral("GeForce2")) || chipset.contains(QStringLiteral("GeForce 256")))
        return NV10;

    if (chipset.contains(QStringLiteral("GeForce3")))
        return NV20;

    if (chipset.contains(QStringLiteral("GeForce4"))) {
        if (chipset.contains(QStringLiteral("MX 420"))  ||
                chipset.contains(QStringLiteral("MX 440"))  || // including MX 440SE
                chipset.contains(QStringLiteral("MX 460"))  ||
                chipset.contains(QStringLiteral("MX 4000")) ||
                chipset.contains(QStringLiteral("PCX 4300")))
            return NV10;

        return NV20;
    }

    // GeForce 5,6,7,8,9
    name = extract(chipset, QStringLiteral("GeForce (FX |PCX |Go )?\\d{4}(M|\\b)")).trimmed();
    if (!name.isEmpty()) {
        if (!name[name.length() - 1].isDigit())
            name.chop(1);

        const int id = name.right(4).toInt();
        if (id < 6000)
            return NV30;

        if (id >= 6000 && id < 8000)
            return NV40;

        if (id >= 8000)
            return G80;

        return UnknownNVidia;
    }

    // GeForce 100/200/300/400/500
    name = extract(chipset, QStringLiteral("GeForce (G |GT |GTX |GTS )?\\d{3}(M|\\b)")).trimmed();
    if (!name.isEmpty()) {
        if (!name[name.length() - 1].isDigit())
            name.chop(1);

        const int id = name.right(3).toInt();
        if (id >= 100 && id < 600) {
            if (id >= 400)
                return GF100;

            return G80;
        }
        return UnknownNVidia;
    }

    return UnknownNVidia;
}

static ChipClass detectIntelClass(const QByteArray &chipset)
{
    // see mesa repository: src/mesa/drivers/dri/intel/intel_context.c
    // GL 1.3, DX8? SM ?
    if (chipset.contains("845G")        ||
            chipset.contains("830M")        ||
            chipset.contains("852GM/855GM") ||
            chipset.contains("865G"))
        return I8XX;

    // GL 1.4, DX 9.0, SM 2.0
    if (chipset.contains("915G")   ||
            chipset.contains("E7221G") ||
            chipset.contains("915GM")  ||
            chipset.contains("945G")   ||   // DX 9.0c
            chipset.contains("945GM")  ||
            chipset.contains("945GME") ||
            chipset.contains("Q33")    ||   // GL1.5
            chipset.contains("Q35")    ||
            chipset.contains("G33")    ||
            chipset.contains("965Q")   ||   // GMA 3000, but apparently considered gen 4 by the driver
            chipset.contains("946GZ")  ||   // GMA 3000, but apparently considered gen 4 by the driver
            chipset.contains("IGD"))
        return I915;

    // GL 2.0, DX 9.0c, SM 3.0
    if (chipset.contains("965G")       ||
            chipset.contains("G45/G43")    || // SM 4.0
            chipset.contains("965GM")      || // GL 2.1
            chipset.contains("965GME/GLE") ||
            chipset.contains("GM45")       ||
            chipset.contains("Q45/Q43")    ||
            chipset.contains("G41")        ||
            chipset.contains("B43")        ||
            chipset.contains("Ironlake"))
        return I965;

    // GL 3.1, CL 1.1, DX 10.1
    if (chipset.contains("Sandybridge")) {
        return SandyBridge;
    }

    // GL4.0, CL1.1, DX11, SM 5.0
    if (chipset.contains("Ivybridge")) {
        return IvyBridge;
    }

    // GL4.0, CL1.2, DX11.1, SM 5.0
    if (chipset.contains("Haswell")) {
        return Haswell;
    }

    return UnknownIntel;
}

QString GLPlatform::versionToString(qint64 version)
{
    int major = (version >> 32);
    int minor = (version >> 16) & 0xffff;
    int patch = version & 0xffff;

    QString string = QString::number(major) + QStringLiteral(".") + QString::number(minor);
    if (patch != 0)
        string += QStringLiteral(".") + QString::number(patch);

    return string;
}

QString GLPlatform::driverToString(Driver driver)
{
    switch(driver) {
    case Driver_R100:
        return QStringLiteral("Radeon");
    case Driver_R200:
        return QStringLiteral("R200");
    case Driver_R300C:
        return QStringLiteral("R300C");
    case Driver_R300G:
        return QStringLiteral("R300G");
    case Driver_R600C:
        return QStringLiteral("R600C");
    case Driver_R600G:
        return QStringLiteral("R600G");
    case Driver_Nouveau:
        return QStringLiteral("Nouveau");
    case Driver_Intel:
        return QStringLiteral("Intel");
    case Driver_NVidia:
        return QStringLiteral("NVIDIA");
    case Driver_Catalyst:
        return QStringLiteral("Catalyst");
    case Driver_Swrast:
        return QStringLiteral("Software rasterizer");
    case Driver_Softpipe:
        return QStringLiteral("softpipe");
    case Driver_Llvmpipe:
        return QStringLiteral("LLVMpipe");
    case Driver_VirtualBox:
        return QStringLiteral("VirtualBox (Chromium)");
    case Driver_VMware:
        return QStringLiteral("VMware (SVGA3D)");

    default:
        return QStringLiteral("Unknown");
    }
}

QString GLPlatform::chipClassToString(ChipClass chipClass)
{
    switch(chipClass) {
    case R100:
        return QStringLiteral("R100");
    case R200:
        return QStringLiteral("R200");
    case R300:
        return QStringLiteral("R300");
    case R400:
        return QStringLiteral("R400");
    case R500:
        return QStringLiteral("R500");
    case R600:
        return QStringLiteral("R600");
    case R700:
        return QStringLiteral("R700");
    case Evergreen:
        return QStringLiteral("EVERGREEN");
    case NorthernIslands:
        return QStringLiteral("NI");

    case NV10:
        return QStringLiteral("NV10");
    case NV20:
        return QStringLiteral("NV20");
    case NV30:
        return QStringLiteral("NV30");
    case NV40:
        return QStringLiteral("NV40/G70");
    case G80:
        return QStringLiteral("G80/G90");
    case GF100:
        return QStringLiteral("GF100");

    case I8XX:
        return QStringLiteral("i830/i835");
    case I915:
        return QStringLiteral("i915/i945");
    case I965:
        return QStringLiteral("i965");
    case SandyBridge:
        return QStringLiteral("SandyBridge");
    case IvyBridge:
        return QStringLiteral("IvyBridge");
    case Haswell:
        return QStringLiteral("Haswell");

    default:
        return QStringLiteral("Unknown");
    }
}



// -------



GLPlatform::GLPlatform()
    : m_driver(Driver_Unknown),
      m_chipClass(UnknownChipClass),
      m_recommendedCompositor(XRenderCompositing),
      m_mesaVersion(0),
      m_galliumVersion(0),
      m_looseBinding(false),
      m_directRendering(false),
      m_supportsGLSL(false),
      m_limitedGLSL(false),
      m_textureNPOT(false),
      m_limitedNPOT(false),
      m_virtualMachine(false)
{
}

GLPlatform::~GLPlatform()
{
}

void GLPlatform::detect(OpenGLPlatformInterface platformInterface)
{
    m_vendor       = (const char*)glGetString(GL_VENDOR);
    m_renderer     = (const char*)glGetString(GL_RENDERER);
    m_version      = (const char*)glGetString(GL_VERSION);

    // Parse the OpenGL version
    const QList<QByteArray> versionTokens = m_version.split(' ');
    if (versionTokens.count() > 0) {
        const QByteArray version = QByteArray(m_version);
        m_glVersion = parseVersionString(version);
    }

#ifndef KWIN_HAVE_OPENGLES
    if (m_glVersion >= kVersionNumber(3, 0)) {
        PFNGLGETSTRINGIPROC glGetStringi;

#ifdef KWIN_HAVE_EGL
        if (platformInterface == EglPlatformInterface)
            glGetStringi = (PFNGLGETSTRINGIPROC) eglGetProcAddress("glGetStringi");
        else
#endif
            glGetStringi = (PFNGLGETSTRINGIPROC) glXGetProcAddress((const GLubyte *) "glGetStringi");

        int count;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);

        for (int i = 0; i < count; i++) {
            const char *name = (const char *) glGetStringi(GL_EXTENSIONS, i);
            m_extensions.insert(name);
        }
    } else
#endif
    {
        const QByteArray extensions = (const char *) glGetString(GL_EXTENSIONS);
        m_extensions = QSet<QByteArray>::fromList(extensions.split(' '));
    }

    // Parse the Mesa version
    const int mesaIndex = versionTokens.indexOf("Mesa");
    if (mesaIndex != -1) {
        const QByteArray version = versionTokens.at(mesaIndex + 1);
        m_mesaVersion = parseVersionString(version);
    }

    if (platformInterface == EglPlatformInterface) {
        m_directRendering = true;
#ifdef KWIN_HAVE_OPENGLES
        m_supportsGLSL = true;
        m_textureNPOT = true;
#else
        m_supportsGLSL = m_extensions.contains("GL_ARB_shader_objects") &&
                         m_extensions.contains("GL_ARB_fragment_shader") &&
                         m_extensions.contains("GL_ARB_vertex_shader");

        m_textureNPOT = m_extensions.contains("GL_ARB_texture_non_power_of_two");
#endif
    } else if (platformInterface == GlxPlatformInterface) {
#ifndef KWIN_HAVE_OPENGLES
        GLXContext ctx = glXGetCurrentContext();
        m_directRendering = glXIsDirect(display(), ctx);

        m_supportsGLSL = m_directRendering &&
                         m_extensions.contains("GL_ARB_shader_objects") &&
                         m_extensions.contains("GL_ARB_fragment_shader") &&
                         m_extensions.contains("GL_ARB_vertex_shader");

        m_textureNPOT = m_extensions.contains("GL_ARB_texture_non_power_of_two");
#endif
    }

    m_serverVersion = getXServerVersion();
    m_kernelVersion = getKernelVersion();

    m_glslVersion = 0;
    m_glsl_version = QByteArray();

    if (m_supportsGLSL) {
        // Parse the GLSL version
        m_glsl_version = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
        m_glslVersion = parseVersionString(m_glsl_version);
    }

    m_chipset = "Unknown";
    m_preferBufferSubData = false;


    // Mesa classic drivers
    // ====================================================

    // Radeon
    if (m_renderer.startsWith("Mesa DRI R")) {
        // Sample renderer string: Mesa DRI R600 (RV740 94B3) 20090101 x86/MMX/SSE2 TCL DRI2
        const QList<QByteArray> tokens = m_renderer.split(' ');
        const QByteArray chipClass = tokens.at(2);
        m_chipset = tokens.at(3).mid(1, -1); // Strip the leading '('

        if (chipClass == "R100")
            // Vendor: Tungsten Graphics, Inc.
            m_driver = Driver_R100;

        else if (chipClass == "R200")
            // Vendor: Tungsten Graphics, Inc.
            m_driver = Driver_R200;

        else if (chipClass == "R300")
            // Vendor: DRI R300 Project
            m_driver = Driver_R300C;

        else if (chipClass == "R600")
            // Vendor: Advanced Micro Devices, Inc.
            m_driver = Driver_R600C;

        m_chipClass = detectRadeonClass(QString::fromUtf8(m_chipset));
    }

    // Intel
    else if (m_renderer.contains("Intel")) {
        // Vendor: Tungsten Graphics, Inc.
        // Sample renderer string: Mesa DRI Mobile Intel® GM45 Express Chipset GEM 20100328 2010Q1

        QByteArray chipset;
        if (m_renderer.startsWith("Intel(R) Integrated Graphics Device"))
            chipset = "IGD";
        else
            chipset = m_renderer;

        m_driver = Driver_Intel;
        m_chipClass = detectIntelClass(chipset);
    }

    // Gallium drivers
    // ====================================================
    else if (m_renderer.contains("Gallium")) {
        // Sample renderer string: Gallium 0.4 on AMD RV740
        const QList<QByteArray> tokens = m_renderer.split(' ');
        m_galliumVersion = parseVersionString(tokens.at(1));
        m_chipset = (tokens.at(3) == "AMD" || tokens.at(3) == "ATI") ?
                    tokens.at(4) : tokens.at(3);

        // R300G
        if (m_vendor == "X.Org R300 Project") {
            m_chipClass = detectRadeonClass(QString::fromUtf8(m_chipset));
            m_driver = Driver_R300G;
        }

        // R600G
        else if (m_vendor == "X.Org" &&
                (m_renderer.contains("R6")        ||
                 m_renderer.contains("R7")        ||
                 m_renderer.contains("RV6")       ||
                 m_renderer.contains("RV7")       ||
                 m_renderer.contains("RS780")     ||
                 m_renderer.contains("RS880")     ||
                 m_renderer.contains("CEDAR")     ||
                 m_renderer.contains("REDWOOD")   ||
                 m_renderer.contains("JUNIPER")   ||
                 m_renderer.contains("CYPRESS")   ||
                 m_renderer.contains("HEMLOCK")   ||
                 m_renderer.contains("PALM")      ||
                 m_renderer.contains("EVERGREEN") ||
                 m_renderer.contains("SUMO")      ||
                 m_renderer.contains("SUMO2")     ||
                 m_renderer.contains("BARTS")     ||
                 m_renderer.contains("TURKS")     ||
                 m_renderer.contains("CAICOS")    ||
                 m_renderer.contains("CAYMAN"))) {
            m_chipClass = detectRadeonClass(QString::fromUtf8(m_chipset));
            m_driver = Driver_R600G;
        }

        // Nouveau
        else if (m_vendor == "nouveau") {
            m_chipClass = detectNVidiaClass(QString::fromUtf8(m_chipset));
            m_driver = Driver_Nouveau;
        }

        // softpipe
        else if (m_vendor == "VMware, Inc." && m_chipset == "softpipe" ) {
            m_driver = Driver_Softpipe;
        }

        // llvmpipe
        else if (m_vendor == "VMware, Inc." && m_chipset == "llvmpipe") {
            m_driver = Driver_Llvmpipe;
        }

        // SVGA3D
        else if (m_vendor == "VMware, Inc." && m_chipset.contains("SVGA3D")) {
            m_driver = Driver_VMware;
        }
    }


    // Properietary drivers
    // ====================================================
    else if (m_vendor == "ATI Technologies Inc.") {
        m_chipClass = detectRadeonClass(QString::fromUtf8(m_renderer));
        m_driver = Driver_Catalyst;

        if (versionTokens.count() > 1 && versionTokens.at(2)[0] == '(')
            m_driverVersion = parseVersionString(versionTokens.at(1));
        else if (versionTokens.count() > 0)
            m_driverVersion = parseVersionString(versionTokens.at(0));
        else
            m_driverVersion = 0;
    }

    else if (m_vendor == "NVIDIA Corporation") {
        m_chipClass = detectNVidiaClass(QString::fromUtf8(m_renderer));
        m_driver = Driver_NVidia;

        int index = versionTokens.indexOf("NVIDIA");
        if (versionTokens.count() > index)
            m_driverVersion = parseVersionString(versionTokens.at(index + 1));
        else
            m_driverVersion = 0;
    }

    else if (m_renderer == "Software Rasterizer") {
        m_driver = Driver_Swrast;
    }

    // Virtual Hardware
    // ====================================================
    else if (m_vendor == "Humper" && m_renderer == "Chromium") {
        // Virtual Box
        m_driver = Driver_VirtualBox;

        const int index = versionTokens.indexOf("Chromium");
        if (versionTokens.count() > index)
            m_driverVersion = parseVersionString(versionTokens.at(index + 1));
        else
            m_driverVersion = 0;
    }


    // Driver/GPU specific features
    // ====================================================
    if (isRadeon()) {
        // R200 technically has a programmable pipeline, but since it's SM 1.4,
        // it's too limited to to be of any practical value to us.
        if (m_chipClass < R300)
            m_supportsGLSL = false;

        m_limitedGLSL = false;
        m_limitedNPOT = false;

        if (m_chipClass < R600) {
            if (driver() == Driver_Catalyst)
                m_textureNPOT = m_limitedNPOT = false; // Software fallback
            else if (driver() == Driver_R300G)
                m_limitedNPOT = m_textureNPOT;

            m_limitedGLSL = m_supportsGLSL;
        }

        if (m_chipClass < R300) {
            // fallback to XRender for R100 and R200
            m_recommendedCompositor = XRenderCompositing;
        } else if (m_chipClass < R600) {
            // XRender due to NPOT limitations not supported by KWin's shaders
            m_recommendedCompositor = XRenderCompositing;
        } else {
            m_recommendedCompositor = OpenGL2Compositing;
        }

        if (driver() == Driver_R600G ||
                (driver() == Driver_R600C && m_renderer.contains("DRI2"))) {
            m_looseBinding = true;
        }
    }

    if (isNvidia()) {
        if (m_driver == Driver_NVidia && m_chipClass < NV40)
            m_supportsGLSL = false; // High likelihood of software emulation

        if (m_driver == Driver_NVidia) {
            m_looseBinding = true;
            m_preferBufferSubData = true;
        }

        if (m_chipClass < NV40) {
            m_recommendedCompositor = XRenderCompositing;
        } else {
            m_recommendedCompositor = OpenGL2Compositing;
        }

        m_limitedNPOT = m_textureNPOT && m_chipClass < NV40;
        m_limitedGLSL = m_supportsGLSL && m_chipClass < G80;
    }

    if (isIntel()) {
        if (m_chipClass < I915)
            m_supportsGLSL = false;

        m_limitedGLSL = m_supportsGLSL && m_chipClass < I965;
        m_looseBinding = true;

        if (m_chipClass < I965) {
            m_recommendedCompositor = XRenderCompositing;
        } else {
            m_recommendedCompositor = OpenGL2Compositing;
        }
    }

    if (isMesaDriver() && platformInterface == EglPlatformInterface) {
        // According to the reference implementation in
        // mesa/demos/src/egl/opengles1/texture_from_pixmap
        // the mesa egl implementation does not require a strict binding (so far).
        m_looseBinding = true;
    }

    if (isSoftwareEmulation()) {
        // we recommend XRender
        m_recommendedCompositor = XRenderCompositing;
        if (m_driver < Driver_Llvmpipe) {
            // Software emulation does not provide GLSL
            m_limitedGLSL = m_supportsGLSL = false;
        } else {
            // llvmpipe does support GLSL
            m_limitedGLSL = false;
            m_supportsGLSL = true;
        }
    }

    if (m_chipClass == UnknownChipClass && m_driver == Driver_Unknown) {
        // we don't know the hardware. Let's be optimistic and assume OpenGL compatible hardware
        m_recommendedCompositor = OpenGL2Compositing;
        m_supportsGLSL = true;
    }

    if (isVirtualBox()) {
        m_virtualMachine = true;
    }

    if (isVMware()) {
        m_virtualMachine = true;
    }
}

static void print(const QString &label, const QString &setting)
{
    std::cout << std::setw(40) << std::left
              << qPrintable(label) << qPrintable(setting) << std::endl;
}

void GLPlatform::printResults() const
{
    print(QStringLiteral("OpenGL vendor string:"),   QString::fromUtf8(m_vendor));
    print(QStringLiteral("OpenGL renderer string:"), QString::fromUtf8(m_renderer));
    print(QStringLiteral("OpenGL version string:"),  QString::fromUtf8(m_version));

    if (m_supportsGLSL)
        print(QStringLiteral("OpenGL shading language version string:"), QString::fromUtf8(m_glsl_version));

    print(QStringLiteral("Driver:"), driverToString(m_driver));
    if (!isMesaDriver())
        print(QStringLiteral("Driver version:"), versionToString(m_driverVersion));

    print(QStringLiteral("GPU class:"), chipClassToString(m_chipClass));

    print(QStringLiteral("OpenGL version:"), versionToString(m_glVersion));

    if (m_supportsGLSL)
        print(QStringLiteral("GLSL version:"), versionToString(m_glslVersion));

    if (isMesaDriver())
        print(QStringLiteral("Mesa version:"), versionToString(mesaVersion()));
    //if (galliumVersion() > 0)
    //    print("Gallium version:", versionToString(m_galliumVersion));
    if (serverVersion() > 0)
        print(QStringLiteral("X server version:"), versionToString(m_serverVersion));
    if (kernelVersion() > 0)
        print(QStringLiteral("Linux kernel version:"), versionToString(m_kernelVersion));

    print(QStringLiteral("Direct rendering:"), m_directRendering ? QStringLiteral("yes") : QStringLiteral("no"));
    print(QStringLiteral("Requires strict binding:"), !m_looseBinding ? QStringLiteral("yes") : QStringLiteral("no"));
    print(QStringLiteral("GLSL shaders:"), m_supportsGLSL ? (m_limitedGLSL ? QStringLiteral("limited") : QStringLiteral("yes")) : QStringLiteral("no"));
    print(QStringLiteral("Texture NPOT support:"), m_textureNPOT ? (m_limitedNPOT ? QStringLiteral("limited") : QStringLiteral("yes")) : QStringLiteral("no"));
    print(QStringLiteral("Virtual Machine:"), m_virtualMachine ? QStringLiteral("yes") : QStringLiteral("no"));
}

bool GLPlatform::supports(GLFeature feature) const
{
    switch(feature) {
    case LooseBinding:
        return m_looseBinding;

    case GLSL:
        return m_supportsGLSL;

    case LimitedGLSL:
        return m_limitedGLSL;

    case TextureNPOT:
        return m_textureNPOT;

    case LimitedNPOT:
        return m_limitedNPOT;

    default:
        return false;
    }
}

qint64 GLPlatform::glVersion() const
{
    return m_glVersion;
}

qint64 GLPlatform::glslVersion() const
{
    return m_glslVersion;
}

qint64 GLPlatform::mesaVersion() const
{
    return m_mesaVersion;
}

qint64 GLPlatform::galliumVersion() const
{
    return m_galliumVersion;
}

qint64 GLPlatform::serverVersion() const
{
    return m_serverVersion;
}

qint64 GLPlatform::kernelVersion() const
{
    return m_kernelVersion;
}

qint64 GLPlatform::driverVersion() const
{
    if (isMesaDriver())
        return mesaVersion();

    return m_driverVersion;
}

Driver GLPlatform::driver() const
{
    return m_driver;
}

ChipClass GLPlatform::chipClass() const
{
    return m_chipClass;
}

bool GLPlatform::isMesaDriver() const
{
    return mesaVersion() > 0;
}

bool GLPlatform::isGalliumDriver() const
{
    return galliumVersion() > 0;
}

bool GLPlatform::isRadeon() const
{
    return m_chipClass >= R100 && m_chipClass <= UnknownRadeon;
}

bool GLPlatform::isNvidia() const
{
    return m_chipClass >= NV10 && m_chipClass <= UnknownNVidia;
}

bool GLPlatform::isIntel() const
{
    return m_chipClass >= I8XX && m_chipClass <= UnknownIntel;
}

bool GLPlatform::isVirtualBox() const
{
    return m_driver == Driver_VirtualBox;
}

bool GLPlatform::isVMware() const
{
    return m_driver == Driver_VMware;
}

bool GLPlatform::isSoftwareEmulation() const
{
    return m_driver == Driver_Softpipe || m_driver == Driver_Swrast || m_driver == Driver_Llvmpipe;
}

const QByteArray &GLPlatform::glRendererString() const
{
    return m_renderer;
}

const QByteArray &GLPlatform::glVendorString() const
{
    return m_vendor;
}

const QByteArray &GLPlatform::glVersionString() const
{
    return m_version;
}

const QByteArray &GLPlatform::glShadingLanguageVersionString() const
{
    return m_glsl_version;
}

bool GLPlatform::isDirectRendering() const
{
    return m_directRendering;
}

bool GLPlatform::isLooseBinding() const
{
    return m_looseBinding;
}

bool GLPlatform::isVirtualMachine() const
{
    return m_virtualMachine;
}

CompositingType GLPlatform::recommendedCompositor() const
{
    return m_recommendedCompositor;
}

bool GLPlatform::preferBufferSubData() const
{
    return m_preferBufferSubData;
}

bool GLPlatform::isGLES() const
{
#ifdef KWIN_HAVE_OPENGLES
    return true;
#else
    return false;
#endif
}

} // namespace KWin

