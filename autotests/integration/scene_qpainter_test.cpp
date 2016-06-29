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
#include "kwin_wayland_test.h"
#include "composite.h"
#include "effectloader.h"
#include "cursor.h"
#include "platform.h"
#include "scene_qpainter.h"
#include "wayland_server.h"
#include "effect_builtins.h"

#include <KConfigGroup>

#include <QPainter>

using namespace KWin;
static const QString s_socketName = QStringLiteral("wayland_test_kwin_scene_qpainter-0");

class SceneQPainterTest : public QObject
{
Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testStartFrame();
    void testCursorMoving();
};

void SceneQPainterTest::initTestCase()
{
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    waylandServer()->init(s_socketName.toLocal8Bit());

    // disable all effects - we don't want to have it interact with the rendering
    auto config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    KConfigGroup plugins(config, QStringLiteral("Plugins"));
    ScriptedEffectLoader loader;
    const auto builtinNames = BuiltInEffects::availableEffectNames() << loader.listOfKnownEffects();
    for (QString name : builtinNames) {
        plugins.writeEntry(name + QStringLiteral("Enabled"), false);
    }

    config->sync();
    kwinApp()->setConfig(config);

    qputenv("XCURSOR_THEME", QByteArrayLiteral("DMZ-White"));
    qputenv("XCURSOR_SIZE", QByteArrayLiteral("24"));
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("Q"));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QVERIFY(Compositor::self());
}

void SceneQPainterTest::testStartFrame()
{
    // this test verifies that the initial rendering is correct
    Compositor::self()->addRepaintFull();
    auto scene = qobject_cast<SceneQPainter*>(Compositor::self()->scene());
    QVERIFY(scene);
    QSignalSpy frameRenderedSpy(scene, &Scene::frameRendered);
    QVERIFY(frameRenderedSpy.isValid());
    QVERIFY(frameRenderedSpy.wait());
    // now let's render a reference image for comparison
    QImage referenceImage(QSize(1280, 1024), QImage::Format_RGB32);
    referenceImage.fill(Qt::black);
    QPainter p(&referenceImage);
    const QImage cursorImage = kwinApp()->platform()->softwareCursor();
    QVERIFY(!cursorImage.isNull());
    p.drawImage(KWin::Cursor::pos() - kwinApp()->platform()->softwareCursorHotspot(), cursorImage);
    QCOMPARE(referenceImage, *scene->backend()->buffer());
}

void SceneQPainterTest::testCursorMoving()
{
    // this test verifies that rendering is correct also after moving the cursor a few times
    auto scene = qobject_cast<SceneQPainter*>(Compositor::self()->scene());
    QVERIFY(scene);
    QSignalSpy frameRenderedSpy(scene, &Scene::frameRendered);
    QVERIFY(frameRenderedSpy.isValid());
    KWin::Cursor::setPos(0, 0);
    QVERIFY(frameRenderedSpy.wait());
    KWin::Cursor::setPos(10, 0);
    QVERIFY(frameRenderedSpy.wait());
    KWin::Cursor::setPos(10, 12);
    QVERIFY(frameRenderedSpy.wait());
    KWin::Cursor::setPos(12, 14);
    QVERIFY(frameRenderedSpy.wait());
    KWin::Cursor::setPos(50, 60);
    QVERIFY(frameRenderedSpy.wait());
    KWin::Cursor::setPos(45, 45);
    QVERIFY(frameRenderedSpy.wait());
    // now let's render a reference image for comparison
    QImage referenceImage(QSize(1280, 1024), QImage::Format_RGB32);
    referenceImage.fill(Qt::black);
    QPainter p(&referenceImage);
    const QImage cursorImage = kwinApp()->platform()->softwareCursor();
    QVERIFY(!cursorImage.isNull());
    p.drawImage(QPoint(45, 45) - kwinApp()->platform()->softwareCursorHotspot(), cursorImage);
    QCOMPARE(referenceImage, *scene->backend()->buffer());
}

WAYLANDTEST_MAIN(SceneQPainterTest)
#include "scene_qpainter_test.moc"
