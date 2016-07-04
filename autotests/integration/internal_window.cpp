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
#include "platform.h"
#include "cursor.h"
#include "shell_client.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <QPainter>
#include <QRasterWindow>

#include <linux/input.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_internal_window-0");

class InternalWindowTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void testEnterLeave();
    void testPointerPressRelease();
    void testPointerAxis();
    void testKeyboard_data();
    void testKeyboard();
    void testTouch();
};

class HelperWindow : public QRasterWindow
{
    Q_OBJECT
public:
    HelperWindow();
    ~HelperWindow();

    QPoint latestGlobalMousePos() const {
        return m_latestGlobalMousePos;
    }
    Qt::MouseButtons pressedButtons() const {
        return m_pressedButtons;
    }

Q_SIGNALS:
    void entered();
    void left();
    void mouseMoved(const QPoint &global);
    void mousePressed();
    void mouseReleased();
    void wheel();
    void keyPressed();
    void keyReleased();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    QPoint m_latestGlobalMousePos;
    Qt::MouseButtons m_pressedButtons = Qt::MouseButtons();
};

HelperWindow::HelperWindow()
    : QRasterWindow(nullptr)
{
}

HelperWindow::~HelperWindow() = default;

void HelperWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.fillRect(0, 0, width(), height(), Qt::red);
}

bool HelperWindow::event(QEvent *event)
{
    if (event->type() == QEvent::Enter) {
        emit entered();
    }
    if (event->type() == QEvent::Leave) {
        emit left();
    }
    return QRasterWindow::event(event);
}

void HelperWindow::mouseMoveEvent(QMouseEvent *event)
{
    m_latestGlobalMousePos = event->globalPos();
    emit mouseMoved(event->globalPos());
}

void HelperWindow::mousePressEvent(QMouseEvent *event)
{
    m_latestGlobalMousePos = event->globalPos();
    m_pressedButtons = event->buttons();
    emit mousePressed();
}

void HelperWindow::mouseReleaseEvent(QMouseEvent *event)
{
    m_latestGlobalMousePos = event->globalPos();
    m_pressedButtons = event->buttons();
    emit mouseReleased();
}

void HelperWindow::wheelEvent(QWheelEvent *event)
{
    Q_UNUSED(event)
    emit wheel();
}

void HelperWindow::keyPressEvent(QKeyEvent *event)
{
    Q_UNUSED(event)
    emit keyPressed();
}

void HelperWindow::keyReleaseEvent(QKeyEvent *event)
{
    Q_UNUSED(event)
    emit keyReleased();
}

void InternalWindowTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void InternalWindowTest::init()
{
    Cursor::setPos(QPoint(1280, 512));
}

void InternalWindowTest::testEnterLeave()
{
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();

    QVERIFY(clientAddedSpy.wait());
    QCOMPARE(clientAddedSpy.count(), 1);
    QVERIFY(!workspace()->activeClient());
    ShellClient *c = clientAddedSpy.first().first().value<ShellClient*>();
    QVERIFY(c->isInternal());
    QCOMPARE(c->geometry(), QRect(0, 0, 100, 100));

    QSignalSpy enterSpy(&win, &HelperWindow::entered);
    QVERIFY(enterSpy.isValid());
    QSignalSpy leaveSpy(&win, &HelperWindow::left);
    QVERIFY(leaveSpy.isValid());
    QSignalSpy moveSpy(&win, &HelperWindow::mouseMoved);
    QVERIFY(moveSpy.isValid());

    quint32 timestamp = 1;
    kwinApp()->platform()->pointerMotion(QPoint(50, 50), timestamp++);
    QTRY_COMPARE(enterSpy.count(), 1);

    kwinApp()->platform()->pointerMotion(QPoint(60, 50), timestamp++);
    QTRY_COMPARE(moveSpy.count(), 1);
    QCOMPARE(moveSpy.first().first().toPoint(), QPoint(60, 50));

    kwinApp()->platform()->pointerMotion(QPoint(101, 50), timestamp++);
    QTRY_COMPARE(leaveSpy.count(), 1);

    // set a mask on the window
    win.setMask(QRegion(10, 20, 30, 40));
    // outside the mask we should not get an enter
    kwinApp()->platform()->pointerMotion(QPoint(5, 5), timestamp++);
    QVERIFY(!enterSpy.wait(100));
    QCOMPARE(enterSpy.count(), 1);
    // inside the mask we should still get an enter
    kwinApp()->platform()->pointerMotion(QPoint(25, 27), timestamp++);
    QTRY_COMPARE(enterSpy.count(), 2);
}

void InternalWindowTest::testPointerPressRelease()
{
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QSignalSpy pressSpy(&win, &HelperWindow::mousePressed);
    QVERIFY(pressSpy.isValid());
    QSignalSpy releaseSpy(&win, &HelperWindow::mouseReleased);
    QVERIFY(releaseSpy.isValid());

    QVERIFY(clientAddedSpy.wait());
    QCOMPARE(clientAddedSpy.count(), 1);

    quint32 timestamp = 1;
    kwinApp()->platform()->pointerMotion(QPoint(50, 50), timestamp++);

    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QTRY_COMPARE(pressSpy.count(), 1);
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QTRY_COMPARE(releaseSpy.count(), 1);
}

void InternalWindowTest::testPointerAxis()
{
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QSignalSpy wheelSpy(&win, &HelperWindow::wheel);
    QVERIFY(wheelSpy.isValid());
    QVERIFY(clientAddedSpy.wait());
    QCOMPARE(clientAddedSpy.count(), 1);

    quint32 timestamp = 1;
    kwinApp()->platform()->pointerMotion(QPoint(50, 50), timestamp++);

    kwinApp()->platform()->pointerAxisVertical(5.0, timestamp++);
    QTRY_COMPARE(wheelSpy.count(), 1);
    kwinApp()->platform()->pointerAxisHorizontal(5.0, timestamp++);
    QTRY_COMPARE(wheelSpy.count(), 2);
}

void InternalWindowTest::testKeyboard_data()
{
    QTest::addColumn<QPoint>("cursorPos");

    QTest::newRow("on Window") << QPoint(50, 50);
    QTest::newRow("outside Window") << QPoint(250, 250);
}

void InternalWindowTest::testKeyboard()
{
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QSignalSpy pressSpy(&win, &HelperWindow::keyPressed);
    QVERIFY(pressSpy.isValid());
    QSignalSpy releaseSpy(&win, &HelperWindow::keyReleased);
    QVERIFY(releaseSpy.isValid());
    QVERIFY(clientAddedSpy.wait());
    QCOMPARE(clientAddedSpy.count(), 1);

    quint32 timestamp = 1;
    QFETCH(QPoint, cursorPos);
    kwinApp()->platform()->pointerMotion(cursorPos, timestamp++);

    kwinApp()->platform()->keyboardKeyPressed(KEY_A, timestamp++);
    QTRY_COMPARE(pressSpy.count(), 1);
    QCOMPARE(releaseSpy.count(), 0);
    kwinApp()->platform()->keyboardKeyReleased(KEY_A, timestamp++);
    QTRY_COMPARE(releaseSpy.count(), 1);
    QCOMPARE(pressSpy.count(), 1);
}

void InternalWindowTest::testTouch()
{
    // touch events for internal windows are emulated through mouse events
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    QVERIFY(clientAddedSpy.isValid());
    HelperWindow win;
    win.setGeometry(0, 0, 100, 100);
    win.show();
    QVERIFY(clientAddedSpy.wait());
    QCOMPARE(clientAddedSpy.count(), 1);

    QSignalSpy pressSpy(&win, &HelperWindow::mousePressed);
    QVERIFY(pressSpy.isValid());
    QSignalSpy releaseSpy(&win, &HelperWindow::mouseReleased);
    QVERIFY(releaseSpy.isValid());
    QSignalSpy moveSpy(&win, &HelperWindow::mouseMoved);
    QVERIFY(moveSpy.isValid());

    quint32 timestamp = 1;
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons());
    kwinApp()->platform()->touchDown(0, QPointF(50, 50), timestamp++);
    QCOMPARE(pressSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(50, 50));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons(Qt::LeftButton));

    // further touch down should not trigger
    kwinApp()->platform()->touchDown(1, QPointF(75, 75), timestamp++);
    QCOMPARE(pressSpy.count(), 1);
    kwinApp()->platform()->touchUp(1, timestamp++);
    QCOMPARE(releaseSpy.count(), 0);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(50, 50));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons(Qt::LeftButton));

    // another press
    kwinApp()->platform()->touchDown(1, QPointF(10, 10), timestamp++);
    QCOMPARE(pressSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(50, 50));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons(Qt::LeftButton));

    // simulate the move
    QCOMPARE(moveSpy.count(), 0);
    kwinApp()->platform()->touchMotion(0, QPointF(80, 90), timestamp++);
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(80, 90));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons(Qt::LeftButton));

    // move on other ID should not do anything
    kwinApp()->platform()->touchMotion(1, QPointF(20, 30), timestamp++);
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(80, 90));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons(Qt::LeftButton));

    // now up our main point
    kwinApp()->platform()->touchUp(0, timestamp++);
    QCOMPARE(releaseSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(80, 90));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons());

    // and up the additional point
    kwinApp()->platform()->touchUp(1, timestamp++);
    QCOMPARE(releaseSpy.count(), 1);
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(win.latestGlobalMousePos(), QPoint(80, 90));
    QCOMPARE(win.pressedButtons(), Qt::MouseButtons());
}

}

WAYLANDTEST_MAIN(KWin::InternalWindowTest)
#include "internal_window.moc"
