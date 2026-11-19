/*
    SPDX-FileCopyrightText: 2024 Xaver Hugl <xaver.hugl@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"
#include "utils/memorymap.h"
#include "wayland_server.h"
#include "workspace.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>

namespace KWin
{

class ScreenshotTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void screenshot();
};

void ScreenshotTest::initTestCase()
{
    QVERIFY(waylandServer()->init(qAppName()));
    kwinApp()->start();
    Test::setOutputConfig({
        Rect(0, 0, 1280, 1024),
    });
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 1);
    QCOMPARE(outputs[0]->geometry(), Rect(0, 0, 1280, 1024));
}

void ScreenshotTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::PresentationTime));
}

void ScreenshotTest::cleanup()
{
    Test::destroyWaylandConnection();
}

static const QString s_screenShotService = QStringLiteral("org.kde.KWin.ScreenShot3");
static const QString s_screenShotObjectPath = QStringLiteral("/org/kde/KWin/ScreenShot3");
static const QString s_screenShotInterface = QStringLiteral("org.kde.KWin.ScreenShot3");

void ScreenshotTest::screenshot()
{
    LogicalOutput *const output = workspace()->outputs().front();

    QImage testImage(output->pixelSize(), QImage::Format_RGB32);
    testImage.fill(Qt::blue);
    testImage.setPixel(0, 0, qRgba(50, 123, 321, 128));
    testImage.setPixel(output->pixelSize().width() - 1, output->pixelSize().height() - 1, qRgba(244, 321, 123, 64));

    Test::XdgToplevelWindow window{[](Test::XdgToplevel *toplevel) {
        toplevel->set_fullscreen(nullptr);
    }};
    QVERIFY(window.show(testImage));
    QVERIFY(window.presentWait());

    const QString mode = QStringLiteral("CaptureScreen");
    const QString screen = output->name();
    const QVariantMap options = {};

    QDBusMessage message = QDBusMessage::createMethodCall(s_screenShotService, s_screenShotObjectPath, s_screenShotInterface, mode);

    QVariantList arguments;
    arguments.push_back(screen);
    arguments.push_back(options);
    message.setArguments(arguments);

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    QDBusPendingCallWatcher watcher(pendingCall);

    QSignalSpy received(&watcher, &QDBusPendingCallWatcher::finished);
    QVERIFY(received.wait());

    const QDBusPendingReply<QDBusUnixFileDescriptor, QVariantMap> reply = watcher;
    const QDBusUnixFileDescriptor image = reply.argumentAt<0>();
    const QVariantMap metadata = reply.argumentAt<1>();

    QVERIFY(image.fileDescriptor() != -1);

    bool ok;
    const int width = metadata.value("width").toInt(&ok);
    QVERIFY(ok);
    QCOMPARE(width, output->pixelSize().width());

    const int height = metadata.value("height").toInt(&ok);
    QVERIFY(ok);
    QCOMPARE(height, output->pixelSize().height());

    const int stride = metadata.value("stride").toInt(&ok);
    QVERIFY(ok);
    QCOMPARE_GE(stride, width * 4);

    const uint format = metadata.value("format").toUInt(&ok);
    QVERIFY(ok);

    MemoryMap map(stride * height, PROT_READ, MAP_SHARED, image.fileDescriptor(), 0);
    QVERIFY(map.isValid());

    const QImage resultImage(static_cast<uchar *>(map.data()), width, height, QImage::Format(format));
    QCOMPARE(resultImage, testImage);
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::ScreenshotTest)
#include "screenshot_test.moc"
