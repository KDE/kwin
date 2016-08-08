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
