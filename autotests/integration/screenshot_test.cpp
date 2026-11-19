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
    void areashot();
    void windowshot();
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

std::optional<std::pair<QImage, MemoryMap>> takeScreenshot(const QString &mode,
                                                           QVariantList arguments,
                                                           const QSize &size)
{
    QDBusMessage message = QDBusMessage::createMethodCall(s_screenShotService, s_screenShotObjectPath, s_screenShotInterface, mode);
    // options, currently unused
    arguments.push_back(QVariantMap{});
    message.setArguments(arguments);

    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    QDBusPendingCallWatcher watcher(pendingCall);

    QSignalSpy received(&watcher, &QDBusPendingCallWatcher::finished);
    if (!received.wait()) {
        return std::nullopt;
    }

    const QDBusPendingReply<QDBusUnixFileDescriptor, QVariantMap> reply = watcher;
    const QDBusUnixFileDescriptor image = reply.argumentAt<0>();
    const QVariantMap metadata = reply.argumentAt<1>();

    if (image.fileDescriptor() == -1) {
        return std::nullopt;
    }

    bool ok;
    const int width = metadata.value("width").toInt(&ok);
    if (!ok || width != size.width()) {
        return std::nullopt;
    }

    const int height = metadata.value("height").toInt(&ok);
    if (!ok || height != size.height()) {
        return std::nullopt;
    }

    const int stride = metadata.value("stride").toInt(&ok);
    if (!ok || stride < width * 4) {
        return std::nullopt;
    }

    const uint format = metadata.value("format").toUInt(&ok);
    if (!ok) {
        return std::nullopt;
    }

    MemoryMap map(stride * height, PROT_READ, MAP_SHARED, image.fileDescriptor(), 0);
    if (!map.isValid()) {
        return std::nullopt;
    }

    return std::make_pair(QImage(static_cast<uchar *>(map.data()), width, height, QImage::Format(format)), std::move(map));
}

void ScreenshotTest::screenshot()
{
    LogicalOutput *const output = workspace()->outputs().front();

    QImage testImage(output->pixelSize(), QImage::Format_RGB32);
    testImage.fill(Qt::blue);
    testImage.setPixel(0, 0, qRgba(50, 123, 231, 128));
    testImage.setPixel(testImage.width() - 1, testImage.height() - 1, qRgba(244, 231, 123, 64));

    Test::XdgToplevelWindow window{[](Test::XdgToplevel *toplevel) {
        toplevel->set_fullscreen(nullptr);
    }};
    QVERIFY(window.show(testImage));
    QVERIFY(window.presentWait());

    const auto shot = takeScreenshot(QStringLiteral("CaptureScreen"), {output->name()}, testImage.size());
    QVERIFY(shot);
    QCOMPARE(shot->first, testImage);
}

void ScreenshotTest::areashot()
{
    QImage testImage(QSize(200, 200), QImage::Format_RGB32);
    testImage.fill(Qt::blue);
    testImage.setPixel(0, 0, qRgba(50, 123, 231, 128));
    testImage.setPixel(testImage.width() - 1, testImage.height() - 1, qRgba(244, 231, 123, 64));

    Test::XdgToplevelWindow window;
    QVERIFY(window.show(testImage));
    QVERIFY(window.presentWait());
    window.m_window->move(QPointF(100, 100));

    const auto shot = takeScreenshot(QStringLiteral("CaptureArea"), {int32_t(100), int32_t(100), uint32_t(200), uint32_t(200)}, testImage.size());
    QCOMPARE(shot->first, testImage);
}

void ScreenshotTest::windowshot()
{
    LogicalOutput *const output = workspace()->outputs().front();

    QImage testImage(QSize(300, 300), QImage::Format_ARGB32_Premultiplied);
    testImage.fill(Qt::blue);
    testImage.setPixel(0, 0, qRgba(50, 123, 231, 128));
    testImage.setPixel(testImage.width() - 1, testImage.height() - 1, qRgba(244, 231, 123, 64));

    Test::XdgToplevelWindow window;
    QVERIFY(window.show(testImage));
    QVERIFY(window.presentWait());
    window.m_window->move(output->geometryF().bottomRight() - QPointF(20, 30));

    const auto shot = takeScreenshot(QStringLiteral("CaptureActiveWindow"), {}, testImage.size());
    QCOMPARE(shot->first, testImage);
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::ScreenshotTest)
#include "screenshot_test.moc"
