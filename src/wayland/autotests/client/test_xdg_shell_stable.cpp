/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "test_xdg_shell.h"
#include <wayland-xdg-shell-client-protocol.h>

class XdgShellTestStable : public XdgShellTest {
    Q_OBJECT
public:
    XdgShellTestStable() :
        XdgShellTest(KWaylandServer::XdgShellInterfaceVersion::Stable) {}

private Q_SLOTS:
    void testMaxSize();
    void testMinSize();

    void testPopup_data();
    void testPopup();

    void testMultipleRoles1();
    void testMultipleRoles2();

    void testWindowGeometry();
};

void XdgShellTestStable::testMaxSize()
{
    qRegisterMetaType<OutputInterface*>();
    // this test verifies changing the window maxSize
    QSignalSpy xdgSurfaceCreatedSpy(m_xdgShellInterface, &XdgShellInterface::surfaceCreated);
    QVERIFY(xdgSurfaceCreatedSpy.isValid());
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<XdgShellSurface> xdgSurface(m_xdgShell->createSurface(surface.data()));
    QVERIFY(xdgSurfaceCreatedSpy.wait());
    auto serverXdgSurface = xdgSurfaceCreatedSpy.first().first().value<XdgShellSurfaceInterface*>();
    QVERIFY(serverXdgSurface);

    QSignalSpy maxSizeSpy(serverXdgSurface, &XdgShellSurfaceInterface::maxSizeChanged);
    QVERIFY(maxSizeSpy.isValid());

    xdgSurface->setMaxSize(QSize(100, 100));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(maxSizeSpy.wait());
    QCOMPARE(maxSizeSpy.count(), 1);
    QCOMPARE(maxSizeSpy.last().at(0).value<QSize>(), QSize(100,100));
    QCOMPARE(serverXdgSurface->maximumSize(), QSize(100, 100));

    xdgSurface->setMaxSize(QSize(200, 200));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(maxSizeSpy.wait());
    QCOMPARE(maxSizeSpy.count(), 2);
    QCOMPARE(maxSizeSpy.last().at(0).value<QSize>(), QSize(200,200));
    QCOMPARE(serverXdgSurface->maximumSize(), QSize(200, 200));
}


void XdgShellTestStable::testPopup_data()
{
    QTest::addColumn<XdgPositioner>("positioner");
    XdgPositioner positioner(QSize(10,10), QRect(100,100,50,50));
    QTest::newRow("default") << positioner;

    XdgPositioner positioner2(QSize(20,20), QRect(101,102,51,52));
    QTest::newRow("sizeAndAnchorRect") << positioner2;

    positioner.setAnchorEdge(Qt::TopEdge | Qt::RightEdge);
    QTest::newRow("anchorEdge") << positioner;

    positioner.setGravity(Qt::BottomEdge);
    QTest::newRow("gravity") << positioner;

    positioner.setGravity(Qt::TopEdge | Qt::RightEdge);
    QTest::newRow("gravity2") << positioner;

    positioner.setConstraints(XdgPositioner::Constraint::SlideX | XdgPositioner::Constraint::FlipY);
    QTest::newRow("constraints") << positioner;

    positioner.setConstraints(XdgPositioner::Constraint::SlideX | XdgPositioner::Constraint::SlideY | XdgPositioner::Constraint::FlipX | XdgPositioner::Constraint::FlipY | XdgPositioner::Constraint::ResizeX | XdgPositioner::Constraint::ResizeY);
    QTest::newRow("constraints2") << positioner;

    positioner.setAnchorOffset(QPoint(4,5));
    QTest::newRow("offset") << positioner;
}

void XdgShellTestStable::testPopup()
{
    QSignalSpy xdgTopLevelCreatedSpy(m_xdgShellInterface, &XdgShellInterface::surfaceCreated);
    QSignalSpy xdgPopupCreatedSpy(m_xdgShellInterface, &XdgShellInterface::xdgPopupCreated);

    QScopedPointer<Surface> parentSurface(m_compositor->createSurface());
    QScopedPointer<XdgShellSurface> xdgParentSurface(m_xdgShell->createSurface(parentSurface.data()));

    QVERIFY(xdgTopLevelCreatedSpy.wait());
    auto serverXdgTopLevel = xdgTopLevelCreatedSpy.first().first().value<XdgShellSurfaceInterface*>();

    QFETCH(XdgPositioner, positioner);

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<XdgShellPopup> xdgSurface(m_xdgShell->createPopup(surface.data(), xdgParentSurface.data(), positioner));
    QVERIFY(xdgPopupCreatedSpy.wait());
    auto serverXdgPopup = xdgPopupCreatedSpy.first().first().value<XdgShellPopupInterface*>();
    QVERIFY(serverXdgPopup);

    QCOMPARE(serverXdgPopup->initialSize(), positioner.initialSize());
    QCOMPARE(serverXdgPopup->anchorRect(), positioner.anchorRect());
    QCOMPARE(serverXdgPopup->anchorEdge(), positioner.anchorEdge());
    QCOMPARE(serverXdgPopup->gravity(), positioner.gravity());
    QCOMPARE(serverXdgPopup->anchorOffset(), positioner.anchorOffset());
    //we have different enums for client server, but they share the same values
    QCOMPARE((int)serverXdgPopup->constraintAdjustments(), (int)positioner.constraints());
    QCOMPARE(serverXdgPopup->transientFor().data(), serverXdgTopLevel->surface());
}

void XdgShellTestStable::testMinSize()
{
    qRegisterMetaType<OutputInterface*>();
    // this test verifies changing the window minSize
    QSignalSpy xdgSurfaceCreatedSpy(m_xdgShellInterface, &XdgShellInterface::surfaceCreated);
    QVERIFY(xdgSurfaceCreatedSpy.isValid());
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<XdgShellSurface> xdgSurface(m_xdgShell->createSurface(surface.data()));
    QVERIFY(xdgSurfaceCreatedSpy.wait());
    auto serverXdgSurface = xdgSurfaceCreatedSpy.first().first().value<XdgShellSurfaceInterface*>();
    QVERIFY(serverXdgSurface);

    QSignalSpy minSizeSpy(serverXdgSurface, &XdgShellSurfaceInterface::minSizeChanged);
    QVERIFY(minSizeSpy.isValid());

    xdgSurface->setMinSize(QSize(200, 200));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(minSizeSpy.wait());
    QCOMPARE(minSizeSpy.count(), 1);
    QCOMPARE(minSizeSpy.last().at(0).value<QSize>(), QSize(200,200));
    QCOMPARE(serverXdgSurface->minimumSize(), QSize(200, 200));

    xdgSurface->setMinSize(QSize(100, 100));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(minSizeSpy.wait());
    QCOMPARE(minSizeSpy.count(), 2);
    QCOMPARE(minSizeSpy.last().at(0).value<QSize>(), QSize(100,100));
    QCOMPARE(serverXdgSurface->minimumSize(), QSize(100, 100));
}

//top level then toplevel
void XdgShellTestStable::testMultipleRoles1()
{
    //setting multiple roles on an xdg surface should fail
    QSignalSpy xdgSurfaceCreatedSpy(m_xdgShellInterface, &XdgShellInterface::surfaceCreated);
    QSignalSpy xdgPopupCreatedSpy(m_xdgShellInterface, &XdgShellInterface::xdgPopupCreated);

    QVERIFY(xdgSurfaceCreatedSpy.isValid());
    QVERIFY(xdgPopupCreatedSpy.isValid());

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    //This is testing we work when a client does something stupid
    //we can't use KWayland API here because by design that stops you from doing anything stupid

    qDebug() << (xdg_wm_base*)*m_xdgShell;
    auto xdgSurface = xdg_wm_base_get_xdg_surface(*m_xdgShell, *surface.data());

    //create a top level
    auto xdgTopLevel1 = xdg_surface_get_toplevel(xdgSurface);
    QVERIFY(xdgSurfaceCreatedSpy.wait());

    //now try to create another top level for the same xdg surface. It should fail
    auto xdgTopLevel2 = xdg_surface_get_toplevel(xdgSurface);
    QVERIFY(!xdgSurfaceCreatedSpy.wait(10));

    xdg_toplevel_destroy(xdgTopLevel1);
    xdg_toplevel_destroy(xdgTopLevel2);
    xdg_surface_destroy(xdgSurface);
}

//toplevel then popup
void XdgShellTestStable::testMultipleRoles2()
{
    QSignalSpy xdgSurfaceCreatedSpy(m_xdgShellInterface, &XdgShellInterface::surfaceCreated);
    QSignalSpy xdgPopupCreatedSpy(m_xdgShellInterface, &XdgShellInterface::xdgPopupCreated);

    QVERIFY(xdgSurfaceCreatedSpy.isValid());
    QVERIFY(xdgPopupCreatedSpy.isValid());

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<Surface> parentSurface(m_compositor->createSurface());

    auto parentXdgSurface = xdg_wm_base_get_xdg_surface(*m_xdgShell, *parentSurface.data());
    auto xdgTopLevelParent = xdg_surface_get_toplevel(parentXdgSurface);
    QVERIFY(xdgSurfaceCreatedSpy.wait());


    auto xdgSurface = xdg_wm_base_get_xdg_surface(*m_xdgShell, *surface.data());
    //create a top level
    auto xdgTopLevel1 = xdg_surface_get_toplevel(xdgSurface);
    QVERIFY(xdgSurfaceCreatedSpy.wait());

    //now try to create a popup on the same xdg surface. It should fail

    auto positioner = xdg_wm_base_create_positioner(*m_xdgShell);
    xdg_positioner_set_anchor_rect(positioner,10, 10, 10, 10);
    xdg_positioner_set_size(positioner,10, 100);

    auto xdgPopup2 = xdg_surface_get_popup(xdgSurface, parentXdgSurface, positioner);
    QVERIFY(!xdgPopupCreatedSpy.wait(10));

    xdg_positioner_destroy(positioner);
    xdg_toplevel_destroy(xdgTopLevel1);
    xdg_toplevel_destroy(xdgTopLevelParent);
    xdg_popup_destroy(xdgPopup2);
    xdg_surface_destroy(xdgSurface);
}

void XdgShellTestStable::testWindowGeometry()
{
    SURFACE
    QSignalSpy windowGeometryChangedSpy(serverXdgSurface, &XdgShellSurfaceInterface::windowGeometryChanged);
    xdgSurface->setWindowGeometry(QRect(50, 50, 400, 400));
    surface->commit(Surface::CommitFlag::None);
    QVERIFY(windowGeometryChangedSpy.wait());
    QCOMPARE(serverXdgSurface->windowGeometry(), QRect(50, 50, 400, 400));

    //add a popup to this surface
    XdgPositioner positioner(QSize(10,10), QRect(100,100,50,50));
    QSignalSpy xdgPopupCreatedSpy(m_xdgShellInterface, &XdgShellInterface::xdgPopupCreated);
    QScopedPointer<Surface> popupSurface(m_compositor->createSurface());
    QScopedPointer<XdgShellPopup> xdgPopupSurface(m_xdgShell->createPopup(popupSurface.data(), xdgSurface.data(), positioner));
    QVERIFY(xdgPopupCreatedSpy.wait());
    auto serverXdgPopup = xdgPopupCreatedSpy.first().first().value<XdgShellPopupInterface*>();
    QVERIFY(serverXdgPopup);

    QSignalSpy popupWindowGeometryChangedSpy(serverXdgPopup, &XdgShellPopupInterface::windowGeometryChanged);
    xdgPopupSurface->setWindowGeometry(QRect(60, 60, 300, 300));
    popupSurface->commit(Surface::CommitFlag::None);
    QVERIFY(popupWindowGeometryChangedSpy.wait());
    QCOMPARE(serverXdgPopup->windowGeometry(), QRect(60, 60, 300, 300));
}


QTEST_GUILESS_MAIN(XdgShellTestStable)

#include "test_xdg_shell_stable.moc"

