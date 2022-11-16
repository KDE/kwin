/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "scripting/scripting.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KPackage/PackageLoader>
#include <KWayland/Client/surface.h>

#include <linux/input.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_minimizeall-0");
static const QString s_scriptName = QStringLiteral("minimizeall");

class MinimizeAllScriptTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testMinimizeUnminimize();
};

void MinimizeAllScriptTest::initTestCase()
{
    qputenv("XDG_DATA_DIRS", QCoreApplication::applicationDirPath().toUtf8());

    qRegisterMetaType<Window *>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

static QString locateMainScript(const QString &pluginName)
{
    const QList<KPluginMetaData> offers = KPackage::PackageLoader::self()->findPackages(
        QStringLiteral("KWin/Script"),
        QStringLiteral("kwin/scripts"),
        [&](const KPluginMetaData &metaData) {
            return metaData.pluginId() == pluginName;
        });
    if (offers.isEmpty()) {
        return QString();
    }
    const KPluginMetaData &metaData = offers.first();
    const QString mainScriptFileName = metaData.value(QStringLiteral("X-Plasma-MainScript"));
    const QFileInfo metaDataFileInfo(metaData.fileName());
    return metaDataFileInfo.path() + QLatin1String("/contents/") + mainScriptFileName;
}

void MinimizeAllScriptTest::init()
{
    QVERIFY(Test::setupWaylandConnection());

    Scripting::self()->loadScript(locateMainScript(s_scriptName), s_scriptName);
    QTRY_VERIFY(Scripting::self()->isScriptLoaded(s_scriptName));

    AbstractScript *script = Scripting::self()->findScript(s_scriptName);
    QVERIFY(script);
    QSignalSpy runningChangedSpy(script, &AbstractScript::runningChanged);
    script->run();
    QTRY_COMPARE(runningChangedSpy.count(), 1);
}

void MinimizeAllScriptTest::cleanup()
{
    Test::destroyWaylandConnection();

    Scripting::self()->unloadScript(s_scriptName);
    QTRY_VERIFY(!Scripting::self()->isScriptLoaded(s_scriptName));
}

void MinimizeAllScriptTest::testMinimizeUnminimize()
{
    // This test verifies that all windows are minimized when Meta+Shift+D
    // is pressed, and unminimized when the shortcut is pressed once again.

    // Create a couple of test windows.
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    Window *window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window1);
    QVERIFY(window1->isActive());
    QVERIFY(window1->isMinimizable());

    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    Window *window2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::red);
    QVERIFY(window2);
    QVERIFY(window2->isActive());
    QVERIFY(window2->isMinimizable());

    // Minimize the windows.
    quint32 timestamp = 1;
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTSHIFT, timestamp++);
    Test::keyboardKeyPressed(KEY_D, timestamp++);
    Test::keyboardKeyReleased(KEY_D, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTSHIFT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);

    QTRY_VERIFY(window1->isMinimized());
    QTRY_VERIFY(window2->isMinimized());

    // Unminimize the windows.
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    Test::keyboardKeyPressed(KEY_LEFTSHIFT, timestamp++);
    Test::keyboardKeyPressed(KEY_D, timestamp++);
    Test::keyboardKeyReleased(KEY_D, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTSHIFT, timestamp++);
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);

    QTRY_VERIFY(!window1->isMinimized());
    QTRY_VERIFY(!window2->isMinimized());

    // Destroy test windows.
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(window2));
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(window1));
}

}

WAYLANDTEST_MAIN(KWin::MinimizeAllScriptTest)
#include "minimizeall_test.moc"
