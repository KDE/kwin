
/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "composite.h"
#include "core/output.h"
#include "core/outputbackend.h"
#include "core/renderbackend.h"
#include "cursor.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"
#include <kwineffects.h>

#include <KWayland/Client/surface.h>

#include <KDecoration2/Decoration>

#include <linux/input.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_dont_crash_no_border-0");

class DontCrashNoBorder : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testCreateWindow();
};

void DontCrashNoBorder::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group("org.kde.kdecoration2").writeEntry("NoPlugin", true);
    config->sync();
    kwinApp()->setConfig(config);

    // this test needs to enforce OpenGL compositing to get into the crashy condition
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));
    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);

    QCOMPARE(Compositor::self()->backend()->compositingType(), KWin::OpenGLCompositing);
}

void DontCrashNoBorder::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::XdgDecorationV1));

    workspace()->setActiveOutput(QPoint(640, 512));
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void DontCrashNoBorder::cleanup()
{
    Test::destroyWaylandConnection();
}

void DontCrashNoBorder::testCreateWindow()
{
    // create a window and ensure that this doesn't crash
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    std::unique_ptr<Test::XdgToplevelDecorationV1> decoration(Test::createXdgToplevelDecorationV1(shellSurface.get()));
    QSignalSpy decorationConfigureRequestedSpy(decoration.get(), &Test::XdgToplevelDecorationV1::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);

    // Initialize the xdg-toplevel surface.
    decoration->set_mode(Test::XdgToplevelDecorationV1::mode_server_side);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(decorationConfigureRequestedSpy.last().at(0).value<Test::XdgToplevelDecorationV1::mode>(), Test::XdgToplevelDecorationV1::mode_client_side);

    // let's render
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(500, 50), Qt::blue);
    QVERIFY(window);
    QCOMPARE(workspace()->activeWindow(), window);
    QVERIFY(!window->isDecorated());
}

}

WAYLANDTEST_MAIN(KWin::DontCrashNoBorder)
#include "dont_crash_no_border.moc"
