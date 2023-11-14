/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "compositor.h"
#include "core/output.h"
#include "generic_scene_opengl_test.h"
#include "opengl/glplatform.h"
#include "pointer_input.h"
#include "scene/workspacescene.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KWayland/Client/output.h>
#include <KWayland/Client/subsurface.h>
#include <KWayland/Client/surface.h>
#include <PipeWireSourceStream>
#include <QPainter>
#include <QScreen>

Q_DECLARE_METATYPE(PipeWireFrame);

#define QCOMPAREIMG(actual, expected, id)                                                        \
    {                                                                                            \
        if ((actual) != (expected)) {                                                            \
            const auto actualFile = QStringLiteral("appium_artifact_actual_%1.png").arg(id);     \
            const auto expectedFile = QStringLiteral("appium_artifact_expected_%1.png").arg(id); \
            (actual).save(actualFile);                                                           \
            (expected).save(expectedFile);                                                       \
            qDebug() << "Generated failed file" << actualFile << expectedFile;                   \
        }                                                                                        \
        QCOMPARE(actual, expected);                                                              \
    }

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_buffer_size_change-0");

class ScreencastingTest : public GenericSceneOpenGLTest
{
    Q_OBJECT
public:
    ScreencastingTest()
        : GenericSceneOpenGLTest(QByteArrayLiteral("O2"))
    {
        qRegisterMetaType<PipeWireFrame>();

        auto wrap = [this](const QString &process, const QStringList &arguments = {}) {
            // Make sure PipeWire is running. If it's already running it will just exit
            QProcess *p = new QProcess(this);
            p->setProcessChannelMode(QProcess::MergedChannels);
            p->setArguments(arguments);
            connect(this, &QObject::destroyed, p, [p] {
                p->terminate();
                p->waitForFinished();
                p->kill();
            });
            connect(p, &QProcess::errorOccurred, p, [p](auto status) {
                qDebug() << "error" << status << p->program();
            });
            connect(p, &QProcess::finished, p, [p](int code, auto status) {
                if (code != 0) {
                    qDebug() << p->readAll();
                }
                qDebug() << "finished" << code << status << p->program();
            });
            p->setProgram(process);
            p->start();
        };

        // If I run this outside the CI, it breaks the system's pipewire
        if (qgetenv("KDECI_BUILD") == "TRUE") {
            wrap("pipewire");
            wrap("dbus-launch", {"wireplumber"});
        }
    }
private Q_SLOTS:
    void init();
    void testWindowCasting();
    void testOutputCasting();

private:
    std::optional<QImage> oneFrameAndClose(Test::ScreencastingStreamV1 *stream);
};

void ScreencastingTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::ScreencastingV1));
    QVERIFY(KWin::Test::screencasting());
    Cursors::self()->hideCursor();
}

std::optional<QImage> ScreencastingTest::oneFrameAndClose(Test::ScreencastingStreamV1 *stream)
{
    Q_ASSERT(stream);
    PipeWireSourceStream pwStream;
    qDebug() << "start" << stream;
    connect(stream, &Test::ScreencastingStreamV1::failed, qGuiApp, [](const QString &error) {
        qDebug() << "stream failed with error" << error;
        Q_ASSERT(false);
    });
    connect(stream, &Test::ScreencastingStreamV1::closed, qGuiApp, [&pwStream] {
        pwStream.setActive(false);
    });
    connect(stream, &Test::ScreencastingStreamV1::created, qGuiApp, [&pwStream](quint32 nodeId) {
        pwStream.createStream(nodeId, 0);
    });

    std::optional<QImage> img;
    connect(&pwStream, &PipeWireSourceStream::frameReceived, qGuiApp, [&img](const PipeWireFrame &frame) {
        img = frame.image;
    });

    QSignalSpy spy(&pwStream, &PipeWireSourceStream::frameReceived);
    if (!spy.wait()) {
        qDebug() << "Did not receive any frames";
    }
    pwStream.stopStreaming();
    return img;
}

void ScreencastingTest::testWindowCasting()
{
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);

    QImage sourceImage(QSize(30, 10), QImage::Format_RGBA8888_Premultiplied);
    sourceImage.fill(Qt::red);

    Window *window = Test::renderAndWaitForShown(surface.get(), sourceImage);
    QVERIFY(window);

    auto stream = KWin::Test::screencasting()->createWindowStream(window->internalId().toString(), QtWayland::zkde_screencast_unstable_v1::pointer_hidden);

    std::optional<QImage> img = oneFrameAndClose(stream);
    QVERIFY(img);
    img->convertTo(sourceImage.format());
    QCOMPAREIMG(*img, sourceImage, QLatin1String("window_cast"));
}

void ScreencastingTest::testOutputCasting()
{
    auto theOutput = KWin::Test::waylandOutputs().constFirst();

    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);

    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);

    QImage sourceImage(theOutput->pixelSize(), QImage::Format_RGBA8888_Premultiplied);
    sourceImage.fill(Qt::green);
    {
        QPainter p(&sourceImage);
        p.drawRect(100, 100, 100, 100);
    }

    Window *window = Test::renderAndWaitForShown(surface.get(), sourceImage);
    QVERIFY(window);
    QCOMPARE(window->frameGeometry(), window->output()->geometry());

    auto stream = KWin::Test::screencasting()->createOutputStream(theOutput->output(), QtWayland::zkde_screencast_unstable_v1::pointer_hidden);

    std::optional<QImage> img = oneFrameAndClose(stream);
    QVERIFY(img);
    img->convertTo(sourceImage.format());
    QCOMPAREIMG(*img, sourceImage, QLatin1String("output_cast"));
}

}

WAYLANDTEST_MAIN(KWin::ScreencastingTest)
#include "screencasting_test.moc"
