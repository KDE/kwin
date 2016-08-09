/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#include "mock_gl.h"
#include <QtTest/QTest>
#include <kwinglplatform.h>

#include <KConfig>
#include <KConfigGroup>

Q_DECLARE_METATYPE(KWin::Driver)
Q_DECLARE_METATYPE(KWin::ChipClass)

using namespace KWin;

void KWin::cleanupGL()
{
    GLPlatform::cleanup();
}

class GLPlatformTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void cleanup();

    void testDriverToString_data();
    void testDriverToString();
    void testChipClassToString_data();
    void testChipClassToString();
    void testPriorDetect();
    void testDetect_data();
    void testDetect();
};

void GLPlatformTest::cleanup()
{
    cleanupGL();
    delete s_gl;
    s_gl = nullptr;
}

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
    QTest::newRow("Nouveau") << Driver_Nouveau << QStringLiteral("Nouveau");
    QTest::newRow("Intel") << Driver_Intel << QStringLiteral("Intel");
    QTest::newRow("NVidia") << Driver_NVidia << QStringLiteral("NVIDIA");
    QTest::newRow("Catalyst") << Driver_Catalyst << QStringLiteral("Catalyst");
    QTest::newRow("Swrast") << Driver_Swrast << QStringLiteral("Software rasterizer");
    QTest::newRow("Softpipe") << Driver_Softpipe << QStringLiteral("softpipe");
    QTest::newRow("Llvmpipe") << Driver_Llvmpipe << QStringLiteral("LLVMpipe");
    QTest::newRow("VirtualBox") << Driver_VirtualBox << QStringLiteral("VirtualBox (Chromium)");
    QTest::newRow("VMware") << Driver_VMware << QStringLiteral("VMware (SVGA3D)");
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
    QTest::newRow("NorthernIslands") << NorthernIslands << QStringLiteral("NI");
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
    QTest::newRow("UnknownChipClass") << UnknownChipClass << QStringLiteral("Unknown");
}

void GLPlatformTest::testChipClassToString()
{
    QFETCH(ChipClass, chipClass);
    QTEST(GLPlatform::chipClassToString(chipClass), "expected");
}

void GLPlatformTest::testPriorDetect()
{
    auto *gl = GLPlatform::instance();
    QVERIFY(gl);
    QCOMPARE(gl->supports(LooseBinding), false);
    QCOMPARE(gl->supports(GLSL), false);
    QCOMPARE(gl->supports(LimitedGLSL), false);
    QCOMPARE(gl->supports(TextureNPOT), false);
    QCOMPARE(gl->supports(LimitedNPOT), false);

    QCOMPARE(gl->glVersion(), 0);
    QCOMPARE(gl->glslVersion(), 0);
    QCOMPARE(gl->mesaVersion(), 0);
    QCOMPARE(gl->galliumVersion(), 0);
    QCOMPARE(gl->serverVersion(), 0);
    QCOMPARE(gl->kernelVersion(), 0);
    QCOMPARE(gl->driverVersion(), 0);

    QCOMPARE(gl->driver(), Driver_Unknown);
    QCOMPARE(gl->chipClass(), UnknownChipClass);

    QCOMPARE(gl->isMesaDriver(), false);
    QCOMPARE(gl->isGalliumDriver(), false);
    QCOMPARE(gl->isRadeon(), false);
    QCOMPARE(gl->isNvidia(), false);
    QCOMPARE(gl->isIntel(), false);
    QCOMPARE(gl->isVirtualBox(), false);
    QCOMPARE(gl->isVMware(), false);

    QCOMPARE(gl->isSoftwareEmulation(), false);
    QCOMPARE(gl->isVirtualMachine(), false);

    QCOMPARE(gl->glVersionString(), QByteArray());
    QCOMPARE(gl->glRendererString(), QByteArray());
    QCOMPARE(gl->glVendorString(), QByteArray());
    QCOMPARE(gl->glShadingLanguageVersionString(), QByteArray());

    QCOMPARE(gl->isLooseBinding(), false);
    QCOMPARE(gl->isGLES(), false);
    QCOMPARE(gl->recommendedCompositor(), XRenderCompositing);
    QCOMPARE(gl->preferBufferSubData(), false);
    QCOMPARE(gl->platformInterface(), NoOpenGLPlatformInterface);
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

static qint64 readVersion(const KConfigGroup &group, const char *entry)
{
    const QStringList parts = group.readEntry(entry, QString()).split(',');
    if (parts.count() < 2) {
        return 0;
    }
    QVector<qint64> versionParts;
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
    return kVersionNumber(versionParts.at(0), versionParts.at(1), versionParts.at(2));
}

void GLPlatformTest::testDetect()
{
    QFETCH(QString, configFile);
    KConfig config(configFile);
    const KConfigGroup driverGroup = config.group("Driver");
    s_gl = new MockGL;
    s_gl->getString.vendor = driverGroup.readEntry("Vendor").toUtf8();
    s_gl->getString.renderer = driverGroup.readEntry("Renderer").toUtf8();
    s_gl->getString.version = driverGroup.readEntry("Version").toUtf8();
    s_gl->getString.shadingLanguageVersion = driverGroup.readEntry("ShadingLanguageVersion").toUtf8();
    s_gl->getString.extensions = QVector<QByteArray>{QByteArrayLiteral("GL_ARB_shader_objects"),
                                                     QByteArrayLiteral("GL_ARB_fragment_shader"),
                                                     QByteArrayLiteral("GL_ARB_vertex_shader"),
                                                     QByteArrayLiteral("GL_ARB_texture_non_power_of_two")};
    s_gl->getString.extensionsString = QByteArray();

    auto *gl = GLPlatform::instance();
    QVERIFY(gl);
    gl->detect(EglPlatformInterface);
    QCOMPARE(gl->platformInterface(), EglPlatformInterface);

    const KConfigGroup settingsGroup = config.group("Settings");

    QCOMPARE(gl->supports(LooseBinding), settingsGroup.readEntry("LooseBinding", false));
    QCOMPARE(gl->supports(GLSL), settingsGroup.readEntry("GLSL", false));
    QCOMPARE(gl->supports(LimitedGLSL), settingsGroup.readEntry("LimitedGLSL", false));
    QCOMPARE(gl->supports(TextureNPOT), settingsGroup.readEntry("TextureNPOT", false));
    QCOMPARE(gl->supports(LimitedNPOT), settingsGroup.readEntry("LimitedNPOT", false));

    QCOMPARE(gl->glVersion(), readVersion(settingsGroup, "GLVersion"));
    QCOMPARE(gl->glslVersion(), readVersion(settingsGroup, "GLSLVersion"));
    QCOMPARE(gl->mesaVersion(), readVersion(settingsGroup, "MesaVersion"));
    QCOMPARE(gl->galliumVersion(), readVersion(settingsGroup, "GalliumVersion"));
    QCOMPARE(gl->serverVersion(), 0);
    QEXPECT_FAIL("amd-catalyst-radeonhd-7700M-3.1.13399", "Detects GL version instead of driver version", Continue);
    QCOMPARE(gl->driverVersion(), readVersion(settingsGroup, "DriverVersion"));

    QEXPECT_FAIL("amd-gallium-bonaire-3.0", "Not detected as a radeon driver", Continue);
    QEXPECT_FAIL("amd-gallium-hawaii-3.0", "Not detected as a radeon driver", Continue);
    QEXPECT_FAIL("amd-gallium-tonga-4.1", "Not detected as a radeon driver", Continue);
    QCOMPARE(gl->driver(), Driver(settingsGroup.readEntry("Driver", int(Driver_Unknown))));
    QEXPECT_FAIL("amd-gallium-bonaire-3.0", "Not detected as a radeon driver", Continue);
    QEXPECT_FAIL("amd-gallium-hawaii-3.0", "Not detected as a radeon driver", Continue);
    QEXPECT_FAIL("amd-gallium-tonga-4.1", "Not detected as a radeon driver", Continue);
    QCOMPARE(gl->chipClass(), ChipClass(settingsGroup.readEntry("ChipClass", int(UnknownChipClass))));

    QCOMPARE(gl->isMesaDriver(), settingsGroup.readEntry("Mesa", false));
    QCOMPARE(gl->isGalliumDriver(), settingsGroup.readEntry("Gallium", false));
    QEXPECT_FAIL("amd-gallium-bonaire-3.0", "Not detected as a radeon driver", Continue);
    QEXPECT_FAIL("amd-gallium-hawaii-3.0", "Not detected as a radeon driver", Continue);
    QEXPECT_FAIL("amd-gallium-tonga-4.1", "Not detected as a radeon driver", Continue);
    QCOMPARE(gl->isRadeon(), settingsGroup.readEntry("Radeon", false));
    QCOMPARE(gl->isNvidia(), settingsGroup.readEntry("Nvidia", false));
    QCOMPARE(gl->isIntel(), settingsGroup.readEntry("Intel", false));
    QCOMPARE(gl->isVirtualBox(), settingsGroup.readEntry("VirtualBox", false));
    QCOMPARE(gl->isVMware(), settingsGroup.readEntry("VMware", false));

    QCOMPARE(gl->isSoftwareEmulation(), settingsGroup.readEntry("SoftwareEmulation", false));
    QCOMPARE(gl->isVirtualMachine(), settingsGroup.readEntry("VirtualMachine", false));

    QCOMPARE(gl->glVersionString(), s_gl->getString.version);
    QCOMPARE(gl->glRendererString(), s_gl->getString.renderer);
    QCOMPARE(gl->glVendorString(), s_gl->getString.vendor);
    QCOMPARE(gl->glShadingLanguageVersionString(), s_gl->getString.shadingLanguageVersion);

    QCOMPARE(gl->isLooseBinding(), settingsGroup.readEntry("LooseBinding", false));
    QCOMPARE(gl->isGLES(), settingsGroup.readEntry("GLES", false));
    QCOMPARE(gl->recommendedCompositor(), CompositingType(settingsGroup.readEntry("Compositor", int(NoCompositing))));
    QCOMPARE(gl->preferBufferSubData(), settingsGroup.readEntry("PreferBufferSubData", false));
}

QTEST_GUILESS_MAIN(GLPlatformTest)
#include "kwinglplatformtest.moc"
