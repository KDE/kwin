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
#include "abstract_backend.h"
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
    void testKeyboard();
};

class HelperWindow : public QRasterWindow
{
    Q_OBJECT
public:
    HelperWindow();
    ~HelperWindow();

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
    emit mouseMoved(event->globalPos());
}

void HelperWindow::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    emit mousePressed();
}

void HelperWindow::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
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
    waylandServer()->backend()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(waylandServer()->backend(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    waylandServer()->init(s_socketName.toLocal8Bit());

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
    waylandServer()->backend()->pointerMotion(QPoint(50, 50), timestamp++);
    QTRY_COMPARE(enterSpy.count(), 1);

    waylandServer()->backend()->pointerMotion(QPoint(60, 50), timestamp++);
    QTRY_COMPARE(moveSpy.count(), 1);
    QCOMPARE(moveSpy.first().first().toPoint(), QPoint(60, 50));

    waylandServer()->backend()->pointerMotion(QPoint(101, 50), timestamp++);
    QTRY_COMPARE(leaveSpy.count(), 1);
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
    waylandServer()->backend()->pointerMotion(QPoint(50, 50), timestamp++);

    waylandServer()->backend()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QTRY_COMPARE(pressSpy.count(), 1);
    waylandServer()->backend()->pointerButtonReleased(BTN_LEFT, timestamp++);
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
    waylandServer()->backend()->pointerMotion(QPoint(50, 50), timestamp++);

    waylandServer()->backend()->pointerAxisVertical(5.0, timestamp++);
    QTRY_COMPARE(wheelSpy.count(), 1);
    waylandServer()->backend()->pointerAxisHorizontal(5.0, timestamp++);
    QTRY_COMPARE(wheelSpy.count(), 2);
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
    waylandServer()->backend()->pointerMotion(QPoint(50, 50), timestamp++);

    waylandServer()->backend()->keyboardKeyPressed(KEY_A, timestamp++);
    QTRY_COMPARE(pressSpy.count(), 1);
    QCOMPARE(releaseSpy.count(), 0);
    waylandServer()->backend()->keyboardKeyReleased(KEY_A, timestamp++);
    QTRY_COMPARE(releaseSpy.count(), 1);
    QCOMPARE(pressSpy.count(), 1);
}

}

WAYLANDTEST_MAIN(KWin::InternalWindowTest)
#include "internal_window.moc"
