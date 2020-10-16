/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <algorithm>

#include <QByteArray>
#include <QDir>
#include <QObject>
#include <QPair>
#include <QVector>

#include <KDecoration2/Decoration>
#include <KDecoration2/DecorationShadow>

#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/shadow.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWaylandServer/shadow_interface.h>
#include <KWaylandServer/surface_interface.h>

#include "kwin_wayland_test.h"

#include "abstract_client.h"
#include "composite.h"
#include "effect_builtins.h"
#include "effectloader.h"
#include "effects.h"
#include "platform.h"
#include "shadow.h"
#include "wayland_server.h"
#include "workspace.h"

Q_DECLARE_METATYPE(KWin::WindowQuadList);

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_scene_opengl_shadow-0");

class SceneOpenGLShadowTest : public QObject
{
    Q_OBJECT

public:
    SceneOpenGLShadowTest() {}

private Q_SLOTS:
    void initTestCase();
    void cleanup();

    void testShadowTileOverlaps_data();
    void testShadowTileOverlaps();
    void testNoCornerShadowTiles();
    void testDistributeHugeCornerTiles();

};

inline bool isClose(double a, double b, double eps = 1e-5)
{
    if (a == b) {
        return true;
    }
    const double diff = std::fabs(a - b);
    if (a == 0 || b == 0) {
        return diff < eps;
    }
    return diff / std::max(a, b) < eps;
}

inline bool compareQuads(const WindowQuad &a, const WindowQuad &b)
{
    for (int i = 0; i < 4; i++) {
        if (!isClose(a[i].x(), b[i].x())
                || !isClose(a[i].y(), b[i].y())
                || !isClose(a[i].u(), b[i].u())
                || !isClose(a[i].v(), b[i].v())) {
            return false;
        }
    }
    return true;
}

inline WindowQuad makeShadowQuad(const QRectF &geo, qreal tx1, qreal ty1, qreal tx2, qreal ty2)
{
    WindowQuad quad(WindowQuadShadow);
    quad[0] = WindowVertex(geo.left(),  geo.top(),    tx1, ty1);
    quad[1] = WindowVertex(geo.right(), geo.top(),    tx2, ty1);
    quad[2] = WindowVertex(geo.right(), geo.bottom(), tx2, ty2);
    quad[3] = WindowVertex(geo.left(),  geo.bottom(), tx1, ty2);
    return quad;
}

void SceneOpenGLShadowTest::initTestCase()
{
    // Copied from generic_scene_opengl_test.cpp

    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

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
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QVERIFY(KWin::Compositor::self());

    // Add directory with fake decorations to the plugin search path.
    QCoreApplication::addLibraryPath(
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("fakes")
    );

    // Change decoration theme.
    KConfigGroup group = kwinApp()->config()->group("org.kde.kdecoration2");
    group.writeEntry("library", "org.kde.test.fakedecowithshadows");
    group.sync();
    Workspace::self()->slotReconfigure();

    auto scene = KWin::Compositor::self()->scene();
    QVERIFY(scene);
    QCOMPARE(scene->compositingType(), KWin::OpenGL2Compositing);

}

void SceneOpenGLShadowTest::cleanup()
{
    Test::destroyWaylandConnection();
}

namespace {
    const int SHADOW_SIZE = 128;

    const int SHADOW_OFFSET_TOP  = 64;
    const int SHADOW_OFFSET_LEFT = 48;

    // NOTE: We assume deco shadows are generated with blur so that's
    //       why there is 4, 1 is the size of the inner shadow rect.
    const int SHADOW_TEXTURE_WIDTH  = 4 * SHADOW_SIZE + 1;
    const int SHADOW_TEXTURE_HEIGHT = 4 * SHADOW_SIZE + 1;

    const int SHADOW_PADDING_TOP    = SHADOW_SIZE - SHADOW_OFFSET_TOP;
    const int SHADOW_PADDING_RIGHT  = SHADOW_SIZE + SHADOW_OFFSET_LEFT;
    const int SHADOW_PADDING_BOTTOM = SHADOW_SIZE + SHADOW_OFFSET_TOP;
    const int SHADOW_PADDING_LEFT   = SHADOW_SIZE - SHADOW_OFFSET_LEFT;

    const QRectF SHADOW_INNER_RECT(2 * SHADOW_SIZE, 2 * SHADOW_SIZE, 1, 1);
}

void SceneOpenGLShadowTest::testShadowTileOverlaps_data()
{
    QTest::addColumn<QSize>("windowSize");
    QTest::addColumn<WindowQuadList>("expectedQuads");

    // Precompute shadow tile geometries(in texture's space).
    const QRectF topLeftTile(
        0,
        0,
        SHADOW_INNER_RECT.x(),
        SHADOW_INNER_RECT.y());
    const QRectF topRightTile(
        SHADOW_INNER_RECT.right(),
        0,
        SHADOW_TEXTURE_WIDTH - SHADOW_INNER_RECT.right(),
        SHADOW_INNER_RECT.y());
    const QRectF topTile(topLeftTile.topRight(), topRightTile.bottomLeft());

    const QRectF bottomLeftTile(
        0,
        SHADOW_INNER_RECT.bottom(),
        SHADOW_INNER_RECT.x(),
        SHADOW_TEXTURE_HEIGHT - SHADOW_INNER_RECT.bottom());
    const QRectF bottomRightTile(
        SHADOW_INNER_RECT.right(),
        SHADOW_INNER_RECT.bottom(),
        SHADOW_TEXTURE_WIDTH - SHADOW_INNER_RECT.right(),
        SHADOW_TEXTURE_HEIGHT - SHADOW_INNER_RECT.bottom());
    const QRectF bottomTile(bottomLeftTile.topRight(), bottomRightTile.bottomLeft());

    const QRectF leftTile(topLeftTile.bottomLeft(), bottomLeftTile.topRight());
    const QRectF rightTile(topRightTile.bottomLeft(), bottomRightTile.topRight());

    qreal tx1 = 0;
    qreal ty1 = 0;
    qreal tx2 = 0;
    qreal ty2 = 0;

    // Explanation behind numbers: (256+1 x 256+1) is the minimum window size
    // which doesn't cause overlapping of shadow tiles. For example, if a window
    // has (256 x 256+1) size, top-left and top-right or bottom-left and
    // bottom-right shadow tiles overlap.

    // No overlaps: In this case corner tiles are rendered as they are,
    // and top/right/bottom/left tiles are stretched.
    {
        const QSize windowSize(256 + 1, 256 + 1);
        WindowQuadList shadowQuads;

        const QRectF outerRect(
            -SHADOW_PADDING_LEFT,
            -SHADOW_PADDING_TOP,
            windowSize.width() + SHADOW_PADDING_LEFT + SHADOW_PADDING_RIGHT,
            windowSize.height() + SHADOW_PADDING_TOP + SHADOW_PADDING_BOTTOM);

        const QRectF topLeft(
            outerRect.left(),
            outerRect.top(),
            topLeftTile.width(),
            topLeftTile.height());
        tx1 = topLeftTile.left()   / SHADOW_TEXTURE_WIDTH;
        ty1 = topLeftTile.top()    / SHADOW_TEXTURE_HEIGHT;
        tx2 = topLeftTile.right()  / SHADOW_TEXTURE_WIDTH;
        ty2 = topLeftTile.bottom() / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(topLeft, tx1, ty1, tx2, ty2);

        const QRectF topRight(
            outerRect.right() - topRightTile.width(),
            outerRect.top(),
            topRightTile.width(),
            topRightTile.height());
        tx1 = topRightTile.left()   / SHADOW_TEXTURE_WIDTH;
        ty1 = topRightTile.top()    / SHADOW_TEXTURE_HEIGHT;
        tx2 = topRightTile.right()  / SHADOW_TEXTURE_WIDTH;
        ty2 = topRightTile.bottom() / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(topRight, tx1, ty1, tx2, ty2);

        const QRectF top(topLeft.topRight(), topRight.bottomLeft());
        tx1 = topTile.left()   / SHADOW_TEXTURE_WIDTH;
        ty1 = topTile.top()    / SHADOW_TEXTURE_HEIGHT;
        tx2 = topTile.right()  / SHADOW_TEXTURE_WIDTH;
        ty2 = topTile.bottom() / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(top, tx1, ty1, tx2, ty2);

        const QRectF bottomLeft(
            outerRect.left(),
            outerRect.bottom() - bottomLeftTile.height(),
            bottomLeftTile.width(),
            bottomLeftTile.height());
        tx1 = bottomLeftTile.left()   / SHADOW_TEXTURE_WIDTH;
        ty1 = bottomLeftTile.top()    / SHADOW_TEXTURE_HEIGHT;
        tx2 = bottomLeftTile.right()  / SHADOW_TEXTURE_WIDTH;
        ty2 = bottomLeftTile.bottom() / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(bottomLeft, tx1, ty1, tx2, ty2);

        const QRectF bottomRight(
            outerRect.right() - bottomRightTile.width(),
            outerRect.bottom() - bottomRightTile.height(),
            bottomRightTile.width(),
            bottomRightTile.height());
        tx1 = bottomRightTile.left()   / SHADOW_TEXTURE_WIDTH;
        ty1 = bottomRightTile.top()    / SHADOW_TEXTURE_HEIGHT;
        tx2 = bottomRightTile.right()  / SHADOW_TEXTURE_WIDTH;
        ty2 = bottomRightTile.bottom() / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(bottomRight, tx1, ty1, tx2, ty2);

        const QRectF bottom(bottomLeft.topRight(), bottomRight.bottomLeft());
        tx1 = bottomTile.left()   / SHADOW_TEXTURE_WIDTH;
        ty1 = bottomTile.top()    / SHADOW_TEXTURE_HEIGHT;
        tx2 = bottomTile.right()  / SHADOW_TEXTURE_WIDTH;
        ty2 = bottomTile.bottom() / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(bottom, tx1, ty1, tx2, ty2);

        const QRectF left(topLeft.bottomLeft(), bottomLeft.topRight());
        tx1 = leftTile.left()   / SHADOW_TEXTURE_WIDTH;
        ty1 = leftTile.top()    / SHADOW_TEXTURE_HEIGHT;
        tx2 = leftTile.right()  / SHADOW_TEXTURE_WIDTH;
        ty2 = leftTile.bottom() / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(left, tx1, ty1, tx2, ty2);

        const QRectF right(topRight.bottomLeft(), bottomRight.topRight());
        tx1 = rightTile.left()   / SHADOW_TEXTURE_WIDTH;
        ty1 = rightTile.top()    / SHADOW_TEXTURE_HEIGHT;
        tx2 = rightTile.right()  / SHADOW_TEXTURE_WIDTH;
        ty2 = rightTile.bottom() / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(right, tx1, ty1, tx2, ty2);

        QTest::newRow("no overlaps") << windowSize << shadowQuads;
    }

    // Top-Left & Bottom-Left/Top-Right & Bottom-Right overlap:
    // In this case overlapping parts are clipped and left/right
    // tiles aren't rendered.
    const QVector<QPair<QByteArray, QSize>> verticalOverlapTestTable {
        QPair<QByteArray, QSize> {
            QByteArray("top-left & bottom-left/top-right & bottom-right overlap"),
            QSize(256 + 1, 256)
        },
        QPair<QByteArray, QSize> {
            QByteArray("top-left & bottom-left/top-right & bottom-right overlap :: pre"),
            QSize(256 + 1, 256 - 1)
        }
        // No need to test the case when window size is QSize(256 + 1, 256 + 1).
        // It has been tested already (no overlaps test case).
    };

    for (auto const &tt : verticalOverlapTestTable) {
        const char *testName = tt.first.constData();
        const QSize windowSize = tt.second;

        WindowQuadList shadowQuads;
        qreal halfOverlap = 0.0;

        const QRectF outerRect(
            -SHADOW_PADDING_LEFT,
            -SHADOW_PADDING_TOP,
            windowSize.width() + SHADOW_PADDING_LEFT + SHADOW_PADDING_RIGHT,
            windowSize.height() + SHADOW_PADDING_TOP + SHADOW_PADDING_BOTTOM);

        QRectF topLeft(
            outerRect.left(),
            outerRect.top(),
            topLeftTile.width(),
            topLeftTile.height());

        QRectF bottomLeft(
            outerRect.left(),
            outerRect.bottom() - bottomLeftTile.height(),
            bottomLeftTile.width(),
            bottomLeftTile.height());

        halfOverlap = qAbs(topLeft.bottom() - bottomLeft.top()) / 2;
        topLeft.setBottom(topLeft.bottom() - halfOverlap);
        bottomLeft.setTop(bottomLeft.top() + halfOverlap);

        tx1 = topLeftTile.left()   / SHADOW_TEXTURE_WIDTH;
        ty1 = topLeftTile.top()    / SHADOW_TEXTURE_HEIGHT;
        tx2 = topLeftTile.right()  / SHADOW_TEXTURE_WIDTH;
        ty2 = topLeft.height()     / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(topLeft, tx1, ty1, tx2, ty2);

        tx1 = bottomLeftTile.left()      / SHADOW_TEXTURE_WIDTH;
        ty1 = 1.0 - (bottomLeft.height() / SHADOW_TEXTURE_HEIGHT);
        tx2 = bottomLeftTile.right()     / SHADOW_TEXTURE_WIDTH;
        ty2 = bottomLeftTile.bottom()    / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(bottomLeft, tx1, ty1, tx2, ty2);

        QRectF topRight(
            outerRect.right() - topRightTile.width(),
            outerRect.top(),
            topRightTile.width(),
            topRightTile.height());

        QRectF bottomRight(
            outerRect.right() - bottomRightTile.width(),
            outerRect.bottom() - bottomRightTile.height(),
            bottomRightTile.width(),
            bottomRightTile.height());

        halfOverlap = qAbs(topRight.bottom() - bottomRight.top()) / 2;
        topRight.setBottom(topRight.bottom() - halfOverlap);
        bottomRight.setTop(bottomRight.top() + halfOverlap);

        tx1 = topRightTile.left()   / SHADOW_TEXTURE_WIDTH;
        ty1 = topRightTile.top()    / SHADOW_TEXTURE_HEIGHT;
        tx2 = topRightTile.right()  / SHADOW_TEXTURE_WIDTH;
        ty2 = topRight.height()     / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(topRight, tx1, ty1, tx2, ty2);

        tx1 = bottomRightTile.left()      / SHADOW_TEXTURE_WIDTH;
        ty1 = 1.0 - (bottomRight.height() / SHADOW_TEXTURE_HEIGHT);
        tx2 = bottomRightTile.right()     / SHADOW_TEXTURE_WIDTH;
        ty2 = bottomRightTile.bottom()    / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(bottomRight, tx1, ty1, tx2, ty2);

        const QRectF top(topLeft.topRight(), topRight.bottomLeft());
        tx1 = topTile.left()  / SHADOW_TEXTURE_WIDTH;
        ty1 = topTile.top()   / SHADOW_TEXTURE_HEIGHT;
        tx2 = topTile.right() / SHADOW_TEXTURE_WIDTH;
        ty2 = top.height()    / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(top, tx1, ty1, tx2, ty2);

        const QRectF bottom(bottomLeft.topRight(), bottomRight.bottomLeft());
        tx1 = bottomTile.left()      / SHADOW_TEXTURE_WIDTH;
        ty1 = 1.0 - (bottom.height() / SHADOW_TEXTURE_HEIGHT);
        tx2 = bottomTile.right()     / SHADOW_TEXTURE_WIDTH;
        ty2 = bottomTile.bottom()    / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(bottom, tx1, ty1, tx2, ty2);

        QTest::newRow(testName) << windowSize << shadowQuads;
    }

    // Top-Left & Top-Right/Bottom-Left & Bottom-Right overlap:
    // In this case overlapping parts are clipped and top/bottom
    // tiles aren't rendered.
    const QVector<QPair<QByteArray, QSize>> horizontalOverlapTestTable {
        QPair<QByteArray, QSize> {
            QByteArray("top-left & top-right/bottom-left & bottom-right overlap"),
            QSize(256, 256 + 1)
        },
        QPair<QByteArray, QSize> {
            QByteArray("top-left & top-right/bottom-left & bottom-right overlap :: pre"),
            QSize(256 - 1, 256 + 1)
        }
        // No need to test the case when window size is QSize(256 + 1, 256 + 1).
        // It has been tested already (no overlaps test case).
    };

    for (auto const &tt : horizontalOverlapTestTable) {
        const char *testName = tt.first.constData();
        const QSize windowSize = tt.second;

        WindowQuadList shadowQuads;
        qreal halfOverlap = 0.0;

        const QRectF outerRect(
            -SHADOW_PADDING_LEFT,
            -SHADOW_PADDING_TOP,
            windowSize.width() + SHADOW_PADDING_LEFT + SHADOW_PADDING_RIGHT,
            windowSize.height() + SHADOW_PADDING_TOP + SHADOW_PADDING_BOTTOM);

        QRectF topLeft(
            outerRect.left(),
            outerRect.top(),
            topLeftTile.width(),
            topLeftTile.height());

        QRectF topRight(
            outerRect.right() - topRightTile.width(),
            outerRect.top(),
            topRightTile.width(),
            topRightTile.height());

        halfOverlap = qAbs(topLeft.right() - topRight.left()) / 2;
        topLeft.setRight(topLeft.right() - halfOverlap);
        topRight.setLeft(topRight.left() + halfOverlap);

        tx1 = topLeftTile.left()   / SHADOW_TEXTURE_WIDTH;
        ty1 = topLeftTile.top()    / SHADOW_TEXTURE_HEIGHT;
        tx2 = topLeft.width()      / SHADOW_TEXTURE_WIDTH;
        ty2 = topLeftTile.bottom() / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(topLeft, tx1, ty1, tx2, ty2);

        tx1 = 1.0 - (topRight.width() / SHADOW_TEXTURE_WIDTH);
        ty1 = topRightTile.top()      / SHADOW_TEXTURE_HEIGHT;
        tx2 = topRightTile.right()    / SHADOW_TEXTURE_WIDTH;
        ty2 = topRightTile.bottom()   / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(topRight, tx1, ty1, tx2, ty2);

        QRectF bottomLeft(
            outerRect.left(),
            outerRect.bottom() - bottomLeftTile.height(),
            bottomLeftTile.width(),
            bottomLeftTile.height());

        QRectF bottomRight(
            outerRect.right() - bottomRightTile.width(),
            outerRect.bottom() - bottomRightTile.height(),
            bottomRightTile.width(),
            bottomRightTile.height());

        halfOverlap = qAbs(bottomLeft.right() - bottomRight.left()) / 2;
        bottomLeft.setRight(bottomLeft.right() - halfOverlap);
        bottomRight.setLeft(bottomRight.left() + halfOverlap);

        tx1 = bottomLeftTile.left()   / SHADOW_TEXTURE_WIDTH;
        ty1 = bottomLeftTile.top()    / SHADOW_TEXTURE_HEIGHT;
        tx2 = bottomLeft.width()      / SHADOW_TEXTURE_WIDTH;
        ty2 = bottomLeftTile.bottom() / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(bottomLeft, tx1, ty1, tx2, ty2);

        tx1 = 1.0 - (bottomRight.width() / SHADOW_TEXTURE_WIDTH);
        ty1 = bottomRightTile.top()      / SHADOW_TEXTURE_HEIGHT;
        tx2 = bottomRightTile.right()    / SHADOW_TEXTURE_WIDTH;
        ty2 = bottomRightTile.bottom()   / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(bottomRight, tx1, ty1, tx2, ty2);

        const QRectF left(topLeft.bottomLeft(), bottomLeft.topRight());
        tx1 = leftTile.left()   / SHADOW_TEXTURE_WIDTH;
        ty1 = leftTile.top()    / SHADOW_TEXTURE_HEIGHT;
        tx2 = left.width()      / SHADOW_TEXTURE_WIDTH;
        ty2 = leftTile.bottom() / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(left, tx1, ty1, tx2, ty2);

        const QRectF right(topRight.bottomLeft(), bottomRight.topRight());
        tx1 = 1.0 - (right.width() / SHADOW_TEXTURE_WIDTH);
        ty1 = rightTile.top()      / SHADOW_TEXTURE_HEIGHT;
        tx2 = rightTile.right()    / SHADOW_TEXTURE_WIDTH;
        ty2 = rightTile.bottom()   / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(right, tx1, ty1, tx2, ty2);

        QTest::newRow(testName) << windowSize << shadowQuads;
    }

    // All shadow tiles overlap: In this case all overlapping parts
    // are clippend and top/right/bottom/left tiles aren't rendered.
    const QVector<QPair<QByteArray, QSize>> allOverlapTestTable {
        QPair<QByteArray, QSize> {
            QByteArray("all corner tiles overlap"),
            QSize(256, 256)
        },
        QPair<QByteArray, QSize> {
            QByteArray("all corner tiles overlap :: pre"),
            QSize(256 - 1, 256 - 1)
        }
        // No need to test the case when window size is QSize(256 + 1, 256 + 1).
        // It has been tested already (no overlaps test case).
    };

    for (auto const &tt : allOverlapTestTable) {
        const char *testName = tt.first.constData();
        const QSize windowSize = tt.second;

        WindowQuadList shadowQuads;
        qreal halfOverlap = 0.0;

        const QRectF outerRect(
            -SHADOW_PADDING_LEFT,
            -SHADOW_PADDING_TOP,
            windowSize.width() + SHADOW_PADDING_LEFT + SHADOW_PADDING_RIGHT,
            windowSize.height() + SHADOW_PADDING_TOP + SHADOW_PADDING_BOTTOM);

        QRectF topLeft(
            outerRect.left(),
            outerRect.top(),
            topLeftTile.width(),
            topLeftTile.height());

        QRectF topRight(
            outerRect.right() - topRightTile.width(),
            outerRect.top(),
            topRightTile.width(),
            topRightTile.height());

        QRectF bottomLeft(
            outerRect.left(),
            outerRect.bottom() - bottomLeftTile.height(),
            bottomLeftTile.width(),
            bottomLeftTile.height());

        QRectF bottomRight(
            outerRect.right() - bottomRightTile.width(),
            outerRect.bottom() - bottomRightTile.height(),
            bottomRightTile.width(),
            bottomRightTile.height());

        halfOverlap = qAbs(topLeft.right() - topRight.left()) / 2;
        topLeft.setRight(topLeft.right() - halfOverlap);
        topRight.setLeft(topRight.left() + halfOverlap);

        halfOverlap = qAbs(bottomLeft.right() - bottomRight.left()) / 2;
        bottomLeft.setRight(bottomLeft.right() - halfOverlap);
        bottomRight.setLeft(bottomRight.left() + halfOverlap);

        halfOverlap = qAbs(topLeft.bottom() - bottomLeft.top()) / 2;
        topLeft.setBottom(topLeft.bottom() - halfOverlap);
        bottomLeft.setTop(bottomLeft.top() + halfOverlap);

        halfOverlap = qAbs(topRight.bottom() - bottomRight.top()) / 2;
        topRight.setBottom(topRight.bottom() - halfOverlap);
        bottomRight.setTop(bottomRight.top() + halfOverlap);

        tx1 = topLeftTile.left() / SHADOW_TEXTURE_WIDTH;
        ty1 = topLeftTile.top()  / SHADOW_TEXTURE_HEIGHT;
        tx2 = topLeft.width()    / SHADOW_TEXTURE_WIDTH;
        ty2 = topLeft.height()   / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(topLeft, tx1, ty1, tx2, ty2);

        tx1 = 1.0 - (topRight.width() / SHADOW_TEXTURE_WIDTH);
        ty1 = topRightTile.top()      / SHADOW_TEXTURE_HEIGHT;
        tx2 = topRightTile.right()    / SHADOW_TEXTURE_WIDTH;
        ty2 = topRight.height()       / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(topRight, tx1, ty1, tx2, ty2);

        tx1 = bottomLeftTile.left()      / SHADOW_TEXTURE_WIDTH;
        ty1 = 1.0 - (bottomLeft.height() / SHADOW_TEXTURE_HEIGHT);
        tx2 = bottomLeft.width()         / SHADOW_TEXTURE_WIDTH;
        ty2 = bottomLeftTile.bottom()    / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(bottomLeft, tx1, ty1, tx2, ty2);

        tx1 = 1.0 - (bottomRight.width()  / SHADOW_TEXTURE_WIDTH);
        ty1 = 1.0 - (bottomRight.height() / SHADOW_TEXTURE_HEIGHT);
        tx2 = bottomRightTile.right()     / SHADOW_TEXTURE_WIDTH;
        ty2 = bottomRightTile.bottom()    / SHADOW_TEXTURE_HEIGHT;
        shadowQuads << makeShadowQuad(bottomRight, tx1, ty1, tx2, ty2);

        QTest::newRow(testName) << windowSize << shadowQuads;
    }

    // Window is too small: do not render any shadow tiles.
    {
        const QSize windowSize(1, 1);
        const WindowQuadList shadowQuads;

        QTest::newRow("window is too small") << windowSize << shadowQuads;
    }
}

void SceneOpenGLShadowTest::testShadowTileOverlaps()
{
    QFETCH(QSize, windowSize);
    QFETCH(WindowQuadList, expectedQuads);

    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Decoration));

    // Create a decorated client.
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QScopedPointer<ServerSideDecoration> ssd(Test::waylandServerSideDecoration()->create(surface.data()));

    auto *client = Test::renderAndWaitForShown(surface.data(), windowSize, Qt::blue);

    QSignalSpy sizeChangedSpy(shellSurface.data(), &XdgShellSurface::sizeChanged);
    QVERIFY(sizeChangedSpy.isValid());

    // Check the client is decorated.
    QVERIFY(client);
    QVERIFY(client->isDecorated());
    auto *decoration = client->decoration();
    QVERIFY(decoration);

    // If speciefied decoration theme is not found, KWin loads a default one
    // so we have to check whether a client has right decoration.
    auto decoShadow = decoration->shadow();
    QCOMPARE(decoShadow->shadow().size(), QSize(SHADOW_TEXTURE_WIDTH, SHADOW_TEXTURE_HEIGHT));
    QCOMPARE(decoShadow->paddingTop(),    SHADOW_PADDING_TOP);
    QCOMPARE(decoShadow->paddingRight(),  SHADOW_PADDING_RIGHT);
    QCOMPARE(decoShadow->paddingBottom(), SHADOW_PADDING_BOTTOM);
    QCOMPARE(decoShadow->paddingLeft(),   SHADOW_PADDING_LEFT);

    // Get shadow.
    QVERIFY(client->effectWindow());
    QVERIFY(client->effectWindow()->sceneWindow());
    QVERIFY(client->effectWindow()->sceneWindow()->shadow());
    auto *shadow = client->effectWindow()->sceneWindow()->shadow();

    // Validate shadow quads.
    const WindowQuadList &quads = shadow->shadowQuads();
    QCOMPARE(quads.size(), expectedQuads.size());

    QVector<bool> mask(expectedQuads.size(), false);
    for (const auto &q : quads) {
        for (int i = 0; i < expectedQuads.size(); i++) {
            if (!compareQuads(q, expectedQuads[i])) {
                continue;
            }
            if (!mask[i]) {
                mask[i] = true;
                break;
            } else {
                QFAIL("got a duplicate shadow quad");
            }
        }
    }

    for (const auto &v : qAsConst(mask)) {
        if (!v) {
            QFAIL("missed a shadow quad");
        }
    }
}

void SceneOpenGLShadowTest::testNoCornerShadowTiles()
{
    // this test verifies that top/right/bottom/left shadow tiles are
    // still drawn even when corner tiles are missing

    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::ShadowManager));

    // Create a surface.
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    auto *client = Test::renderAndWaitForShown(surface.data(), QSize(512, 512), Qt::blue);
    QVERIFY(client);
    QVERIFY(!client->isDecorated());

    // Render reference shadow texture with the following params:
    //  - shadow size: 128
    //  - inner rect size: 1
    //  - padding: 128
    QImage referenceShadowTexture(256 + 1, 256 + 1, QImage::Format_ARGB32_Premultiplied);
    referenceShadowTexture.fill(Qt::transparent);

    // We don't care about content of the shadow.

    // Submit the shadow to KWin.
    QScopedPointer<KWayland::Client::Shadow> clientShadow(Test::waylandShadowManager()->createShadow(surface.data()));
    QVERIFY(clientShadow->isValid());

    auto *shmPool = Test::waylandShmPool();

    Buffer::Ptr bufferTop = shmPool->createBuffer(
        referenceShadowTexture.copy(QRect(128, 0, 1, 128)));
    clientShadow->attachTop(bufferTop);

    Buffer::Ptr bufferRight = shmPool->createBuffer(
        referenceShadowTexture.copy(QRect(128 + 1, 128, 128, 1)));
    clientShadow->attachRight(bufferRight);

    Buffer::Ptr bufferBottom = shmPool->createBuffer(
        referenceShadowTexture.copy(QRect(128, 128 + 1, 1, 128)));
    clientShadow->attachBottom(bufferBottom);

    Buffer::Ptr bufferLeft = shmPool->createBuffer(
        referenceShadowTexture.copy(QRect(0, 128, 128, 1)));
    clientShadow->attachLeft(bufferLeft);

    clientShadow->setOffsets(QMarginsF(128, 128, 128, 128));

    QSignalSpy shadowChangedSpy(client->surface(), &KWaylandServer::SurfaceInterface::shadowChanged);
    QVERIFY(shadowChangedSpy.isValid());
    clientShadow->commit();
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(shadowChangedSpy.wait());

    // Check that we got right shadow from the client.
    QPointer<KWaylandServer::ShadowInterface> shadowIface = client->surface()->shadow();
    QVERIFY(!shadowIface.isNull());
    QCOMPARE(shadowIface->offset().left(),   128.0);
    QCOMPARE(shadowIface->offset().top(),    128.0);
    QCOMPARE(shadowIface->offset().right(),  128.0);
    QCOMPARE(shadowIface->offset().bottom(), 128.0);

    QVERIFY(client->effectWindow());
    QVERIFY(client->effectWindow()->sceneWindow());
    KWin::Shadow *shadow = client->effectWindow()->sceneWindow()->shadow();
    QVERIFY(shadow != nullptr);

    const WindowQuadList &quads = shadow->shadowQuads();
    QCOMPARE(quads.count(), 4);

    // Shadow size: 128
    // Padding: QMargins(128, 128, 128, 128)
    // Inner rect: QRect(128, 128, 1, 1)
    // Texture size: QSize(257, 257)
    // Window size: QSize(512, 512)
    WindowQuadList expectedQuads;
    expectedQuads << makeShadowQuad(QRectF(   0, -128, 512, 128), 128.0 / 257.0,           0.0, 129.0 / 257.0, 128.0 / 257.0); // top
    expectedQuads << makeShadowQuad(QRectF( 512,    0, 128, 512), 129.0 / 257.0, 128.0 / 257.0,           1.0, 129.0 / 257.0); // right
    expectedQuads << makeShadowQuad(QRectF(   0,  512, 512, 128), 128.0 / 257.0, 129.0 / 257.0, 129.0 / 257.0, 1.0);           // bottom
    expectedQuads << makeShadowQuad(QRectF(-128,    0, 128, 512),           0.0, 128.0 / 257.0, 128.0 / 257.0, 129.0 / 257.0); // left

    for (const WindowQuad &expectedQuad : expectedQuads) {
        auto it = std::find_if(quads.constBegin(), quads.constEnd(),
            [&expectedQuad](const WindowQuad &quad) {
                return compareQuads(quad, expectedQuad);
            });
        if (it == quads.constEnd()) {
            const QString message = QStringLiteral("Missing shadow quad (left: %1, top: %2, right: %3, bottom: %4)")
                .arg(expectedQuad.left())
                .arg(expectedQuad.top())
                .arg(expectedQuad.right())
                .arg(expectedQuad.bottom());
            const QByteArray rawMessage = message.toLocal8Bit().data();
            QFAIL(rawMessage.data());
        }
    }
}

void SceneOpenGLShadowTest::testDistributeHugeCornerTiles()
{
    // this test verifies that huge corner tiles are distributed correctly

    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::ShadowManager));

    // Create a surface.
    QScopedPointer<Surface> surface(Test::createSurface());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    auto *client = Test::renderAndWaitForShown(surface.data(), QSize(64, 64), Qt::blue);
    QVERIFY(client);
    QVERIFY(!client->isDecorated());

    // Submit the shadow to KWin.
    QScopedPointer<KWayland::Client::Shadow> clientShadow(Test::waylandShadowManager()->createShadow(surface.data()));
    QVERIFY(clientShadow->isValid());

    QImage referenceTileTexture(512, 512, QImage::Format_ARGB32_Premultiplied);
    referenceTileTexture.fill(Qt::transparent);

    auto *shmPool = Test::waylandShmPool();

    Buffer::Ptr bufferTopLeft = shmPool->createBuffer(referenceTileTexture);
    clientShadow->attachTopLeft(bufferTopLeft);

    Buffer::Ptr bufferTopRight = shmPool->createBuffer(referenceTileTexture);
    clientShadow->attachTopRight(bufferTopRight);

    clientShadow->setOffsets(QMarginsF(256, 256, 256, 0));

    QSignalSpy shadowChangedSpy(client->surface(), &KWaylandServer::SurfaceInterface::shadowChanged);
    QVERIFY(shadowChangedSpy.isValid());
    clientShadow->commit();
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(shadowChangedSpy.wait());

    // Check that we got right shadow from the client.
    QPointer<KWaylandServer::ShadowInterface> shadowIface = client->surface()->shadow();
    QVERIFY(!shadowIface.isNull());
    QCOMPARE(shadowIface->offset().left(),   256.0);
    QCOMPARE(shadowIface->offset().top(),    256.0);
    QCOMPARE(shadowIface->offset().right(),  256.0);
    QCOMPARE(shadowIface->offset().bottom(), 0.0);

    QVERIFY(client->effectWindow());
    QVERIFY(client->effectWindow()->sceneWindow());
    KWin::Shadow *shadow = client->effectWindow()->sceneWindow()->shadow();
    QVERIFY(shadow != nullptr);

    WindowQuadList expectedQuads;

    // Top-left quad
    expectedQuads << makeShadowQuad(
        QRectF(-256, -256, 256 + 32, 256 + 64),
        0.0, 0.0, (256.0 + 32.0) / 1024.0, (256.0 + 64.0) / 512.0);

    // Top-right quad
    expectedQuads << makeShadowQuad(
        QRectF(32, -256, 256 + 32, 256 + 64),
        1.0 - (256.0 + 32.0) / 1024.0, 0.0, 1.0, (256.0 + 64.0) / 512.0);

    const WindowQuadList &quads = shadow->shadowQuads();
    QCOMPARE(quads.count(), expectedQuads.count());

    for (const WindowQuad &expectedQuad : expectedQuads) {
        auto it = std::find_if(quads.constBegin(), quads.constEnd(),
            [&expectedQuad](const WindowQuad &quad) {
                return compareQuads(quad, expectedQuad);
            });
        if (it == quads.constEnd()) {
            const QString message = QStringLiteral("Missing shadow quad (left: %1, top: %2, right: %3, bottom: %4)")
                .arg(expectedQuad.left())
                .arg(expectedQuad.top())
                .arg(expectedQuad.right())
                .arg(expectedQuad.bottom());
            const QByteArray rawMessage = message.toLocal8Bit().data();
            QFAIL(rawMessage.data());
        }
    }
}

WAYLANDTEST_MAIN(SceneOpenGLShadowTest)
#include "scene_opengl_shadow_test.moc"
