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

#include <kwinglobals.h>

#include <QRegExp>
#include <QStringList>
#include <QDebug>

#include <sys/utsname.h>

#ifdef KWIN_HAVE_OPENGL

namespace KWin
{

GLPlatform *GLPlatform::s_platform = 0;

static qint64 parseVersionString(const QByteArray &version)
{
    // Skip any leading non digit
    int start = 0;
    while (start < version.length() && !QChar(version[start]).isDigit())
        start++;

    // Strip any non digit, non '.' characters from the end
    int end = start;
    while (end < version.length() && (version[end] == '.' || QChar(version[end]).isDigit()))
        end++;

    const QByteArray result = version.mid(start, end);
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

    if (chipset.contains("R100")  ||
        chipset.contains("RV100") ||
        chipset.contains("RS100"))
        return R100;

    if (chipset.contains("RV200") ||
        chipset.contains("RS200") ||
        chipset.contains("R200")  ||
        chipset.contains("RV250") ||
        chipset.contains("RS300") ||
        chipset.contains("RV280"))
        return R200;

    if (chipset.contains("R300")  ||
        chipset.contains("R350")  ||
        chipset.contains("RV350") ||
        chipset.contains("RV380"))
        return R300;

    if (chipset.contains("R420")  ||
        chipset.contains("RV410") ||
        chipset.contains("RS400") ||
        chipset.contains("RS600") ||
        chipset.contains("RS690") ||
        chipset.contains("RS740"))
        return R400;

    if (chipset.contains("RV515") ||
        chipset.contains("R520")  ||
        chipset.contains("RV530") ||
        chipset.contains("R580")  ||
        chipset.contains("RV560") ||
        chipset.contains("RV570"))
        return R500;

    if (chipset.contains("R600")  ||
        chipset.contains("RV610") ||
        chipset.contains("RV630") ||
        chipset.contains("RV670") ||
        chipset.contains("RV620") ||
        chipset.contains("RV635") ||
        chipset.contains("RS780") ||
        chipset.contains("RS880"))
        return R600;

    if (chipset.contains("R700")  ||
        chipset.contains("RV770") ||
        chipset.contains("RV730") ||
        chipset.contains("RV710") ||
        chipset.contains("RV740"))
        return R700;

    if (chipset.contains("EVERGREEN") || // Not an actual chipset, but returned by R600G in 7.9
        chipset.contains("CEDAR")     ||
        chipset.contains("REDWOOD")   ||
        chipset.contains("JUNIPER")   ||
        chipset.contains("CYPRESS")   ||
        chipset.contains("HEMLOCK"))
        return Evergreen;

    int index = chipset.indexOf(QRegExp("HD [0-9]{4,4}")); // HD followed by a space and 4 digits
    if (index != -1) {
        switch (chipset[index + 3].digitValue()) {
        case 6: // HD 6xxx
            return NorthernIslands;
        case 5: // HD 5xxx
            return Evergreen;
        case 4: // HD 4xxx
            return R700;
        case 3: // HD 3xxx
        case 2: // HD 2xxx
            return R600;
        default:
            return UnknownRadeon;
        }
    }

    QString name = extract(chipset, "X[0-9]{3,4}"); // X followed by 3-4 digits
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

    name = extract(chipset, "\\b[0-9]{4,4}\\b"); // A group of 4 digits
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
    const QString name = extract(chipset, "\\bNV[0-9,A-C]{2,2}\\b"); // NV followed by two hexadecimal digits
    if (!name.isEmpty())
    {
        const int id = chipset.mid(2, -1).toInt(0, 16); // Strip the 'NV' from the id

        switch (id & 0xf0)
        {
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

    return UnknownNVidia;
}

static ChipClass detectIntelClass(const QByteArray &chipset)
{
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
        chipset.contains("Ironlake")   ||
        chipset.contains("Sandybridge"))  // GL 3.1, CL 1.1, DX 10.1
        return I965;

    return UnknownIntel;
}



// -------



GLPlatform::GLPlatform()
    : m_driver(Driver_Unknown),
      m_chipClass(UnknownChipClass),
      m_mesaVersion(0),
      m_galliumVersion(0),
      m_looseBinding(false),
      m_directRendering(false),
      m_supportsGLSL(false),
      m_limitedGLSL(false)
{
}

GLPlatform::~GLPlatform()
{
}

void GLPlatform::detect()
{
    m_vendor       = (const char*)glGetString(GL_VENDOR);
    m_renderer     = (const char*)glGetString(GL_RENDERER);
    m_version      = (const char*)glGetString(GL_VERSION);
    m_glsl_version = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);

    const QByteArray extensions = (const char*)glGetString(GL_EXTENSIONS);
    m_extensions = QSet<QByteArray>::fromList(extensions.split(' '));

    // Parse the OpenGL version
    const QList<QByteArray> versionTokens = m_version.split(' ');
    if (versionTokens.count() > 0) {
        const QByteArray version = versionTokens.at(0);
        m_glVersion = parseVersionString(version);
    }

    // Parse the Mesa version
    const int mesaIndex = versionTokens.indexOf("Mesa");
    if (mesaIndex != -1) {
        const QByteArray version = versionTokens.at(mesaIndex + 1);
        m_mesaVersion = parseVersionString(version);
    }

    // Parse the GLSL version
    const QList<QByteArray> glslVersionTokens = m_glsl_version.split(' ');
    if (glslVersionTokens.count() > 0) {
        const QByteArray version = glslVersionTokens.at(0);
        m_glslVersion = parseVersionString(version);
    }

    GLXContext ctx = glXGetCurrentContext();
    m_directRendering = glXIsDirect(display(), ctx);

    m_serverVersion = getXServerVersion();
    m_kernelVersion = getKernelVersion();

    m_supportsGLSL = m_extensions.contains("GL_ARB_shading_language_100") &&
                     m_extensions.contains("GL_ARB_shader_objects") &&
                     m_extensions.contains("GL_ARB_fragment_shader") &&
                     m_extensions.contains("GL_ARB_vertex_shader") &&
                     m_glslVersion >= kVersionNumber(1, 0);

    m_chipset = "Unknown";


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

        m_chipClass = detectRadeonClass(m_chipset);
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
        m_chipset = tokens.at(3) == "AMD" ? tokens.at(4) : tokens.at(3);

        // R300G
        if (m_vendor == "X.Org R300 Project") {
            m_chipClass = detectRadeonClass(m_chipset);
            m_driver = Driver_R300G;
        }

        // R600G
        else if (m_vendor == "X.Org" &&
                 (m_renderer.contains("R6")      ||
                  m_renderer.contains("R7")      ||
                  m_renderer.contains("RV6")     ||
                  m_renderer.contains("RV7")     ||
                  m_renderer.contains("RS780")   ||
                  m_renderer.contains("RS880")   ||
                  m_renderer.contains("CEDAR")   ||
                  m_renderer.contains("REDWOOD") ||
                  m_renderer.contains("JUNIPER") ||
                  m_renderer.contains("CYPRESS") ||
                  m_renderer.contains("HEMLOCK") ||
                  m_renderer.contains("EVERGREEN")))
        {
            m_chipClass = detectRadeonClass(m_chipset);
            m_driver = Driver_R600G;
        }

        // Nouveau
        else if (m_vendor == "nouveau") {
            m_chipClass = detectNVidiaClass(m_chipset);
            m_driver = Driver_Nouveau;
        }

        else if (m_chipset == "softpipe" || m_chipset == "llvmpipe") {
            // Vendor: VMware, Inc.
            // TODO
        }
    } 


    // Properietary drivers
    // ====================================================
    else if (m_vendor == "ATI Technologies Inc.") {
        m_chipClass = detectRadeonClass(m_renderer);
        m_driver = Driver_Catalyst;

        if (versionTokens.count() > 1 && versionTokens.at(2)[0] == '(')
            m_driverVersion = parseVersionString(versionTokens.at(1));
        else if (versionTokens.count() > 0)
            m_driverVersion = parseVersionString(versionTokens.at(0));
        else
            m_driverVersion = 0;
    }

    else if (m_vendor == "NVIDIA Corporation") {
        m_chipClass = UnknownNVidia;
        m_driver = Driver_NVidia;
    
        int index = versionTokens.indexOf("NVIDIA");
        if (versionTokens.count() > index)
            m_driverVersion = parseVersionString(versionTokens.at(index + 1));
        else
            m_driverVersion = 0;

        // TODO detect the chip class
    }


    // Driver/GPU specific features
    // ====================================================
    if (isRadeon()) {
        // R200 technically has a programmable pipeline, but since it's SM 1.4,
        // it's too limited to to be of any practical value to us.
        if (m_chipClass < R300)
            m_supportsGLSL = false;

        m_limitedGLSL = m_supportsGLSL && m_chipClass < R600;

        if (driver() == Driver_R600G ||
            (driver() == Driver_R600C && m_renderer.contains("DRI2")))
        {
            m_looseBinding = true;
        }
    }

    if (isNvidia()) {
        if (m_driver == Driver_NVidia && m_chipClass < NV40)
            m_supportsGLSL = false; // High likelihood of software emulation

        if (m_driver == Driver_NVidia)
            m_looseBinding = true;

        m_limitedGLSL = m_supportsGLSL && m_chipClass < G80;
    } 

    if (isIntel()) {
        if (m_chipClass < I915)
            m_supportsGLSL = false;

        m_limitedGLSL = m_supportsGLSL && m_chipClass < I965;
    }
}

bool GLPlatform::supports(GLFeature feature) const
{
    switch (feature) {
    case LooseBinding:
        return m_looseBinding;

    case GLSL:
        return m_supportsGLSL;

    case LimitedGLSL:
        return m_limitedGLSL;

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
    return m_glVersion;
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

} // namespace KWin

#endif // KWIN_HAVE_OPENGL

