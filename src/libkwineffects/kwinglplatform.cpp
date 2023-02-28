/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwinglplatform.h"
// include kwinglutils_funcs.h to avoid the redeclaration issues
// between qopengl.h and epoxy/gl.h
#include "kwinglutils_funcs.h"
#include <epoxy/gl.h>

#include <QDebug>
#include <QOpenGLContext>
#include <QRegularExpression>
#include <QStringList>

#include <sys/utsname.h>

#include <iomanip>
#include <ios>
#include <iostream>

namespace KWin
{

std::unique_ptr<GLPlatform> GLPlatform::s_platform;

static qint64 parseVersionString(const QByteArray &version)
{
    // Skip any leading non digit
    int start = 0;
    while (start < version.length() && !QChar::fromLatin1(version[start]).isDigit()) {
        start++;
    }

    // Strip any non digit, non '.' characters from the end
    int end = start;
    while (end < version.length() && (version[end] == '.' || QChar::fromLatin1(version[end]).isDigit())) {
        end++;
    }

    const QByteArray result = version.mid(start, end - start);
    const QList<QByteArray> tokens = result.split('.');
    const qint64 major = tokens.at(0).toInt();
    const qint64 minor = tokens.count() > 1 ? tokens.at(1).toInt() : 0;
    const qint64 patch = tokens.count() > 2 ? tokens.at(2).toInt() : 0;

    return kVersionNumber(major, minor, patch);
}

static qint64 getXServerVersion()
{
    qint64 major, minor, patch;
    major = 0;
    minor = 0;
    patch = 0;

    if (xcb_connection_t *c = connection()) {
        auto setup = xcb_get_setup(c);
        const QByteArray vendorName(xcb_setup_vendor(setup), xcb_setup_vendor_length(setup));
        if (vendorName.contains("X.Org")) {
            const int release = setup->release_number;
            major = (release / 10000000);
            minor = (release / 100000) % 100;
            patch = (release / 1000) % 100;
        }
    }

    return kVersionNumber(major, minor, patch);
}

static qint64 getKernelVersion()
{
    struct utsname name;
    uname(&name);

    if (qstrcmp(name.sysname, "Linux") == 0) {
        return parseVersionString(name.release);
    }

    return 0;
}

// Extracts the portion of a string that matches a regular expression
static QString extract(const QString &text, const QString &pattern)
{
    const QRegularExpression regexp(pattern);
    const QRegularExpressionMatch match = regexp.match(text);
    if (!match.hasMatch()) {
        return QString();
    }
    return match.captured();
}

static ChipClass detectRadeonClass(const QByteArray &chipset)
{
    if (chipset.isEmpty()) {
        return UnknownRadeon;
    }

    if (chipset.contains("R100")
        || chipset.contains("RV100")
        || chipset.contains("RS100")) {
        return R100;
    }

    if (chipset.contains("RV200")
        || chipset.contains("RS200")
        || chipset.contains("R200")
        || chipset.contains("RV250")
        || chipset.contains("RS300")
        || chipset.contains("RV280")) {
        return R200;
    }

    if (chipset.contains("R300")
        || chipset.contains("R350")
        || chipset.contains("R360")
        || chipset.contains("RV350")
        || chipset.contains("RV370")
        || chipset.contains("RV380")) {
        return R300;
    }

    if (chipset.contains("R420")
        || chipset.contains("R423")
        || chipset.contains("R430")
        || chipset.contains("R480")
        || chipset.contains("R481")
        || chipset.contains("RV410")
        || chipset.contains("RS400")
        || chipset.contains("RC410")
        || chipset.contains("RS480")
        || chipset.contains("RS482")
        || chipset.contains("RS600")
        || chipset.contains("RS690")
        || chipset.contains("RS740")) {
        return R400;
    }

    if (chipset.contains("RV515")
        || chipset.contains("R520")
        || chipset.contains("RV530")
        || chipset.contains("R580")
        || chipset.contains("RV560")
        || chipset.contains("RV570")) {
        return R500;
    }

    if (chipset.contains("R600")
        || chipset.contains("RV610")
        || chipset.contains("RV630")
        || chipset.contains("RV670")
        || chipset.contains("RV620")
        || chipset.contains("RV635")
        || chipset.contains("RS780")
        || chipset.contains("RS880")) {
        return R600;
    }

    if (chipset.contains("R700")
        || chipset.contains("RV770")
        || chipset.contains("RV730")
        || chipset.contains("RV710")
        || chipset.contains("RV740")) {
        return R700;
    }

    if (chipset.contains("EVERGREEN") // Not an actual chipset, but returned by R600G in 7.9
        || chipset.contains("CEDAR")
        || chipset.contains("REDWOOD")
        || chipset.contains("JUNIPER")
        || chipset.contains("CYPRESS")
        || chipset.contains("HEMLOCK")
        || chipset.contains("PALM")) {
        return Evergreen;
    }

    if (chipset.contains("SUMO")
        || chipset.contains("SUMO2")
        || chipset.contains("BARTS")
        || chipset.contains("TURKS")
        || chipset.contains("CAICOS")
        || chipset.contains("CAYMAN")) {
        return NorthernIslands;
    }

    if (chipset.contains("TAHITI")
        || chipset.contains("PITCAIRN")
        || chipset.contains("VERDE")
        || chipset.contains("OLAND")
        || chipset.contains("HAINAN")) {
        return SouthernIslands;
    }

    if (chipset.contains("BONAIRE")
        || chipset.contains("KAVERI")
        || chipset.contains("KABINI")
        || chipset.contains("HAWAII")
        || chipset.contains("MULLINS")) {
        return SeaIslands;
    }

    if (chipset.contains("TONGA")
        || chipset.contains("TOPAZ")
        || chipset.contains("FIJI")
        || chipset.contains("CARRIZO")
        || chipset.contains("STONEY")) {
        return VolcanicIslands;
    }

    if (chipset.contains("POLARIS10")
        || chipset.contains("POLARIS11")
        || chipset.contains("POLARIS12")
        || chipset.contains("VEGAM")) {
        return ArcticIslands;
    }

    if (chipset.contains("VEGA10")
        || chipset.contains("VEGA12")
        || chipset.contains("VEGA20")
        || chipset.contains("RAVEN")
        || chipset.contains("RAVEN2")
        || chipset.contains("RENOIR")
        || chipset.contains("ARCTURUS")) {
        return Vega;
    }

    if (chipset.contains("NAVI10")
        || chipset.contains("NAVI12")
        || chipset.contains("NAVI14")) {
        return Navi;
    }

    const QString chipset16 = QString::fromLatin1(chipset);
    QString name = extract(chipset16, QStringLiteral("HD [0-9]{4}")); // HD followed by a space and 4 digits
    if (!name.isEmpty()) {
        const int id = QStringView(name).right(4).toInt();
        if (id == 6250 || id == 6310) { // Palm
            return Evergreen;
        }

        if (id >= 6000 && id < 7000) {
            return NorthernIslands; // HD 6xxx
        }

        if (id >= 5000 && id < 6000) {
            return Evergreen; // HD 5xxx
        }

        if (id >= 4000 && id < 5000) {
            return R700; // HD 4xxx
        }

        if (id >= 2000 && id < 4000) { // HD 2xxx/3xxx
            return R600;
        }

        return UnknownRadeon;
    }

    name = extract(chipset16, QStringLiteral("X[0-9]{3,4}")); // X followed by 3-4 digits
    if (!name.isEmpty()) {
        const int id = QStringView(name).mid(1, -1).toInt();

        // X1xxx
        if (id >= 1300) {
            return R500;
        }

        // X7xx, X8xx, X12xx, 2100
        if ((id >= 700 && id < 1000) || id >= 1200) {
            return R400;
        }

        // X200, X3xx, X5xx, X6xx, X10xx, X11xx
        if ((id >= 300 && id < 700) || (id >= 1000 && id < 1200)) {
            return R300;
        }

        return UnknownRadeon;
    }

    name = extract(chipset16, QStringLiteral("\\b[0-9]{4}\\b")); // A group of 4 digits
    if (!name.isEmpty()) {
        const int id = name.toInt();

        // 7xxx
        if (id >= 7000 && id < 8000) {
            return R100;
        }

        // 8xxx, 9xxx
        if (id >= 8000 && id < 9500) {
            return R200;
        }

        // 9xxx
        if (id >= 9500) {
            return R300;
        }

        if (id == 2100) {
            return R400;
        }
    }

    return UnknownRadeon;
}

static ChipClass detectNVidiaClass(const QString &chipset)
{
    QString name = extract(chipset, QStringLiteral("\\bNV[0-9,A-F]{2}\\b")); // NV followed by two hexadecimal digits
    if (!name.isEmpty()) {
        const int id = QStringView(chipset).mid(2, -1).toInt(nullptr, 16); // Strip the 'NV' from the id

        switch (id & 0xf0) {
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

    if (chipset.contains(QLatin1String("GeForce2")) || chipset.contains(QLatin1String("GeForce 256"))) {
        return NV10;
    }

    if (chipset.contains(QLatin1String("GeForce3"))) {
        return NV20;
    }

    if (chipset.contains(QLatin1String("GeForce4"))) {
        if (chipset.contains(QLatin1String("MX 420"))
            || chipset.contains(QLatin1String("MX 440")) // including MX 440SE
            || chipset.contains(QLatin1String("MX 460"))
            || chipset.contains(QLatin1String("MX 4000"))
            || chipset.contains(QLatin1String("PCX 4300"))) {
            return NV10;
        }

        return NV20;
    }

    // GeForce 5,6,7,8,9
    name = extract(chipset, QStringLiteral("GeForce (FX |PCX |Go )?\\d{4}(M|\\b)")).trimmed();
    if (!name.isEmpty()) {
        if (!name[name.length() - 1].isDigit()) {
            name.chop(1);
        }

        const int id = QStringView(name).right(4).toInt();
        if (id < 6000) {
            return NV30;
        }

        if (id >= 6000 && id < 8000) {
            return NV40;
        }

        if (id >= 8000) {
            return G80;
        }

        return UnknownNVidia;
    }

    // GeForce 100/200/300/400/500
    name = extract(chipset, QStringLiteral("GeForce (G |GT |GTX |GTS )?\\d{3}(M|\\b)")).trimmed();
    if (!name.isEmpty()) {
        if (!name[name.length() - 1].isDigit()) {
            name.chop(1);
        }

        const int id = QStringView(name).right(3).toInt();
        if (id >= 100 && id < 600) {
            if (id >= 400) {
                return GF100;
            }

            return G80;
        }
        return UnknownNVidia;
    }

    return UnknownNVidia;
}
static inline ChipClass detectNVidiaClass(const QByteArray &chipset)
{
    return detectNVidiaClass(QString::fromLatin1(chipset));
}

static ChipClass detectIntelClass(const QByteArray &chipset)
{
    // see mesa repository: src/mesa/drivers/dri/intel/intel_context.c
    // GL 1.3, DX8? SM ?
    if (chipset.contains("845G")
        || chipset.contains("830M")
        || chipset.contains("852GM/855GM")
        || chipset.contains("865G")) {
        return I8XX;
    }

    // GL 1.4, DX 9.0, SM 2.0
    if (chipset.contains("915G")
        || chipset.contains("E7221G")
        || chipset.contains("915GM")
        || chipset.contains("945G") // DX 9.0c
        || chipset.contains("945GM")
        || chipset.contains("945GME")
        || chipset.contains("Q33") // GL1.5
        || chipset.contains("Q35")
        || chipset.contains("G33")
        || chipset.contains("965Q") // GMA 3000, but apparently considered gen 4 by the driver
        || chipset.contains("946GZ") // GMA 3000, but apparently considered gen 4 by the driver
        || chipset.contains("IGD")) {
        return I915;
    }

    // GL 2.0, DX 9.0c, SM 3.0
    if (chipset.contains("965G")
        || chipset.contains("G45/G43") // SM 4.0
        || chipset.contains("965GM") // GL 2.1
        || chipset.contains("965GME/GLE")
        || chipset.contains("GM45")
        || chipset.contains("Q45/Q43")
        || chipset.contains("G41")
        || chipset.contains("B43")
        || chipset.contains("Ironlake")) {
        return I965;
    }

    // GL 3.1, CL 1.1, DX 10.1
    if (chipset.contains("Sandybridge") || chipset.contains("SNB GT")) {
        return SandyBridge;
    }

    // GL4.0, CL1.1, DX11, SM 5.0
    if (chipset.contains("Ivybridge") || chipset.contains("IVB GT")) {
        return IvyBridge;
    }

    // GL4.0, CL1.2, DX11.1, SM 5.0
    if (chipset.contains("Haswell") || chipset.contains("HSW GT")) {
        return Haswell;
    }
    if (chipset.contains("BYT")) {
        return BayTrail;
    }
    if (chipset.contains("CHV") || chipset.contains("BSW")) {
        return Cherryview;
    }
    if (chipset.contains("BDW GT")) {
        return Broadwell;
    }
    if (chipset.contains("SKL GT")) {
        return Skylake;
    }
    if (chipset.contains("APL")) {
        return ApolloLake;
    }
    if (chipset.contains("KBL GT")) {
        return KabyLake;
    }
    if (chipset.contains("WHL GT")) {
        return WhiskeyLake;
    }
    if (chipset.contains("CML GT")) {
        return CometLake;
    }
    if (chipset.contains("CNL GT")) {
        return CannonLake;
    }
    if (chipset.contains("CFL GT")) {
        return CoffeeLake;
    }
    if (chipset.contains("ICL GT")) {
        return IceLake;
    }
    if (chipset.contains("TGL GT")) {
        return TigerLake;
    }

    return UnknownIntel;
}

static ChipClass detectQualcommClass(const QByteArray &chipClass)
{
    if (!chipClass.contains("Adreno")) {
        return UnknownChipClass;
    }
    const auto parts = chipClass.split(' ');
    if (parts.count() < 3) {
        return UnknownAdreno;
    }
    bool ok = false;
    const int value = parts.at(2).toInt(&ok);
    if (ok) {
        if (value >= 100 && value < 200) {
            return Adreno1XX;
        } else if (value >= 200 && value < 300) {
            return Adreno2XX;
        } else if (value >= 300 && value < 400) {
            return Adreno3XX;
        } else if (value >= 400 && value < 500) {
            return Adreno4XX;
        } else if (value >= 500 && value < 600) {
            return Adreno5XX;
        }
    }
    return UnknownAdreno;
}

static ChipClass detectPanfrostClass(const QByteArray &chipClass)
{
// Keep the list of supported Mali chipset up to date with https://docs.mesa3d.org/drivers/panfrost.html
    if (chipClass.contains("T720") || chipClass.contains("T760")) {
        return MaliT7XX;
    }

    if (chipClass.contains("T820") || chipClass.contains("T830") || chipClass.contains("T860") || chipClass.contains("T880")) {
        return MaliT8XX;
    }

    if (chipClass.contains("G31") || chipClass.contains("G51") || chipClass.contains("G52") || chipClass.contains("G57") || chipClass.contains("G72") || chipClass.contains("G76")) {
        return MaliGXX;
    }

    return UnknownPanfrost;
}

static ChipClass detectLimaClass(const QByteArray &chipClass)
{
    if (chipClass.contains("400")) {
        return Mali400;
    } else if (chipClass.contains("450")) {
        return Mali450;
    } else if (chipClass.contains("470")) {
        return Mali470;
    }

    return UnknownLima;
}

static ChipClass detectVC4Class(const QByteArray &chipClass)
{
    if (chipClass.contains("2.1")) {
        return VC4_2_1;
    }

    return UnknownVideoCore4;
}

static ChipClass detectV3DClass(const QByteArray &chipClass)
{
    if (chipClass.contains("4.2")) {
        return V3D_4_2;
    }

    return UnknownVideoCore3D;
}

QString GLPlatform::versionToString(qint64 version)
{
    return QString::fromLatin1(versionToString8(version));
}
QByteArray GLPlatform::versionToString8(qint64 version)
{
    int major = (version >> 32);
    int minor = (version >> 16) & 0xffff;
    int patch = version & 0xffff;

    QByteArray string = QByteArray::number(major) + '.' + QByteArray::number(minor);
    if (patch != 0) {
        string += '.' + QByteArray::number(patch);
    }

    return string;
}

QString GLPlatform::driverToString(Driver driver)
{
    return QString::fromLatin1(driverToString8(driver));
}
QByteArray GLPlatform::driverToString8(Driver driver)
{
    switch (driver) {
    case Driver_R100:
        return QByteArrayLiteral("Radeon");
    case Driver_R200:
        return QByteArrayLiteral("R200");
    case Driver_R300C:
        return QByteArrayLiteral("R300C");
    case Driver_R300G:
        return QByteArrayLiteral("R300G");
    case Driver_R600C:
        return QByteArrayLiteral("R600C");
    case Driver_R600G:
        return QByteArrayLiteral("R600G");
    case Driver_RadeonSI:
        return QByteArrayLiteral("RadeonSI");
    case Driver_Nouveau:
        return QByteArrayLiteral("Nouveau");
    case Driver_Intel:
        return QByteArrayLiteral("Intel");
    case Driver_NVidia:
        return QByteArrayLiteral("NVIDIA");
    case Driver_Catalyst:
        return QByteArrayLiteral("Catalyst");
    case Driver_Swrast:
        return QByteArrayLiteral("Software rasterizer");
    case Driver_Softpipe:
        return QByteArrayLiteral("softpipe");
    case Driver_Llvmpipe:
        return QByteArrayLiteral("LLVMpipe");
    case Driver_VirtualBox:
        return QByteArrayLiteral("VirtualBox (Chromium)");
    case Driver_VMware:
        return QByteArrayLiteral("VMware (SVGA3D)");
    case Driver_Qualcomm:
        return QByteArrayLiteral("Qualcomm");
    case Driver_Virgl:
        return QByteArrayLiteral("Virgl (virtio-gpu, Qemu/KVM guest)");
    case Driver_Panfrost:
        return QByteArrayLiteral("Panfrost");
    case Driver_Lima:
        return QByteArrayLiteral("Mali (Lima)");
    case Driver_VC4:
        return QByteArrayLiteral("VideoCore IV");
    case Driver_V3D:
        return QByteArrayLiteral("VideoCore 3D");

    default:
        return QByteArrayLiteral("Unknown");
    }
}

QString GLPlatform::chipClassToString(ChipClass chipClass)
{
    return QString::fromLatin1(chipClassToString8(chipClass));
}
QByteArray GLPlatform::chipClassToString8(ChipClass chipClass)
{
    switch (chipClass) {
    case R100:
        return QByteArrayLiteral("R100");
    case R200:
        return QByteArrayLiteral("R200");
    case R300:
        return QByteArrayLiteral("R300");
    case R400:
        return QByteArrayLiteral("R400");
    case R500:
        return QByteArrayLiteral("R500");
    case R600:
        return QByteArrayLiteral("R600");
    case R700:
        return QByteArrayLiteral("R700");
    case Evergreen:
        return QByteArrayLiteral("EVERGREEN");
    case NorthernIslands:
        return QByteArrayLiteral("Northern Islands");
    case SouthernIslands:
        return QByteArrayLiteral("Southern Islands");
    case SeaIslands:
        return QByteArrayLiteral("Sea Islands");
    case VolcanicIslands:
        return QByteArrayLiteral("Volcanic Islands");
    case ArcticIslands:
        return QByteArrayLiteral("Arctic Islands");
    case Vega:
        return QByteArrayLiteral("Vega");
    case Navi:
        return QByteArrayLiteral("Navi");

    case NV10:
        return QByteArrayLiteral("NV10");
    case NV20:
        return QByteArrayLiteral("NV20");
    case NV30:
        return QByteArrayLiteral("NV30");
    case NV40:
        return QByteArrayLiteral("NV40/G70");
    case G80:
        return QByteArrayLiteral("G80/G90");
    case GF100:
        return QByteArrayLiteral("GF100");

    case I8XX:
        return QByteArrayLiteral("i830/i835");
    case I915:
        return QByteArrayLiteral("i915/i945");
    case I965:
        return QByteArrayLiteral("i965");
    case SandyBridge:
        return QByteArrayLiteral("SandyBridge");
    case IvyBridge:
        return QByteArrayLiteral("IvyBridge");
    case Haswell:
        return QByteArrayLiteral("Haswell");
    case BayTrail:
        return QByteArrayLiteral("Bay Trail");
    case Cherryview:
        return QByteArrayLiteral("Cherryview");
    case Broadwell:
        return QByteArrayLiteral("Broadwell");
    case ApolloLake:
        return QByteArrayLiteral("Apollo Lake");
    case Skylake:
        return QByteArrayLiteral("Skylake");
    case GeminiLake:
        return QByteArrayLiteral("Gemini Lake");
    case KabyLake:
        return QByteArrayLiteral("Kaby Lake");
    case CoffeeLake:
        return QByteArrayLiteral("Coffee Lake");
    case WhiskeyLake:
        return QByteArrayLiteral("Whiskey Lake");
    case CometLake:
        return QByteArrayLiteral("Comet Lake");
    case CannonLake:
        return QByteArrayLiteral("Cannon Lake");
    case IceLake:
        return QByteArrayLiteral("Ice Lake");
    case TigerLake:
        return QByteArrayLiteral("Tiger Lake");

    case Adreno1XX:
        return QByteArrayLiteral("Adreno 1xx series");
    case Adreno2XX:
        return QByteArrayLiteral("Adreno 2xx series");
    case Adreno3XX:
        return QByteArrayLiteral("Adreno 3xx series");
    case Adreno4XX:
        return QByteArrayLiteral("Adreno 4xx series");
    case Adreno5XX:
        return QByteArrayLiteral("Adreno 5xx series");

    case Mali400:
        return QByteArrayLiteral("Mali 400 series");
    case Mali450:
        return QByteArrayLiteral("Mali 450 series");
    case Mali470:
        return QByteArrayLiteral("Mali 470 series");

    case MaliT7XX:
        return QByteArrayLiteral("Mali T7xx series");
    case MaliT8XX:
        return QByteArrayLiteral("Mali T8xx series");
    case MaliGXX:
        return QByteArrayLiteral("Mali Gxx series");

    case VC4_2_1:
        return QByteArrayLiteral("VideoCore IV");
    case V3D_4_2:
        return QByteArrayLiteral("VideoCore 3D");

    default:
        return QByteArrayLiteral("Unknown");
    }
}

// -------

GLPlatform::GLPlatform()
    : m_driver(Driver_Unknown)
    , m_chipClass(UnknownChipClass)
    , m_recommendedCompositor(QPainterCompositing)
    , m_glVersion(0)
    , m_glslVersion(0)
    , m_mesaVersion(0)
    , m_driverVersion(0)
    , m_galliumVersion(0)
    , m_serverVersion(0)
    , m_kernelVersion(0)
    , m_looseBinding(false)
    , m_supportsGLSL(false)
    , m_limitedGLSL(false)
    , m_textureNPOT(false)
    , m_limitedNPOT(false)
    , m_packInvert(false)
    , m_virtualMachine(false)
    , m_preferBufferSubData(false)
    , m_platformInterface(NoOpenGLPlatformInterface)
    , m_gles(false)
{
}

GLPlatform::~GLPlatform()
{
}

void GLPlatform::detect(OpenGLPlatformInterface platformInterface)
{
    m_platformInterface = platformInterface;

    m_vendor = (const char *)glGetString(GL_VENDOR);
    m_renderer = (const char *)glGetString(GL_RENDERER);
    m_version = (const char *)glGetString(GL_VERSION);

    // Parse the OpenGL version
    const QList<QByteArray> versionTokens = m_version.split(' ');
    if (versionTokens.count() > 0) {
        const QByteArray version = QByteArray(m_version);
        m_glVersion = parseVersionString(version);
        if (platformInterface == EglPlatformInterface) {
            // only EGL can have OpenGLES, GLX is OpenGL only
            if (version.startsWith("OpenGL ES")) {
                // from GLES 2: "Returns a version or release number of the form OpenGL<space>ES<space><version number><space><vendor-specific information>."
                // from GLES 3: "Returns a version or release number." and "The version number uses one of these forms: major_number.minor_number major_number.minor_number.release_number"
                m_gles = true;
            }
        }
    }

    if (!isGLES() && m_glVersion >= kVersionNumber(3, 0)) {
        int count;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);

        for (int i = 0; i < count; i++) {
            const char *name = (const char *)glGetStringi(GL_EXTENSIONS, i);
            m_extensions.insert(name);
        }
    } else {
        const QByteArray extensions = (const char *)glGetString(GL_EXTENSIONS);
        QList<QByteArray> extensionsList = extensions.split(' ');
        m_extensions = {extensionsList.constBegin(), extensionsList.constEnd()};
    }

    // Parse the Mesa version
    const int mesaIndex = versionTokens.indexOf("Mesa");
    if (mesaIndex != -1) {
        const QByteArray &version = versionTokens.at(mesaIndex + 1);
        m_mesaVersion = parseVersionString(version);
    }

    if (isGLES()) {
        m_supportsGLSL = true;
        m_textureNPOT = true;
    } else {
        m_supportsGLSL = (m_extensions.contains("GL_ARB_shader_objects")
                          && m_extensions.contains("GL_ARB_fragment_shader")
                          && m_extensions.contains("GL_ARB_vertex_shader"));

        m_textureNPOT = m_extensions.contains("GL_ARB_texture_non_power_of_two");
    }

    m_serverVersion = getXServerVersion();
    m_kernelVersion = getKernelVersion();

    m_glslVersion = 0;
    m_glsl_version.clear();

    if (m_supportsGLSL) {
        // Parse the GLSL version
        m_glsl_version = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
        m_glslVersion = parseVersionString(m_glsl_version);
    }

    m_chipset = QByteArrayLiteral("Unknown");
    m_preferBufferSubData = false;
    m_packInvert = m_extensions.contains("GL_MESA_pack_invert");

    // Mesa classic drivers
    // ====================================================

    // Radeon
    if (m_renderer.startsWith("Mesa DRI R")) {
        // Sample renderer string: Mesa DRI R600 (RV740 94B3) 20090101 x86/MMX/SSE2 TCL DRI2
        const QList<QByteArray> tokens = m_renderer.split(' ');
        const QByteArray &chipClass = tokens.at(2);
        m_chipset = tokens.at(3).mid(1, -1); // Strip the leading '('

        if (chipClass == "R100") {
            // Vendor: Tungsten Graphics, Inc.
            m_driver = Driver_R100;

        } else if (chipClass == "R200") {
            // Vendor: Tungsten Graphics, Inc.
            m_driver = Driver_R200;

        } else if (chipClass == "R300") {
            // Vendor: DRI R300 Project
            m_driver = Driver_R300C;

        } else if (chipClass == "R600") {
            // Vendor: Advanced Micro Devices, Inc.
            m_driver = Driver_R600C;
        }

        m_chipClass = detectRadeonClass(m_chipset);
    }

    // Intel
    else if (m_renderer.contains("Intel")) {
        // Vendor: Tungsten Graphics, Inc.
        // Sample renderer string: Mesa DRI Mobile Intel® GM45 Express Chipset GEM 20100328 2010Q1

        QByteArray chipset;
        if (m_renderer.startsWith("Intel(R) Integrated Graphics Device")) {
            chipset = "IGD";
        } else {
            chipset = m_renderer;
        }

        m_driver = Driver_Intel;
        m_chipClass = detectIntelClass(chipset);
    }

    // Properietary drivers
    // ====================================================
    else if (m_vendor == "ATI Technologies Inc.") {
        m_chipClass = detectRadeonClass(m_renderer);
        m_driver = Driver_Catalyst;

        if (versionTokens.count() > 1 && versionTokens.at(2)[0] == '(') {
            m_driverVersion = parseVersionString(versionTokens.at(1));
        } else if (versionTokens.count() > 0) {
            m_driverVersion = parseVersionString(versionTokens.at(0));
        } else {
            m_driverVersion = 0;
        }
    }

    else if (m_vendor == "NVIDIA Corporation") {
        m_chipClass = detectNVidiaClass(m_renderer);
        m_driver = Driver_NVidia;

        int index = versionTokens.indexOf("NVIDIA");
        if (versionTokens.count() > index) {
            m_driverVersion = parseVersionString(versionTokens.at(index + 1));
        } else {
            m_driverVersion = 0;
        }
    }

    else if (m_vendor == "Qualcomm") {
        m_driver = Driver_Qualcomm;
        m_chipClass = detectQualcommClass(m_renderer);
    }

    else if (m_renderer.contains("Panfrost")) {
        m_driver = Driver_Panfrost;
        m_chipClass = detectPanfrostClass(m_renderer);
    }

    else if (m_renderer.contains("Mali")) {
        m_driver = Driver_Lima;
        m_chipClass = detectLimaClass(m_renderer);
    }

    else if (m_renderer.startsWith("VC4 ")) {
        m_driver = Driver_VC4;
        m_chipClass = detectVC4Class(m_renderer);
    }

    else if(m_renderer.startsWith("V3D ")) {
        m_driver = Driver_V3D;
        m_chipClass = detectV3DClass(m_renderer);
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
        if (versionTokens.count() > index) {
            m_driverVersion = parseVersionString(versionTokens.at(index + 1));
        } else {
            m_driverVersion = 0;
        }
    }

    // Gallium drivers
    // ====================================================
    else {
        const QList<QByteArray> tokens = m_renderer.split(' ');
        if (m_renderer.contains("Gallium")) {
            // Sample renderer string: Gallium 0.4 on AMD RV740
            m_galliumVersion = parseVersionString(tokens.at(1));
            m_chipset = (tokens.at(3) == "AMD" || tokens.at(3) == "ATI") ? tokens.at(4) : tokens.at(3);
        } else {
            // The renderer string does not contain "Gallium" anymore.
            m_chipset = tokens.at(0);
            // We don't know the actual version anymore, but it's at least 0.4.
            m_galliumVersion = kVersionNumber(0, 4, 0);
        }

        // R300G
        if (m_vendor == QByteArrayLiteral("X.Org R300 Project")) {
            m_chipClass = detectRadeonClass(m_chipset);
            m_driver = Driver_R300G;
        }

        // R600G
        else if (m_vendor == "X.Org" && (m_renderer.contains("R6") || m_renderer.contains("R7") || m_renderer.contains("RV6") || m_renderer.contains("RV7") || m_renderer.contains("RS780") || m_renderer.contains("RS880") || m_renderer.contains("CEDAR") || m_renderer.contains("REDWOOD") || m_renderer.contains("JUNIPER") || m_renderer.contains("CYPRESS") || m_renderer.contains("HEMLOCK") || m_renderer.contains("PALM") || m_renderer.contains("EVERGREEN") || m_renderer.contains("SUMO") || m_renderer.contains("SUMO2") || m_renderer.contains("BARTS") || m_renderer.contains("TURKS") || m_renderer.contains("CAICOS") || m_renderer.contains("CAYMAN"))) {
            m_chipClass = detectRadeonClass(m_chipset);
            m_driver = Driver_R600G;
        }

        // RadeonSI
        else if ((m_vendor == "X.Org" || m_vendor == "AMD") && (m_renderer.contains("TAHITI") || m_renderer.contains("PITCAIRN") || m_renderer.contains("VERDE") || m_renderer.contains("OLAND") || m_renderer.contains("HAINAN") || m_renderer.contains("BONAIRE") || m_renderer.contains("KAVERI") || m_renderer.contains("KABINI") || m_renderer.contains("HAWAII") || m_renderer.contains("MULLINS") || m_renderer.contains("TOPAZ") || m_renderer.contains("TONGA") || m_renderer.contains("FIJI") || m_renderer.contains("CARRIZO") || m_renderer.contains("STONEY") || m_renderer.contains("POLARIS10") || m_renderer.contains("POLARIS11") || m_renderer.contains("POLARIS12") || m_renderer.contains("VEGAM") || m_renderer.contains("VEGA10") || m_renderer.contains("VEGA12") || m_renderer.contains("VEGA20") || m_renderer.contains("RAVEN") || m_renderer.contains("RAVEN2") || m_renderer.contains("RENOIR") || m_renderer.contains("ARCTURUS") || m_renderer.contains("NAVI10") || m_renderer.contains("NAVI12") || m_renderer.contains("NAVI14"))) {
            m_chipClass = detectRadeonClass(m_renderer);
            m_driver = Driver_RadeonSI;
        }

        // Nouveau
        else if (m_vendor == "nouveau") {
            m_chipClass = detectNVidiaClass(m_chipset);
            m_driver = Driver_Nouveau;
        }

        // softpipe
        else if (m_chipset == "softpipe") {
            m_driver = Driver_Softpipe;
        }

        // llvmpipe
        else if (m_chipset == "llvmpipe") {
            m_driver = Driver_Llvmpipe;
        }

        // SVGA3D
        else if (m_vendor == "VMware, Inc." && m_chipset.contains("SVGA3D")) {
            m_driver = Driver_VMware;
        }

        // virgl
        else if (m_renderer == "virgl") {
            m_driver = Driver_Virgl;
        }
    }

    // Driver/GPU specific features
    // ====================================================
    if (isRadeon()) {
        // R200 technically has a programmable pipeline, but since it's SM 1.4,
        // it's too limited to to be of any practical value to us.
        if (m_chipClass < R300) {
            m_supportsGLSL = false;
        }

        m_limitedGLSL = false;
        m_limitedNPOT = false;

        if (m_chipClass < R600) {
            if (driver() == Driver_Catalyst) {
                m_textureNPOT = m_limitedNPOT = false; // Software fallback
            } else if (driver() == Driver_R300G) {
                m_limitedNPOT = m_textureNPOT;
            }

            m_limitedGLSL = m_supportsGLSL;
        }

        if (m_chipClass < R300) {
            // fallback to NoCompositing for R100 and R200
            m_recommendedCompositor = NoCompositing;
        } else if (m_chipClass < R600) {
            // NoCompositing due to NPOT limitations not supported by KWin's shaders
            m_recommendedCompositor = NoCompositing;
        } else {
            m_recommendedCompositor = OpenGLCompositing;
        }

        if (driver() == Driver_R600G || (driver() == Driver_R600C && m_renderer.contains("DRI2"))) {
            m_looseBinding = true;
        }
    }

    if (isNvidia()) {
        if (m_driver == Driver_NVidia && m_chipClass < NV40) {
            m_supportsGLSL = false; // High likelihood of software emulation
        }

        if (m_driver == Driver_NVidia) {
            m_looseBinding = true;
            m_preferBufferSubData = true;
        }

        if (m_chipClass < NV40) {
            m_recommendedCompositor = NoCompositing;
        } else {
            m_recommendedCompositor = OpenGLCompositing;
        }

        m_limitedNPOT = m_textureNPOT && m_chipClass < NV40;
        m_limitedGLSL = m_supportsGLSL && m_chipClass < G80;
    }

    if (isIntel()) {
        if (m_chipClass < I915) {
            m_supportsGLSL = false;
        }

        m_limitedGLSL = m_supportsGLSL && m_chipClass < I965;
        // see https://bugs.freedesktop.org/show_bug.cgi?id=80349#c1
        m_looseBinding = false;

        if (m_chipClass < I915) {
            m_recommendedCompositor = NoCompositing;
        } else {
            m_recommendedCompositor = OpenGLCompositing;
        }
    }

    if (isPanfrost()) {
        m_recommendedCompositor = OpenGLCompositing;
    }

    if (isLima()) {
        m_recommendedCompositor = OpenGLCompositing;
        m_supportsGLSL = true;
    }

    if (isVideoCore4()) {
        // OpenGL works, but is much slower than QPainter
        m_recommendedCompositor = QPainterCompositing;
    }

    if (isVideoCore3D()) {
        // OpenGL works, but is much slower than QPainter
        m_recommendedCompositor = QPainterCompositing;
    }

    if (isMesaDriver() && platformInterface == EglPlatformInterface) {
        // According to the reference implementation in
        // mesa/demos/src/egl/opengles1/texture_from_pixmap
        // the mesa egl implementation does not require a strict binding (so far).
        m_looseBinding = true;
    }

    if (isSoftwareEmulation()) {
        if (m_driver < Driver_Llvmpipe) {
            // we recommend QPainter
            m_recommendedCompositor = QPainterCompositing;
            // Software emulation does not provide GLSL
            m_limitedGLSL = m_supportsGLSL = false;
        } else {
            // llvmpipe does support GLSL
            m_recommendedCompositor = OpenGLCompositing;
            m_limitedGLSL = false;
            m_supportsGLSL = true;
        }
    }

    if (m_driver == Driver_Qualcomm) {
        if (m_chipClass == Adreno1XX) {
            m_recommendedCompositor = NoCompositing;
        } else {
            // all other drivers support at least GLES 2
            m_recommendedCompositor = OpenGLCompositing;
        }
    }

    if (m_chipClass == UnknownChipClass && m_driver == Driver_Unknown) {
        // we don't know the hardware. Let's be optimistic and assume OpenGL compatible hardware
        m_recommendedCompositor = OpenGLCompositing;
        m_supportsGLSL = true;
    }

    if (isVirtualBox()) {
        m_virtualMachine = true;
        m_recommendedCompositor = OpenGLCompositing;
    }

    if (isVMware()) {
        m_virtualMachine = true;
        m_recommendedCompositor = OpenGLCompositing;
    }

    if (m_driver == Driver_Virgl) {
        m_virtualMachine = true;
        m_recommendedCompositor = OpenGLCompositing;
    }

    // and force back to shader supported on gles, we wouldn't have got a context if not supported
    if (isGLES()) {
        m_supportsGLSL = true;
        m_limitedGLSL = false;
    }
}

static void print(const QByteArray &label, const QByteArray &setting)
{
    qInfo("%-40s%s", label.data(), setting.data());
}

void GLPlatform::printResults() const
{
    print(QByteArrayLiteral("OpenGL vendor string:"), m_vendor);
    print(QByteArrayLiteral("OpenGL renderer string:"), m_renderer);
    print(QByteArrayLiteral("OpenGL version string:"), m_version);

    if (m_supportsGLSL) {
        print(QByteArrayLiteral("OpenGL shading language version string:"), m_glsl_version);
    }

    print(QByteArrayLiteral("Driver:"), driverToString8(m_driver));
    if (!isMesaDriver()) {
        print(QByteArrayLiteral("Driver version:"), versionToString8(m_driverVersion));
    }

    print(QByteArrayLiteral("GPU class:"), chipClassToString8(m_chipClass));

    print(QByteArrayLiteral("OpenGL version:"), versionToString8(m_glVersion));

    if (m_supportsGLSL) {
        print(QByteArrayLiteral("GLSL version:"), versionToString8(m_glslVersion));
    }

    if (isMesaDriver()) {
        print(QByteArrayLiteral("Mesa version:"), versionToString8(mesaVersion()));
    }
    // if (galliumVersion() > 0)
    //     print("Gallium version:", versionToString(m_galliumVersion));
    if (serverVersion() > 0) {
        print(QByteArrayLiteral("X server version:"), versionToString8(m_serverVersion));
    }
    if (kernelVersion() > 0) {
        print(QByteArrayLiteral("Linux kernel version:"), versionToString8(m_kernelVersion));
    }

    print(QByteArrayLiteral("Requires strict binding:"), !m_looseBinding ? QByteArrayLiteral("yes") : QByteArrayLiteral("no"));
    print(QByteArrayLiteral("GLSL shaders:"), m_supportsGLSL ? (m_limitedGLSL ? QByteArrayLiteral("limited") : QByteArrayLiteral("yes")) : QByteArrayLiteral("no"));
    print(QByteArrayLiteral("Texture NPOT support:"), m_textureNPOT ? (m_limitedNPOT ? QByteArrayLiteral("limited") : QByteArrayLiteral("yes")) : QByteArrayLiteral("no"));
    print(QByteArrayLiteral("Virtual Machine:"), m_virtualMachine ? QByteArrayLiteral("yes") : QByteArrayLiteral("no"));
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

    case TextureNPOT:
        return m_textureNPOT;

    case LimitedNPOT:
        return m_limitedNPOT;

    case PackInvert:
        return m_packInvert;

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
    if (isMesaDriver()) {
        return mesaVersion();
    }

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

bool GLPlatform::isVirgl() const
{
    return m_driver == Driver_Virgl;
}

bool GLPlatform::isSoftwareEmulation() const
{
    return m_driver == Driver_Softpipe || m_driver == Driver_Swrast || m_driver == Driver_Llvmpipe;
}

bool GLPlatform::isAdreno() const
{
    return m_chipClass >= Adreno1XX && m_chipClass <= UnknownAdreno;
}

bool GLPlatform::isPanfrost() const
{
    return m_chipClass >= MaliT7XX && m_chipClass <= UnknownPanfrost;
}

bool GLPlatform::isLima() const
{
    return m_chipClass >= Mali400 && m_chipClass <= UnknownLima;
}

bool GLPlatform::isVideoCore4() const
{
    return m_chipClass >= VC4_2_1 && m_chipClass <= UnknownVideoCore4;
}

bool GLPlatform::isVideoCore3D() const
{
    return m_chipClass >= V3D_4_2 && m_chipClass <= UnknownVideoCore3D;
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

OpenGLPlatformInterface GLPlatform::platformInterface() const
{
    return m_platformInterface;
}

bool GLPlatform::isGLES() const
{
    return m_gles;
}

void GLPlatform::cleanup()
{
    s_platform.reset();
}

} // namespace KWin
