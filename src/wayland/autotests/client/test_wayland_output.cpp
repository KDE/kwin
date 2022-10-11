/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "wayland/display.h"
#include "wayland/output_interface.h"

#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/output.h"
#include "KWayland/Client/registry.h"

#include "../../tests/fakeoutput.h"

// Wayland
#include <wayland-client-protocol.h>

class TestWaylandOutput : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandOutput(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testRegistry();
    void testModeChange();
    void testScaleChange();

    void testSubPixel_data();
    void testSubPixel();

    void testTransform_data();
    void testTransform();

private:
    std::unique_ptr<KWaylandServer::Display> m_display;
    std::unique_ptr<FakeOutput> m_outputHandle;
    std::unique_ptr<KWaylandServer::OutputInterface> m_outputInterface;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-output-0");

TestWaylandOutput::TestWaylandOutput(QObject *parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_thread(nullptr)
{
}

void TestWaylandOutput::init()
{
    using namespace KWaylandServer;
    m_display = std::make_unique<KWaylandServer::Display>();
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_outputHandle = std::make_unique<FakeOutput>();
    m_outputHandle->setMode(QSize(1024, 768), 60000);

    m_outputInterface = std::make_unique<OutputInterface>(m_display.get(), m_outputHandle.get());
    m_outputInterface->setMode(QSize(1024, 768), 60000);

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new KWayland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());
}

void TestWaylandOutput::cleanup()
{
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    delete m_connection;
    m_connection = nullptr;

    m_display.reset();

    m_outputInterface.reset();
    m_outputHandle.reset();
}

void TestWaylandOutput::testRegistry()
{
    QSignalSpy globalPositionChangedSpy(m_outputInterface.get(), &KWaylandServer::OutputInterface::globalPositionChanged);
    QCOMPARE(m_outputInterface->globalPosition(), QPoint(0, 0));
    m_outputHandle->moveTo(QPoint(100, 50));
    m_outputInterface->setGlobalPosition(QPoint(100, 50));
    QCOMPARE(m_outputInterface->globalPosition(), QPoint(100, 50));
    QCOMPARE(globalPositionChangedSpy.count(), 1);
    // changing again should not trigger signal
    m_outputHandle->moveTo(QPoint(100, 50));
    m_outputInterface->setGlobalPosition(QPoint(100, 50));
    QCOMPARE(globalPositionChangedSpy.count(), 1);

    QSignalSpy physicalSizeChangedSpy(m_outputInterface.get(), &KWaylandServer::OutputInterface::physicalSizeChanged);
    QCOMPARE(m_outputInterface->physicalSize(), QSize());
    m_outputHandle->setPhysicalSize(QSize(200, 100));
    m_outputInterface->setPhysicalSize(QSize(200, 100));
    QCOMPARE(m_outputInterface->physicalSize(), QSize(200, 100));
    QCOMPARE(physicalSizeChangedSpy.count(), 1);
    // changing again should not trigger signal
    m_outputHandle->setPhysicalSize(QSize(200, 100));
    m_outputInterface->setPhysicalSize(QSize(200, 100));
    QCOMPARE(physicalSizeChangedSpy.count(), 1);
    m_outputInterface->done();

    KWayland::Client::Registry registry;
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputAnnounced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    KWayland::Client::Output output;
    QVERIFY(!output.isValid());
    QCOMPARE(output.geometry(), QRect());
    QCOMPARE(output.globalPosition(), QPoint());
    QCOMPARE(output.manufacturer(), QString());
    QCOMPARE(output.model(), QString());
    QCOMPARE(output.physicalSize(), QSize());
    QCOMPARE(output.pixelSize(), QSize());
    QCOMPARE(output.refreshRate(), 0);
    QCOMPARE(output.scale(), 1);
    QCOMPARE(output.subPixel(), KWayland::Client::Output::SubPixel::Unknown);
    QCOMPARE(output.transform(), KWayland::Client::Output::Transform::Normal);

    QSignalSpy outputChanged(&output, &KWayland::Client::Output::changed);
    auto o = registry.bindOutput(announced.first().first().value<quint32>(), announced.first().last().value<quint32>());
    QVERIFY(!KWayland::Client::Output::get(o));
    output.setup(o);
    QCOMPARE(KWayland::Client::Output::get(o), &output);
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());

    QCOMPARE(output.geometry(), QRect(100, 50, 1024, 768));
    QCOMPARE(output.globalPosition(), QPoint(100, 50));
    QCOMPARE(output.manufacturer(), QStringLiteral("org.kde.kwin"));
    QCOMPARE(output.model(), QStringLiteral("none"));
    QCOMPARE(output.physicalSize(), QSize(200, 100));
    QCOMPARE(output.pixelSize(), QSize(1024, 768));
    QCOMPARE(output.refreshRate(), 60000);
    QCOMPARE(output.scale(), 1);
    // for xwayland output it's unknown
    QCOMPARE(output.subPixel(), KWayland::Client::Output::SubPixel::Unknown);
    // for xwayland transform is normal
    QCOMPARE(output.transform(), KWayland::Client::Output::Transform::Normal);
}

void TestWaylandOutput::testModeChange()
{
    using namespace KWayland::Client;
    KWayland::Client::Registry registry;
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    KWayland::Client::Output output;
    QSignalSpy outputChanged(&output, &KWayland::Client::Output::changed);
    QSignalSpy modeAddedSpy(&output, &KWayland::Client::Output::modeAdded);
    output.setup(registry.bindOutput(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());
    QCOMPARE(modeAddedSpy.count(), 1);
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().size, QSize(1024, 768));
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().refreshRate, 60000);
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().flags, Output::Mode::Flags(Output::Mode::Flag::Current));
    QCOMPARE(modeAddedSpy.at(0).first().value<Output::Mode>().output, QPointer<Output>(&output));
    QCOMPARE(output.pixelSize(), QSize(1024, 768));
    QCOMPARE(output.refreshRate(), 60000);

    // change once more
    m_outputHandle->setMode(QSize(1280, 1024), 90000);
    m_outputInterface->setMode(QSize(1280, 1024), 90000);
    QCOMPARE(m_outputInterface->refreshRate(), 90000);
    m_outputInterface->done();
    QVERIFY(outputChanged.wait());
    QCOMPARE(modeAddedSpy.count(), 2);
    QCOMPARE(modeAddedSpy.at(1).first().value<Output::Mode>().size, QSize(1280, 1024));
    QCOMPARE(modeAddedSpy.at(1).first().value<Output::Mode>().refreshRate, 90000);
    QCOMPARE(modeAddedSpy.at(1).first().value<Output::Mode>().flags, Output::Mode::Flags(Output::Mode::Flag::Current));
    QCOMPARE(modeAddedSpy.at(1).first().value<Output::Mode>().output, QPointer<Output>(&output));
    QCOMPARE(output.pixelSize(), QSize(1280, 1024));
    QCOMPARE(output.refreshRate(), 90000);
}

void TestWaylandOutput::testScaleChange()
{
    KWayland::Client::Registry registry;
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputAnnounced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    KWayland::Client::Output output;
    QSignalSpy outputChanged(&output, &KWayland::Client::Output::changed);
    output.setup(registry.bindOutput(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 1);

    // change the scale
    outputChanged.clear();
    QCOMPARE(m_outputInterface->scale(), 1);
    QSignalSpy serverScaleChanged(m_outputInterface.get(), &KWaylandServer::OutputInterface::scaleChanged);
    m_outputHandle->setScale(2);
    m_outputInterface->setScale(2);
    QCOMPARE(m_outputInterface->scale(), 2);
    m_outputInterface->done();
    QCOMPARE(serverScaleChanged.count(), 1);
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 2);
    // changing to same value should not trigger
    m_outputHandle->setScale(2);
    m_outputInterface->setScale(2);
    QCOMPARE(serverScaleChanged.count(), 1);
    QVERIFY(!outputChanged.wait(100));

    // change once more
    outputChanged.clear();
    m_outputHandle->setScale(4);
    m_outputInterface->setScale(4);
    m_outputInterface->done();
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 4);
}

void TestWaylandOutput::testSubPixel_data()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QTest::addColumn<KWayland::Client::Output::SubPixel>("expected");
    QTest::addColumn<KWin::Output::SubPixel>("actual");

    QTest::newRow("none") << Output::SubPixel::None << KWin::Output::SubPixel::None;
    QTest::newRow("horizontal/rgb") << Output::SubPixel::HorizontalRGB << KWin::Output::SubPixel::Horizontal_RGB;
    QTest::newRow("horizontal/bgr") << Output::SubPixel::HorizontalBGR << KWin::Output::SubPixel::Horizontal_BGR;
    QTest::newRow("vertical/rgb") << Output::SubPixel::VerticalRGB << KWin::Output::SubPixel::Vertical_RGB;
    QTest::newRow("vertical/bgr") << Output::SubPixel::VerticalBGR << KWin::Output::SubPixel::Vertical_BGR;
}

void TestWaylandOutput::testSubPixel()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QFETCH(KWin::Output::SubPixel, actual);
    QCOMPARE(m_outputInterface->subPixel(), KWin::Output::SubPixel::Unknown);
    QSignalSpy serverSubPixelChangedSpy(m_outputInterface.get(), &KWaylandServer::OutputInterface::subPixelChanged);
    m_outputHandle->setSubPixel(actual);
    m_outputInterface->setSubPixel(actual);
    QCOMPARE(m_outputInterface->subPixel(), actual);
    QCOMPARE(serverSubPixelChangedSpy.count(), 1);
    // changing to same value should not trigger the signal
    m_outputHandle->setSubPixel(actual);
    m_outputInterface->setSubPixel(actual);
    QCOMPARE(serverSubPixelChangedSpy.count(), 1);

    KWayland::Client::Registry registry;
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputAnnounced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    KWayland::Client::Output output;
    QSignalSpy outputChanged(&output, &KWayland::Client::Output::changed);
    output.setup(registry.bindOutput(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }

    QTEST(output.subPixel(), "expected");

    // change back to unknown
    outputChanged.clear();
    m_outputHandle->setSubPixel(KWin::Output::SubPixel::Unknown);
    m_outputInterface->setSubPixel(KWin::Output::SubPixel::Unknown);
    QCOMPARE(m_outputInterface->subPixel(), KWin::Output::SubPixel::Unknown);
    m_outputInterface->done();
    QCOMPARE(serverSubPixelChangedSpy.count(), 2);
    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }
    QCOMPARE(output.subPixel(), Output::SubPixel::Unknown);
}

void TestWaylandOutput::testTransform_data()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QTest::addColumn<KWayland::Client::Output::Transform>("expected");
    QTest::addColumn<KWin::Output::Transform>("actual");

    QTest::newRow("90") << Output::Transform::Rotated90 << KWin::Output::Transform::Rotated90;
    QTest::newRow("180") << Output::Transform::Rotated180 << KWin::Output::Transform::Rotated180;
    QTest::newRow("270") << Output::Transform::Rotated270 << KWin::Output::Transform::Rotated270;
    QTest::newRow("Flipped") << Output::Transform::Flipped << KWin::Output::Transform::Flipped;
    QTest::newRow("Flipped 90") << Output::Transform::Flipped90 << KWin::Output::Transform::Flipped90;
    QTest::newRow("Flipped 180") << Output::Transform::Flipped180 << KWin::Output::Transform::Flipped180;
    QTest::newRow("Flipped 280") << Output::Transform::Flipped270 << KWin::Output::Transform::Flipped270;
}

void TestWaylandOutput::testTransform()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QFETCH(KWin::Output::Transform, actual);
    QCOMPARE(m_outputInterface->transform(), KWin::Output::Transform::Normal);
    QSignalSpy serverTransformChangedSpy(m_outputInterface.get(), &KWaylandServer::OutputInterface::transformChanged);
    m_outputHandle->setTransform(actual);
    m_outputInterface->setTransform(actual);
    QCOMPARE(m_outputInterface->transform(), actual);
    QCOMPARE(serverTransformChangedSpy.count(), 1);
    // changing to same should not trigger signal
    m_outputHandle->setTransform(actual);
    m_outputInterface->setTransform(actual);
    QCOMPARE(serverTransformChangedSpy.count(), 1);

    KWayland::Client::Registry registry;
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputAnnounced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(announced.wait());

    KWayland::Client::Output *output = registry.createOutput(announced.first().first().value<quint32>(), announced.first().last().value<quint32>(), &registry);
    QSignalSpy outputChanged(output, &KWayland::Client::Output::changed);
    wl_display_flush(m_connection->display());
    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }

    QTEST(output->transform(), "expected");

    // change back to normal
    outputChanged.clear();
    m_outputHandle->setTransform(KWin::Output::Transform::Normal);
    m_outputInterface->setTransform(KWin::Output::Transform::Normal);
    QCOMPARE(m_outputInterface->transform(), KWin::Output::Transform::Normal);
    m_outputInterface->done();
    QCOMPARE(serverTransformChangedSpy.count(), 2);
    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }
    QCOMPARE(output->transform(), Output::Transform::Normal);
}

QTEST_GUILESS_MAIN(TestWaylandOutput)
#include "test_wayland_output.moc"
