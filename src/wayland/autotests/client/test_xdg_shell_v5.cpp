/********************************************************************
Copyright 2016  Martin Gräßlin <mgraesslin@kde.org>
Copyright 2017  David Edmundson <davidedmundson@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "test_xdg_shell.h"

class XdgShellTestV5 : public XdgShellTest {
    Q_OBJECT
public:
    XdgShellTestV5() :
        XdgShellTest(KWayland::Server::XdgShellInterfaceVersion::UnstableV5) {}
private Q_SLOTS:
    void testPopup();
};

void XdgShellTestV5::testPopup()
{
    // this test verifies that the creation of popups works correctly
    SURFACE
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    QVERIFY(surfaceCreatedSpy.isValid());
    QSignalSpy xdgPopupSpy(m_xdgShellInterface, &XdgShellInterface::popupCreated);

    //check as well as the compat signal, the new signal is also fired
    QSignalSpy xdgPopupSpyNew(m_xdgShellInterface, &XdgShellInterface::xdgPopupCreated);


    QVERIFY(xdgPopupSpy.isValid());

    QScopedPointer<Surface> popupSurface(m_compositor->createSurface());
    QVERIFY(surfaceCreatedSpy.wait());

    // TODO: proper serial
    QScopedPointer<XdgShellPopup> xdgPopup(m_xdgShell->createPopup(popupSurface.data(), surface.data(), m_seat, 120, QPoint(10, 20)));
    QVERIFY(xdgPopupSpy.wait());
    QCOMPARE(xdgPopupSpy.count(), 1);
    QCOMPARE(xdgPopupSpyNew.count(), 1);


    QCOMPARE(xdgPopupSpy.first().at(1).value<SeatInterface*>(), m_seatInterface);
    QCOMPARE(xdgPopupSpy.first().at(2).value<quint32>(), 120u);
    auto serverXdgPopup = xdgPopupSpy.first().first().value<XdgShellPopupInterface*>();
    QVERIFY(serverXdgPopup);

    QCOMPARE(serverXdgPopup->surface(), surfaceCreatedSpy.first().first().value<SurfaceInterface*>());
    QCOMPARE(serverXdgPopup->transientFor().data(), serverXdgSurface->surface());
    QCOMPARE(serverXdgPopup->transientOffset(), QPoint(10, 20));

    // now also a popup for the popup
    QScopedPointer<Surface> popup2Surface(m_compositor->createSurface());
    QScopedPointer<XdgShellPopup> xdgPopup2(m_xdgShell->createPopup(popup2Surface.data(), popupSurface.data(), m_seat, 121, QPoint(5, 7)));
    QVERIFY(xdgPopupSpy.wait());
    QCOMPARE(xdgPopupSpy.count(), 2);
    QCOMPARE(xdgPopupSpy.last().at(1).value<SeatInterface*>(), m_seatInterface);
    QCOMPARE(xdgPopupSpy.last().at(2).value<quint32>(), 121u);
    auto serverXdgPopup2 = xdgPopupSpy.last().first().value<XdgShellPopupInterface*>();
    QVERIFY(serverXdgPopup2);

    QCOMPARE(serverXdgPopup2->surface(), surfaceCreatedSpy.last().first().value<SurfaceInterface*>());
    QCOMPARE(serverXdgPopup2->transientFor().data(), serverXdgPopup->surface());
    QCOMPARE(serverXdgPopup2->transientOffset(), QPoint(5, 7));

    QSignalSpy popup2DoneSpy(xdgPopup2.data(), &XdgShellPopup::popupDone);
    QVERIFY(popup2DoneSpy.isValid());
    serverXdgPopup2->popupDone();
    QVERIFY(popup2DoneSpy.wait());
    // TODO: test that this sends also the done to all parents
}




QTEST_GUILESS_MAIN(XdgShellTestV5)

#include "test_xdg_shell_v5.moc"

