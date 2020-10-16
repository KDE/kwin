/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <algorithm>
#include <cmath>

#include <QByteArray>
#include <QDir>
#include <QImage>
#include <QMarginsF>
#include <QObject>
#include <QPainter>
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
#include "plugins/scenes/qpainter/scene_qpainter.h"
#include "shadow.h"
#include "wayland_server.h"
#include "workspace.h"

Q_DECLARE_METATYPE(KWin::WindowQuadList)

using namespace KWin;
using namespace KWayland::Client;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_scene_qpainter_shadow-0");

class SceneQPainterShadowTest : public QObject
{
    Q_OBJECT

public:
    SceneQPainterShadowTest() {}

private Q_SLOTS:
    void initTestCase();
    void cleanup();

    void testShadowTileOverlaps_data();
    void testShadowTileOverlaps();
    void testShadowTextureReconstruction();

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
                || !isClose(a[i].textureX(), b[i].textureX())
                || !isClose(a[i].textureY(), b[i].textureY())) {
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

void SceneQPainterShadowTest::initTestCase()
{
    // Copied from scene_qpainter_test.cpp

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

    if (!QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("icons/DMZ-White/index.theme")).isEmpty()) {
        qputenv("XCURSOR_THEME", QByteArrayLiteral("DMZ-White"));
    } else {
        // might be vanilla-dmz (e.g. Arch, FreeBSD)
        qputenv("XCURSOR_THEME", QByteArrayLiteral("Vanilla-DMZ"));
    }
    qputenv("XCURSOR_SIZE", QByteArrayLiteral("24"));
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("Q"));

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
}

void SceneQPainterShadowTest::cleanup()
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

void SceneQPainterShadowTest::testShadowTileOverlaps_data()
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
        tx1 = topLeftTile.left();
        ty1 = topLeftTile.top();
        tx2 = topLeftTile.right();
        ty2 = topLeftTile.bottom();
        shadowQuads << makeShadowQuad(topLeft, tx1, ty1, tx2, ty2);

        const QRectF topRight(
            outerRect.right() - topRightTile.width(),
            outerRect.top(),
            topRightTile.width(),
            topRightTile.height());
        tx1 = topRightTile.left();
        ty1 = topRightTile.top();
        tx2 = topRightTile.right();
        ty2 = topRightTile.bottom();
        shadowQuads << makeShadowQuad(topRight, tx1, ty1, tx2, ty2);

        const QRectF top(topLeft.topRight(), topRight.bottomLeft());
        tx1 = topTile.left();
        ty1 = topTile.top();
        tx2 = topTile.right();
        ty2 = topTile.bottom();
        shadowQuads << makeShadowQuad(top, tx1, ty1, tx2, ty2);

        const QRectF bottomLeft(
            outerRect.left(),
            outerRect.bottom() - bottomLeftTile.height(),
            bottomLeftTile.width(),
            bottomLeftTile.height());
        tx1 = bottomLeftTile.left();
        ty1 = bottomLeftTile.top();
        tx2 = bottomLeftTile.right();
        ty2 = bottomLeftTile.bottom();
        shadowQuads << makeShadowQuad(bottomLeft, tx1, ty1, tx2, ty2);

        const QRectF bottomRight(
            outerRect.right() - bottomRightTile.width(),
            outerRect.bottom() - bottomRightTile.height(),
            bottomRightTile.width(),
            bottomRightTile.height());
        tx1 = bottomRightTile.left();
        ty1 = bottomRightTile.top();
        tx2 = bottomRightTile.right();
        ty2 = bottomRightTile.bottom();
        shadowQuads << makeShadowQuad(bottomRight, tx1, ty1, tx2, ty2);

        const QRectF bottom(bottomLeft.topRight(), bottomRight.bottomLeft());
        tx1 = bottomTile.left();
        ty1 = bottomTile.top();
        tx2 = bottomTile.right();
        ty2 = bottomTile.bottom();
        shadowQuads << makeShadowQuad(bottom, tx1, ty1, tx2, ty2);

        const QRectF left(topLeft.bottomLeft(), bottomLeft.topRight());
        tx1 = leftTile.left();
        ty1 = leftTile.top();
        tx2 = leftTile.right();
        ty2 = leftTile.bottom();
        shadowQuads << makeShadowQuad(left, tx1, ty1, tx2, ty2);

        const QRectF right(topRight.bottomLeft(), bottomRight.topRight());
        tx1 = rightTile.left();
        ty1 = rightTile.top();
        tx2 = rightTile.right();
        ty2 = rightTile.bottom();
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
        topLeft.setBottom(topLeft.bottom() - std::floor(halfOverlap));
        bottomLeft.setTop(bottomLeft.top() + std::ceil(halfOverlap));

        tx1 = topLeftTile.left();
        ty1 = topLeftTile.top();
        tx2 = topLeftTile.right();
        ty2 = topLeft.height();
        shadowQuads << makeShadowQuad(topLeft, tx1, ty1, tx2, ty2);

        tx1 = bottomLeftTile.left();
        ty1 = SHADOW_TEXTURE_HEIGHT - bottomLeft.height();
        tx2 = bottomLeftTile.right();
        ty2 = bottomLeftTile.bottom();
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
        topRight.setBottom(topRight.bottom() - std::floor(halfOverlap));
        bottomRight.setTop(bottomRight.top() + std::ceil(halfOverlap));

        tx1 = topRightTile.left();
        ty1 = topRightTile.top();
        tx2 = topRightTile.right();
        ty2 = topRight.height();
        shadowQuads << makeShadowQuad(topRight, tx1, ty1, tx2, ty2);

        tx1 = bottomRightTile.left();
        ty1 = SHADOW_TEXTURE_HEIGHT - bottomRight.height();
        tx2 = bottomRightTile.right();
        ty2 = bottomRightTile.bottom();
        shadowQuads << makeShadowQuad(bottomRight, tx1, ty1, tx2, ty2);

        const QRectF top(topLeft.topRight(), topRight.bottomLeft());
        tx1 = topTile.left();
        ty1 = topTile.top();
        tx2 = topTile.right();
        ty2 = top.height();
        shadowQuads << makeShadowQuad(top, tx1, ty1, tx2, ty2);

        const QRectF bottom(bottomLeft.topRight(), bottomRight.bottomLeft());
        tx1 = bottomTile.left();
        ty1 = SHADOW_TEXTURE_HEIGHT - bottom.height();
        tx2 = bottomTile.right();
        ty2 = bottomTile.bottom();
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
        topLeft.setRight(topLeft.right() - std::floor(halfOverlap));
        topRight.setLeft(topRight.left() + std::ceil(halfOverlap));

        tx1 = topLeftTile.left();
        ty1 = topLeftTile.top();
        tx2 = topLeft.width();
        ty2 = topLeftTile.bottom();
        shadowQuads << makeShadowQuad(topLeft, tx1, ty1, tx2, ty2);

        tx1 = SHADOW_TEXTURE_WIDTH - topRight.width();
        ty1 = topRightTile.top();
        tx2 = topRightTile.right();
        ty2 = topRightTile.bottom();
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
        bottomLeft.setRight(bottomLeft.right() - std::floor(halfOverlap));
        bottomRight.setLeft(bottomRight.left() + std::ceil(halfOverlap));

        tx1 = bottomLeftTile.left();
        ty1 = bottomLeftTile.top();
        tx2 = bottomLeft.width();
        ty2 = bottomLeftTile.bottom();
        shadowQuads << makeShadowQuad(bottomLeft, tx1, ty1, tx2, ty2);

        tx1 = SHADOW_TEXTURE_WIDTH - bottomRight.width();
        ty1 = bottomRightTile.top();
        tx2 = bottomRightTile.right();
        ty2 = bottomRightTile.bottom();
        shadowQuads << makeShadowQuad(bottomRight, tx1, ty1, tx2, ty2);

        const QRectF left(topLeft.bottomLeft(), bottomLeft.topRight());
        tx1 = leftTile.left();
        ty1 = leftTile.top();
        tx2 = left.width();
        ty2 = leftTile.bottom();
        shadowQuads << makeShadowQuad(left, tx1, ty1, tx2, ty2);

        const QRectF right(topRight.bottomLeft(), bottomRight.topRight());
        tx1 = SHADOW_TEXTURE_WIDTH - right.width();
        ty1 = rightTile.top();
        tx2 = rightTile.right();
        ty2 = rightTile.bottom();
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
        topLeft.setRight(topLeft.right() - std::floor(halfOverlap));
        topRight.setLeft(topRight.left() + std::ceil(halfOverlap));

        halfOverlap = qAbs(bottomLeft.right() - bottomRight.left()) / 2;
        bottomLeft.setRight(bottomLeft.right() - std::floor(halfOverlap));
        bottomRight.setLeft(bottomRight.left() + std::ceil(halfOverlap));

        halfOverlap = qAbs(topLeft.bottom() - bottomLeft.top()) / 2;
        topLeft.setBottom(topLeft.bottom() - std::floor(halfOverlap));
        bottomLeft.setTop(bottomLeft.top() + std::ceil(halfOverlap));

        halfOverlap = qAbs(topRight.bottom() - bottomRight.top()) / 2;
        topRight.setBottom(topRight.bottom() - std::floor(halfOverlap));
        bottomRight.setTop(bottomRight.top() + std::ceil(halfOverlap));

        tx1 = topLeftTile.left();
        ty1 = topLeftTile.top();
        tx2 = topLeft.width();
        ty2 = topLeft.height();
        shadowQuads << makeShadowQuad(topLeft, tx1, ty1, tx2, ty2);

        tx1 = SHADOW_TEXTURE_WIDTH - topRight.width();
        ty1 = topRightTile.top();
        tx2 = topRightTile.right();
        ty2 = topRight.height();
        shadowQuads << makeShadowQuad(topRight, tx1, ty1, tx2, ty2);

        tx1 = bottomLeftTile.left();
        ty1 = SHADOW_TEXTURE_HEIGHT - bottomLeft.height();
        tx2 = bottomLeft.width();
        ty2 = bottomLeftTile.bottom();
        shadowQuads << makeShadowQuad(bottomLeft, tx1, ty1, tx2, ty2);

        tx1 = SHADOW_TEXTURE_WIDTH - bottomRight.width();
        ty1 = SHADOW_TEXTURE_HEIGHT - bottomRight.height();
        tx2 = bottomRightTile.right();
        ty2 = bottomRightTile.bottom();
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

void SceneQPainterShadowTest::testShadowTileOverlaps()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Decoration));

    QFETCH(QSize, windowSize);
    QFETCH(WindowQuadList, expectedQuads);

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

void SceneQPainterShadowTest::testShadowTextureReconstruction()
{
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
    QImage referenceShadowTexture(QSize(256 + 1, 256 + 1), QImage::Format_ARGB32_Premultiplied);
    referenceShadowTexture.fill(Qt::transparent);

    QPainter painter(&referenceShadowTexture);
    painter.fillRect(QRect(10, 10, 192, 200), QColor(255, 0, 0, 128));
    painter.fillRect(QRect(128, 30, 10, 180), QColor(0, 0, 0, 30));
    painter.fillRect(QRect(20, 140, 160, 10), QColor(0, 255, 0, 128));

    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    painter.fillRect(QRect(128, 128, 1, 1), Qt::black);
    painter.end();

    // Create shadow.
    QScopedPointer<KWayland::Client::Shadow> clientShadow(Test::waylandShadowManager()->createShadow(surface.data()));
    QVERIFY(clientShadow->isValid());

    auto *shmPool = Test::waylandShmPool();

    Buffer::Ptr bufferTopLeft = shmPool->createBuffer(
        referenceShadowTexture.copy(QRect(0, 0, 128, 128)));
    clientShadow->attachTopLeft(bufferTopLeft);

    Buffer::Ptr bufferTop = shmPool->createBuffer(
        referenceShadowTexture.copy(QRect(128, 0, 1, 128)));
    clientShadow->attachTop(bufferTop);

    Buffer::Ptr bufferTopRight = shmPool->createBuffer(
        referenceShadowTexture.copy(QRect(128 + 1, 0, 128, 128)));
    clientShadow->attachTopRight(bufferTopRight);

    Buffer::Ptr bufferRight = shmPool->createBuffer(
        referenceShadowTexture.copy(QRect(128 + 1, 128, 128, 1)));
    clientShadow->attachRight(bufferRight);

    Buffer::Ptr bufferBottomRight = shmPool->createBuffer(
        referenceShadowTexture.copy(QRect(128 + 1, 128 + 1, 128, 128)));
    clientShadow->attachBottomRight(bufferBottomRight);

    Buffer::Ptr bufferBottom = shmPool->createBuffer(
        referenceShadowTexture.copy(QRect(128, 128 + 1, 1, 128)));
    clientShadow->attachBottom(bufferBottom);

    Buffer::Ptr bufferBottomLeft = shmPool->createBuffer(
        referenceShadowTexture.copy(QRect(0, 128 + 1, 128, 128)));
    clientShadow->attachBottomLeft(bufferBottomLeft);

    Buffer::Ptr bufferLeft = shmPool->createBuffer(
        referenceShadowTexture.copy(QRect(0, 128, 128, 1)));
    clientShadow->attachLeft(bufferLeft);

    clientShadow->setOffsets(QMarginsF(128, 128, 128, 128));

    // Commit shadow.
    QSignalSpy shadowChangedSpy(client->surface(), &KWaylandServer::SurfaceInterface::shadowChanged);
    QVERIFY(shadowChangedSpy.isValid());
    clientShadow->commit();
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(shadowChangedSpy.wait());

    // Check whether we've got right shadow.
    auto shadowIface = client->surface()->shadow();
    QVERIFY(!shadowIface.isNull());
    QCOMPARE(shadowIface->offset().left(),   128.0);
    QCOMPARE(shadowIface->offset().top(),    128.0);
    QCOMPARE(shadowIface->offset().right(),  128.0);
    QCOMPARE(shadowIface->offset().bottom(), 128.0);

    // Get SceneQPainterShadow's texture.
    QVERIFY(client->effectWindow());
    QVERIFY(client->effectWindow()->sceneWindow());
    QVERIFY(client->effectWindow()->sceneWindow()->shadow());
    auto &shadowTexture = static_cast<SceneQPainterShadow *>(client->effectWindow()->sceneWindow()->shadow())->shadowTexture();

    QCOMPARE(shadowTexture, referenceShadowTexture);
}

WAYLANDTEST_MAIN(SceneQPainterShadowTest)
#include "scene_qpainter_shadow_test.moc"
