/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QSignalSpy>
#include <QTest>
// KWin
#include "wayland/display.h"
#include "wayland/output.h"

#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/output.h"
#include "KWayland/Client/registry.h"

#include "../../../tests/fakeoutput.h"

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
    KWin::Display *m_display;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;
};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-output-0");

TestWaylandOutput::TestWaylandOutput(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_connection(nullptr)
    , m_thread(nullptr)
{
}

void TestWaylandOutput::init()
{
    delete m_display;
    m_display = new KWin::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

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

    delete m_display;
    m_display = nullptr;
}

void TestWaylandOutput::testRegistry()
{
    auto fakeOutput = std::make_unique<FakeBackendOutput>();
    fakeOutput->setMode(QSize(1024, 768), 60000);
    fakeOutput->moveTo(QPoint(100, 50));
    fakeOutput->setPhysicalSize(QSize(200, 100));
    auto outputHandle = std::make_unique<KWin::LogicalOutput>(fakeOutput.get());

    auto outputInterface = std::make_unique<KWin::OutputInterface>(m_display, outputHandle.get());

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
    QCOMPARE(output.manufacturer(), QString());
    QCOMPARE(output.model(), QString());
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
    auto fakeOutput = std::make_unique<FakeBackendOutput>();
    fakeOutput->setMode(QSize(1024, 768), 60000);
    auto outputHandle = std::make_unique<KWin::LogicalOutput>(fakeOutput.get());

    auto outputInterface = std::make_unique<KWin::OutputInterface>(m_display, outputHandle.get());

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
    QCOMPARE(modeAddedSpy.at(0).first().value<KWayland::Client::Output::Mode>().size, QSize(1024, 768));
    QCOMPARE(modeAddedSpy.at(0).first().value<KWayland::Client::Output::Mode>().refreshRate, 60000);
    QCOMPARE(modeAddedSpy.at(0).first().value<KWayland::Client::Output::Mode>().flags, KWayland::Client::Output::Mode::Flags(KWayland::Client::Output::Mode::Flag::Current));
    QCOMPARE(modeAddedSpy.at(0).first().value<KWayland::Client::Output::Mode>().output, QPointer<KWayland::Client::Output>(&output));
    QCOMPARE(output.pixelSize(), QSize(1024, 768));
    QCOMPARE(output.refreshRate(), 60000);

    // change once more
    fakeOutput->setMode(QSize(1280, 1024), 90000);
    QVERIFY(outputChanged.wait());
    QCOMPARE(modeAddedSpy.count(), 2);
    QCOMPARE(modeAddedSpy.at(1).first().value<KWayland::Client::Output::Mode>().size, QSize(1280, 1024));
    QCOMPARE(modeAddedSpy.at(1).first().value<KWayland::Client::Output::Mode>().refreshRate, 90000);
    QCOMPARE(modeAddedSpy.at(1).first().value<KWayland::Client::Output::Mode>().flags, KWayland::Client::Output::Mode::Flags(KWayland::Client::Output::Mode::Flag::Current));
    QCOMPARE(modeAddedSpy.at(1).first().value<KWayland::Client::Output::Mode>().output, QPointer<KWayland::Client::Output>(&output));
    QCOMPARE(output.pixelSize(), QSize(1280, 1024));
    QCOMPARE(output.refreshRate(), 90000);
}

void TestWaylandOutput::testScaleChange()
{
    auto fakeOutput = std::make_unique<FakeBackendOutput>();
    fakeOutput->setMode(QSize(1024, 768), 60000);
    auto outputHandle = std::make_unique<KWin::LogicalOutput>(fakeOutput.get());

    auto outputInterface = std::make_unique<KWin::OutputInterface>(m_display, outputHandle.get());

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
    fakeOutput->setScale(2);
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 2);
    // changing to same value should not trigger
    fakeOutput->setScale(2);
    QVERIFY(!outputChanged.wait(100));

    // change once more
    outputChanged.clear();
    fakeOutput->setScale(4);
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 4);
}

void TestWaylandOutput::testSubPixel_data()
{
    QTest::addColumn<KWayland::Client::Output::SubPixel>("expected");
    QTest::addColumn<KWin::BackendOutput::SubPixel>("actual");

    QTest::newRow("none") << KWayland::Client::Output::SubPixel::None << KWin::BackendOutput::SubPixel::None;
    QTest::newRow("horizontal/rgb") << KWayland::Client::Output::SubPixel::HorizontalRGB << KWin::BackendOutput::SubPixel::Horizontal_RGB;
    QTest::newRow("horizontal/bgr") << KWayland::Client::Output::SubPixel::HorizontalBGR << KWin::BackendOutput::SubPixel::Horizontal_BGR;
    QTest::newRow("vertical/rgb") << KWayland::Client::Output::SubPixel::VerticalRGB << KWin::BackendOutput::SubPixel::Vertical_RGB;
    QTest::newRow("vertical/bgr") << KWayland::Client::Output::SubPixel::VerticalBGR << KWin::BackendOutput::SubPixel::Vertical_BGR;
}

void TestWaylandOutput::testSubPixel()
{
    QFETCH(KWin::BackendOutput::SubPixel, actual);

    auto fakeOutput = std::make_unique<FakeBackendOutput>();
    fakeOutput->setMode(QSize(1024, 768), 60000);
    fakeOutput->setSubPixel(actual);
    auto outputHandle = std::make_unique<KWin::LogicalOutput>(fakeOutput.get());

    auto outputInterface = std::make_unique<KWin::OutputInterface>(m_display, outputHandle.get());

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
}

void TestWaylandOutput::testTransform_data()
{
    QTest::addColumn<KWayland::Client::Output::Transform>("expected");
    QTest::addColumn<KWin::OutputTransform::Kind>("actual");

    QTest::newRow("90") << KWayland::Client::Output::Transform::Rotated90 << KWin::OutputTransform::Rotate90;
    QTest::newRow("180") << KWayland::Client::Output::Transform::Rotated180 << KWin::OutputTransform::Rotate180;
    QTest::newRow("270") << KWayland::Client::Output::Transform::Rotated270 << KWin::OutputTransform::Rotate270;
    QTest::newRow("Flipped") << KWayland::Client::Output::Transform::Flipped << KWin::OutputTransform::FlipX;
    QTest::newRow("Flipped 90") << KWayland::Client::Output::Transform::Flipped90 << KWin::OutputTransform::FlipX90;
    QTest::newRow("Flipped 180") << KWayland::Client::Output::Transform::Flipped180 << KWin::OutputTransform::FlipX180;
    QTest::newRow("Flipped 280") << KWayland::Client::Output::Transform::Flipped270 << KWin::OutputTransform::FlipX270;
}

void TestWaylandOutput::testTransform()
{
    QFETCH(KWin::OutputTransform::Kind, actual);

    auto fakeOutput = std::make_unique<FakeBackendOutput>();
    fakeOutput->setMode(QSize(1024, 768), 60000);
    fakeOutput->setTransform(actual);
    auto outputHandle = std::make_unique<KWin::LogicalOutput>(fakeOutput.get());

    auto outputInterface = std::make_unique<KWin::OutputInterface>(m_display, outputHandle.get());

    using namespace KWin;

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
    fakeOutput->setTransform(KWin::OutputTransform::Normal);
    if (outputChanged.isEmpty()) {
        QVERIFY(outputChanged.wait());
    }
    QCOMPARE(output->transform(), KWayland::Client::Output::Transform::Normal);
}

QTEST_GUILESS_MAIN(TestWaylandOutput)
#include "test_wayland_output.moc"
