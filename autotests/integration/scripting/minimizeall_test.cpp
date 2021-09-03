/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "abstract_client.h"
#include "abstract_output.h"
#include "platform.h"
#include "screens.h"
#include "scripting/scripting.h"
#include "wayland_server.h"
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

    qRegisterMetaType<AbstractClient *>();

    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = kwinApp()->platform()->enabledOutputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    Test::initWaylandWorkspace();
}

static QString locateMainScript(const QString &pluginName)
{
    const QList<KPluginMetaData> offers = KPackage::PackageLoader::self()->findPackages(
        QStringLiteral("KWin/Script"),
        QStringLiteral("kwin/scripts"),
        [&](const KPluginMetaData &metaData) {
            return metaData.pluginId() == pluginName;
        }
    );
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
    QVERIFY(runningChangedSpy.isValid());
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

    using namespace KWayland::Client;

    // Create a couple of test clients.
    QScopedPointer<KWayland::Client::Surface> surface1(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.data()));
    AbstractClient *client1 = Test::renderAndWaitForShown(surface1.data(), QSize(100, 50), Qt::blue);
    QVERIFY(client1);
    QVERIFY(client1->isActive());
    QVERIFY(client1->isMinimizable());

    QScopedPointer<KWayland::Client::Surface> surface2(Test::createSurface());
    QScopedPointer<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.data()));
    AbstractClient *client2 = Test::renderAndWaitForShown(surface2.data(), QSize(100, 50), Qt::red);
    QVERIFY(client2);
    QVERIFY(client2->isActive());
    QVERIFY(client2->isMinimizable());

    // Minimize the windows.
    quint32 timestamp = 1;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTSHIFT, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_D, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_D, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTSHIFT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTMETA, timestamp++);

    QTRY_VERIFY(client1->isMinimized());
    QTRY_VERIFY(client2->isMinimized());

    // Unminimize the windows.
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTSHIFT, timestamp++);
    kwinApp()->platform()->keyboardKeyPressed(KEY_D, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_D, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTSHIFT, timestamp++);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTMETA, timestamp++);

    QTRY_VERIFY(!client1->isMinimized());
    QTRY_VERIFY(!client2->isMinimized());

    // Destroy test clients.
    shellSurface2.reset();
    QVERIFY(Test::waitForWindowDestroyed(client2));
    shellSurface1.reset();
    QVERIFY(Test::waitForWindowDestroyed(client1));
}

}

WAYLANDTEST_MAIN(KWin::MinimizeAllScriptTest)
#include "minimizeall_test.moc"
