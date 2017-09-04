#include "test_xdg_shell.h"
#include <wayland-xdg-shell-v6-client-protocol.h>

class XdgShellTestV6 : public XdgShellTest {
    Q_OBJECT
public:
    XdgShellTestV6() :
        XdgShellTest(KWayland::Server::XdgShellInterfaceVersion::UnstableV6) {}

private Q_SLOTS:
    void testMaxSize();
    void testMinSize();

    void testPopup_data();
    void testPopup();

    void testMultipleRoles1();
    void testMultipleRoles2();
};

void XdgShellTestV6::testMaxSize()
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
    QVERIFY(maxSizeSpy.wait());
    QCOMPARE(maxSizeSpy.count(), 1);
    QCOMPARE(maxSizeSpy.last().at(0).value<QSize>(), QSize(100,100));

    xdgSurface->setMaxSize(QSize(200, 200));
    QVERIFY(maxSizeSpy.wait());
    QCOMPARE(maxSizeSpy.count(), 2);
    QCOMPARE(maxSizeSpy.last().at(0).value<QSize>(), QSize(200,200));
}


void XdgShellTestV6::testPopup_data()
{
    QTest::addColumn<XdgPositioner>("positioners");
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

void XdgShellTestV6::testPopup()
{
    QSignalSpy xdgTopLevelCreatedSpy(m_xdgShellInterface, &XdgShellInterface::surfaceCreated);
    QSignalSpy xdgPopupCreatedSpy(m_xdgShellInterface, &XdgShellInterface::xdgPopupCreated);

    QScopedPointer<Surface> parentSurface(m_compositor->createSurface());
    QScopedPointer<XdgShellSurface> xdgParentSurface(m_xdgShell->createSurface(parentSurface.data()));

    QVERIFY(xdgTopLevelCreatedSpy.wait());
    auto serverXdgTopLevel = xdgTopLevelCreatedSpy.first().first().value<XdgShellSurfaceInterface*>();

    XdgPositioner positioner(QSize(10,10), QRect(100,100,50,50));

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

void XdgShellTestV6::testMinSize()
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
    QVERIFY(minSizeSpy.wait());
    QCOMPARE(minSizeSpy.count(), 1);
    QCOMPARE(minSizeSpy.last().at(0).value<QSize>(), QSize(200,200));

    xdgSurface->setMinSize(QSize(100, 100));
    QVERIFY(minSizeSpy.wait());
    QCOMPARE(minSizeSpy.count(), 2);
    QCOMPARE(minSizeSpy.last().at(0).value<QSize>(), QSize(100,100));
}

//top level then toplevel
void XdgShellTestV6::testMultipleRoles1()
{
    //setting multiple roles on an xdg surface should fail
    QSignalSpy xdgSurfaceCreatedSpy(m_xdgShellInterface, &XdgShellInterface::surfaceCreated);
    QSignalSpy xdgPopupCreatedSpy(m_xdgShellInterface, &XdgShellInterface::xdgPopupCreated);

    QVERIFY(xdgSurfaceCreatedSpy.isValid());
    QVERIFY(xdgPopupCreatedSpy.isValid());

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    //This is testing we work when a client does something stupid
    //we can't use KWayland API here because by design that stops you from doing anything stupid
    auto xdgSurface = zxdg_shell_v6_get_xdg_surface(*m_xdgShell, *surface.data());

    //create a top level
    auto xdgTopLevel1 = zxdg_surface_v6_get_toplevel(xdgSurface);
    QVERIFY(xdgSurfaceCreatedSpy.wait());

    //now try to create another top level for the same xdg surface. It should fail
    auto xdgTopLevel2 = zxdg_surface_v6_get_toplevel(xdgSurface);
    QVERIFY(!xdgSurfaceCreatedSpy.wait(10));

    zxdg_toplevel_v6_destroy(xdgTopLevel1);
    zxdg_toplevel_v6_destroy(xdgTopLevel2);
    zxdg_surface_v6_destroy(xdgSurface);
}

//toplevel then popup
void XdgShellTestV6::testMultipleRoles2()
{
    QSignalSpy xdgSurfaceCreatedSpy(m_xdgShellInterface, &XdgShellInterface::surfaceCreated);
    QSignalSpy xdgPopupCreatedSpy(m_xdgShellInterface, &XdgShellInterface::xdgPopupCreated);

    QVERIFY(xdgSurfaceCreatedSpy.isValid());
    QVERIFY(xdgPopupCreatedSpy.isValid());

    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QScopedPointer<Surface> parentSurface(m_compositor->createSurface());

    auto parentXdgSurface = zxdg_shell_v6_get_xdg_surface(*m_xdgShell, *parentSurface.data());
    auto xdgTopLevelParent = zxdg_surface_v6_get_toplevel(parentXdgSurface);
    QVERIFY(xdgSurfaceCreatedSpy.wait());


    auto xdgSurface = zxdg_shell_v6_get_xdg_surface(*m_xdgShell, *surface.data());
    //create a top level
    auto xdgTopLevel1 = zxdg_surface_v6_get_toplevel(xdgSurface);
    QVERIFY(xdgSurfaceCreatedSpy.wait());

    //now try to create a popup on the same xdg surface. It should fail

    auto positioner = zxdg_shell_v6_create_positioner(*m_xdgShell);
    zxdg_positioner_v6_set_anchor_rect(positioner,10, 10, 10, 10);
    zxdg_positioner_v6_set_size(positioner,10, 100);

    auto xdgPopup2 = zxdg_surface_v6_get_popup(xdgSurface, parentXdgSurface, positioner);
    QVERIFY(!xdgPopupCreatedSpy.wait(10));

    zxdg_positioner_v6_destroy(positioner);
    zxdg_toplevel_v6_destroy(xdgTopLevel1);
    zxdg_toplevel_v6_destroy(xdgTopLevelParent);
    zxdg_popup_v6_destroy(xdgPopup2);
    zxdg_surface_v6_destroy(xdgSurface);
}


QTEST_GUILESS_MAIN(XdgShellTestV6)

#include "test_xdg_shell_v6.moc"

