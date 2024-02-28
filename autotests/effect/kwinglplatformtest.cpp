/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "opengl/glplatform.h"
#include <QTest>

#include <KConfig>
#include <KConfigGroup>

Q_DECLARE_METATYPE(KWin::Driver)
Q_DECLARE_METATYPE(KWin::ChipClass)

using namespace KWin;

class GLPlatformTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testDriverToString_data();
    void testDriverToString();
    void testChipClassToString_data();
    void testChipClassToString();
    void testDetect_data();
    void testDetect();
};

void GLPlatformTest::testDriverToString_data()
{
    QTest::addColumn<Driver>("driver");
    QTest::addColumn<QString>("expected");

    QTest::newRow("R100") << Driver_R100 << QStringLiteral("Radeon");
    QTest::newRow("R200") << Driver_R200 << QStringLiteral("R200");
    QTest::newRow("R300C") << Driver_R300C << QStringLiteral("R300C");
    QTest::newRow("R300G") << Driver_R300G << QStringLiteral("R300G");
    QTest::newRow("R600C") << Driver_R600C << QStringLiteral("R600C");
    QTest::newRow("R600G") << Driver_R600G << QStringLiteral("R600G");
    QTest::newRow("RadeonSI") << Driver_RadeonSI << QStringLiteral("RadeonSI");
    QTest::newRow("Nouveau") << Driver_Nouveau << QStringLiteral("Nouveau");
    QTest::newRow("Intel") << Driver_Intel << QStringLiteral("Intel");
    QTest::newRow("NVidia") << Driver_NVidia << QStringLiteral("NVIDIA");
    QTest::newRow("Catalyst") << Driver_Catalyst << QStringLiteral("Catalyst");
    QTest::newRow("Swrast") << Driver_Swrast << QStringLiteral("Software rasterizer");
    QTest::newRow("Softpipe") << Driver_Softpipe << QStringLiteral("softpipe");
    QTest::newRow("Llvmpipe") << Driver_Llvmpipe << QStringLiteral("LLVMpipe");
    QTest::newRow("VirtualBox") << Driver_VirtualBox << QStringLiteral("VirtualBox (Chromium)");
    QTest::newRow("VMware") << Driver_VMware << QStringLiteral("VMware (SVGA3D)");
    QTest::newRow("Qualcomm") << Driver_Qualcomm << QStringLiteral("Qualcomm");
    QTest::newRow("Virgl") << Driver_Virgl << QStringLiteral("Virgl (virtio-gpu, Qemu/KVM guest)");
    QTest::newRow("Panfrost") << Driver_Panfrost << QStringLiteral("Panfrost");
    QTest::newRow("Lima") << Driver_Lima << QStringLiteral("Mali (Lima)");
    QTest::newRow("VC4") << Driver_VC4 << QStringLiteral("VideoCore IV");
    QTest::newRow("V3D") << Driver_V3D << QStringLiteral("VideoCore 3D");
    QTest::newRow("Unknown") << Driver_Unknown << QStringLiteral("Unknown");
}

void GLPlatformTest::testDriverToString()
{
    QFETCH(Driver, driver);
    QTEST(GLPlatform::driverToString(driver), "expected");
}

void GLPlatformTest::testChipClassToString_data()
{
    QTest::addColumn<ChipClass>("chipClass");
    QTest::addColumn<QString>("expected");

    QTest::newRow("R100") << R100 << QStringLiteral("R100");
    QTest::newRow("R200") << R200 << QStringLiteral("R200");
    QTest::newRow("R300") << R300 << QStringLiteral("R300");
    QTest::newRow("R400") << R400 << QStringLiteral("R400");
    QTest::newRow("R500") << R500 << QStringLiteral("R500");
    QTest::newRow("R600") << R600 << QStringLiteral("R600");
    QTest::newRow("R700") << R700 << QStringLiteral("R700");
    QTest::newRow("Evergreen") << Evergreen << QStringLiteral("EVERGREEN");
    QTest::newRow("NorthernIslands") << NorthernIslands << QStringLiteral("Northern Islands");
    QTest::newRow("SouthernIslands") << SouthernIslands << QStringLiteral("Southern Islands");
    QTest::newRow("SeaIslands") << SeaIslands << QStringLiteral("Sea Islands");
    QTest::newRow("VolcanicIslands") << VolcanicIslands << QStringLiteral("Volcanic Islands");
    QTest::newRow("Arctic Islands") << ArcticIslands << QStringLiteral("Arctic Islands");
    QTest::newRow("Vega") << Vega << QStringLiteral("Vega");
    QTest::newRow("UnknownRadeon") << UnknownRadeon << QStringLiteral("Unknown");
    QTest::newRow("NV10") << NV10 << QStringLiteral("NV10");
    QTest::newRow("NV20") << NV20 << QStringLiteral("NV20");
    QTest::newRow("NV30") << NV30 << QStringLiteral("NV30");
    QTest::newRow("NV40") << NV40 << QStringLiteral("NV40/G70");
    QTest::newRow("G80") << G80 << QStringLiteral("G80/G90");
    QTest::newRow("GF100") << GF100 << QStringLiteral("GF100");
    QTest::newRow("UnknownNVidia") << UnknownNVidia << QStringLiteral("Unknown");
    QTest::newRow("I8XX") << I8XX << QStringLiteral("i830/i835");
    QTest::newRow("I915") << I915 << QStringLiteral("i915/i945");
    QTest::newRow("I965") << I965 << QStringLiteral("i965");
    QTest::newRow("SandyBridge") << SandyBridge << QStringLiteral("SandyBridge");
    QTest::newRow("IvyBridge") << IvyBridge << QStringLiteral("IvyBridge");
    QTest::newRow("Haswell") << Haswell << QStringLiteral("Haswell");
    QTest::newRow("UnknownIntel") << UnknownIntel << QStringLiteral("Unknown");
    QTest::newRow("Adreno1XX") << Adreno1XX << QStringLiteral("Adreno 1xx series");
    QTest::newRow("Adreno2XX") << Adreno2XX << QStringLiteral("Adreno 2xx series");
    QTest::newRow("Adreno3XX") << Adreno3XX << QStringLiteral("Adreno 3xx series");
    QTest::newRow("Adreno4XX") << Adreno4XX << QStringLiteral("Adreno 4xx series");
    QTest::newRow("Adreno5XX") << Adreno5XX << QStringLiteral("Adreno 5xx series");
    QTest::newRow("UnknownAdreno") << UnknownAdreno << QStringLiteral("Unknown");
    QTest::newRow("MaliT7XX") << MaliT7XX << QStringLiteral("Mali T7xx series");
    QTest::newRow("MaliT8XX") << MaliT8XX << QStringLiteral("Mali T8xx series");
    QTest::newRow("MaliGXX") << MaliGXX << QStringLiteral("Mali Gxx series");
    QTest::newRow("UnknownPanfrost") << UnknownPanfrost << QStringLiteral("Unknown");
    QTest::newRow("Mali400") << Mali400 << QStringLiteral("Mali 400 series");
    QTest::newRow("Mali450") << Mali450 << QStringLiteral("Mali 450 series");
    QTest::newRow("Mali470") << Mali470 << QStringLiteral("Mali 470 series");
    QTest::newRow("UnknownLima") << UnknownLima << QStringLiteral("Unknown");
    QTest::newRow("VC4_2_1") << VC4_2_1 << QStringLiteral("VideoCore IV");
    QTest::newRow("UnknownVideoCore4") << UnknownVideoCore4 << QStringLiteral("Unknown");
    QTest::newRow("V3D_4_2") << V3D_4_2 << QStringLiteral("VideoCore 3D");
    QTest::newRow("UnknownVideoCore3D") << UnknownVideoCore3D << QStringLiteral("Unknown");
    QTest::newRow("UnknownChipClass") << UnknownChipClass << QStringLiteral("Unknown");
}

void GLPlatformTest::testChipClassToString()
{
    QFETCH(ChipClass, chipClass);
    QTEST(GLPlatform::chipClassToString(chipClass), "expected");
}

void GLPlatformTest::testDetect_data()
{
    QTest::addColumn<QString>("configFile");

    QDir dir(QFINDTESTDATA("data/glplatform"));
    const QStringList entries = dir.entryList(QDir::NoDotAndDotDot | QDir::Files);

    for (const QString &file : entries) {
        QTest::newRow(file.toUtf8().constData()) << dir.absoluteFilePath(file);
    }
}

static Version readVersion(const KConfigGroup &group, const char *entry)
{
    const QStringList parts = group.readEntry(entry, QString()).split(',');
    if (parts.count() < 2) {
        return Version();
    }
    QList<qint64> versionParts;
    for (int i = 0; i < parts.count(); ++i) {
        bool ok = false;
        const auto value = parts.at(i).toLongLong(&ok);
        if (ok) {
            versionParts << value;
        } else {
            versionParts << 0;
        }
    }
    while (versionParts.count() < 3) {
        versionParts << 0;
    }
    return Version(versionParts.at(0), versionParts.at(1), versionParts.at(2));
}

void GLPlatformTest::testDetect()
{
    QFETCH(QString, configFile);
    KConfig config(configFile);
    const KConfigGroup driverGroup = config.group(QStringLiteral("Driver"));

    const auto version = driverGroup.readEntry("Version").toUtf8();
    const auto glslVersion = driverGroup.readEntry("ShadingLanguageVersion").toUtf8();
    const auto renderer = driverGroup.readEntry("Renderer").toUtf8();
    const auto vendor = driverGroup.readEntry("Vendor").toUtf8();
    GLPlatform gl(EglPlatformInterface, version, glslVersion, renderer, vendor);
    QCOMPARE(gl.platformInterface(), EglPlatformInterface);

    const KConfigGroup settingsGroup = config.group(QStringLiteral("Settings"));

    QCOMPARE(gl.isLooseBinding(), settingsGroup.readEntry("LooseBinding", false));

    QCOMPARE(gl.glVersion(), readVersion(settingsGroup, "GLVersion"));
    QCOMPARE(gl.glslVersion(), readVersion(settingsGroup, "GLSLVersion"));
    QCOMPARE(gl.mesaVersion(), readVersion(settingsGroup, "MesaVersion"));
    QEXPECT_FAIL("amd-catalyst-radeonhd-7700M-3.1.13399", "Detects GL version instead of driver version", Continue);
    QCOMPARE(gl.driverVersion(), readVersion(settingsGroup, "DriverVersion"));

    QCOMPARE(gl.driver(), Driver(settingsGroup.readEntry("Driver", int(Driver_Unknown))));
    QCOMPARE(gl.chipClass(), ChipClass(settingsGroup.readEntry("ChipClass", int(UnknownChipClass))));

    QCOMPARE(gl.isMesaDriver(), settingsGroup.readEntry("Mesa", false));
    QCOMPARE(gl.isRadeon(), settingsGroup.readEntry("Radeon", false));
    QCOMPARE(gl.isNvidia(), settingsGroup.readEntry("Nvidia", false));
    QCOMPARE(gl.isIntel(), settingsGroup.readEntry("Intel", false));
    QCOMPARE(gl.isVirtualBox(), settingsGroup.readEntry("VirtualBox", false));
    QCOMPARE(gl.isVMware(), settingsGroup.readEntry("VMware", false));
    QCOMPARE(gl.isAdreno(), settingsGroup.readEntry("Adreno", false));
    QCOMPARE(gl.isPanfrost(), settingsGroup.readEntry("Panfrost", false));
    QCOMPARE(gl.isLima(), settingsGroup.readEntry("Lima", false));
    QCOMPARE(gl.isVideoCore4(), settingsGroup.readEntry("VC4", false));
    QCOMPARE(gl.isVideoCore3D(), settingsGroup.readEntry("V3D", false));
    QCOMPARE(gl.isVirgl(), settingsGroup.readEntry("Virgl", false));

    QCOMPARE(gl.isVirtualMachine(), settingsGroup.readEntry("VirtualMachine", false));

    QCOMPARE(gl.glVersionString(), version);
    QCOMPARE(gl.glRendererString(), renderer);
    QCOMPARE(gl.glVendorString(), vendor);
    QCOMPARE(gl.glShadingLanguageVersionString(), glslVersion);

    QCOMPARE(gl.isLooseBinding(), settingsGroup.readEntry("LooseBinding", false));
    QCOMPARE(gl.recommendedCompositor(), CompositingType(settingsGroup.readEntry("Compositor", int(NoCompositing))));
    QCOMPARE(gl.preferBufferSubData(), settingsGroup.readEntry("PreferBufferSubData", false));
}

QTEST_GUILESS_MAIN(GLPlatformTest)
#include "kwinglplatformtest.moc"
