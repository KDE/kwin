/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2015 Sebastian Kügler <sebas@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/outputdevice.h"
#include "KWayland/Client/registry.h"
#include "../../src/server/display.h"
#include "../../src/server/outputdevice_interface.h"
// Wayland
#include <wayland-client-protocol.h>

using namespace KWayland::Client;
using namespace KWaylandServer;

class TestWaylandOutputDevice : public QObject
{
    Q_OBJECT
public:
    explicit TestWaylandOutputDevice(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    void testRegistry();
    void testModeChanges();
#if KWAYLANDSERVER_ENABLE_DEPRECATED_SINCE(5, 50)
    void testScaleChange_legacy();
#endif
    void testScaleChange();
    void testColorCurvesChange();

    void testSubPixel_data();
    void testSubPixel();

    void testTransform_data();
    void testTransform();

    void testEnabled();
    void testEdid();
    void testId();
    void testDone();

private:
    KWaylandServer::Display *m_display;
    KWaylandServer::OutputDeviceInterface *m_serverOutputDevice;
    QByteArray m_edid;
    QString m_serialNumber;
    QString m_eidaId;

    KWaylandServer::OutputDeviceInterface::ColorCurves m_initColorCurves;
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    QThread *m_thread;

};

static const QString s_socketName = QStringLiteral("kwin-test-wayland-output-0");

TestWaylandOutputDevice::TestWaylandOutputDevice(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_serverOutputDevice(nullptr)
    , m_connection(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestWaylandOutputDevice::init()
{
    using namespace KWaylandServer;
    delete m_display;
    m_display = new Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_serverOutputDevice = new OutputDeviceInterface(m_display, this);
    m_serverOutputDevice->setUuid("1337");


    OutputDeviceInterface::Mode m0;
    m0.id = 0;
    m0.size = QSize(800, 600);
    m0.flags = OutputDeviceInterface::ModeFlags(OutputDeviceInterface::ModeFlag::Preferred);
    m_serverOutputDevice->addMode(m0);

    OutputDeviceInterface::Mode m1;
    m1.id = 1;
    m1.size = QSize(1024, 768);
    m_serverOutputDevice->addMode(m1);

    OutputDeviceInterface::Mode m2;
    m2.id = 2;
    m2.size = QSize(1280, 1024);
    m2.refreshRate = 90000;
    m_serverOutputDevice->addMode(m2);

    m_serverOutputDevice->setCurrentMode(1);

    m_edid = QByteArray::fromBase64("AP///////wAQrBbwTExLQQ4WAQOANCB46h7Frk80sSYOUFSlSwCBgKlA0QBxTwEBAQEBAQEBKDyAoHCwI0AwIDYABkQhAAAaAAAA/wBGNTI1TTI0NUFLTEwKAAAA/ABERUxMIFUyNDEwCiAgAAAA/QA4TB5REQAKICAgICAgAToCAynxUJAFBAMCBxYBHxITFCAVEQYjCQcHZwMMABAAOC2DAQAA4wUDAQI6gBhxOC1AWCxFAAZEIQAAHgEdgBhxHBYgWCwlAAZEIQAAngEdAHJR0B4gbihVAAZEIQAAHowK0Iog4C0QED6WAAZEIQAAGAAAAAAAAAAAAAAAAAAAPg==");
    m_serverOutputDevice->setEdid(m_edid);

    m_serialNumber = "23498723948723";
    m_serverOutputDevice->setSerialNumber(m_serialNumber);
    m_eidaId = "asdffoo";
    m_serverOutputDevice->setEisaId(m_eidaId);

    m_initColorCurves.red.clear();
    m_initColorCurves.green.clear();
    m_initColorCurves.blue.clear();
    // 8 bit color ramps
    for (int i = 0; i < 256; i++) {
        quint16 val = (double)i / 255 * UINT16_MAX;
        m_initColorCurves.red << val ;
        m_initColorCurves.green << val ;
    }
    // 10 bit color ramp
    for (int i = 0; i < 320; i++) {
        m_initColorCurves.blue << (double)i / 319 * UINT16_MAX;
    }
    m_serverOutputDevice->setColorCurves(m_initColorCurves);

    m_serverOutputDevice->create();

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

void TestWaylandOutputDevice::cleanup()
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

    delete m_serverOutputDevice;
    m_serverOutputDevice = nullptr;

    delete m_display;
    m_display = nullptr;
}

void TestWaylandOutputDevice::testRegistry()
{
    m_serverOutputDevice->setGlobalPosition(QPoint(100, 50));
    m_serverOutputDevice->setPhysicalSize(QSize(200, 100));

    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputDeviceAnnounced);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    KWayland::Client::OutputDevice output;
    QVERIFY(!output.isValid());
    QCOMPARE(output.uuid(), QByteArray());
    QCOMPARE(output.geometry(), QRect());
    QCOMPARE(output.globalPosition(), QPoint());
    QCOMPARE(output.manufacturer(), QString());
    QCOMPARE(output.model(), QString());
    QCOMPARE(output.physicalSize(), QSize());
    QCOMPARE(output.pixelSize(), QSize());
    QCOMPARE(output.refreshRate(), 0);
#if KWAYLANDSERVER_ENABLE_DEPRECATED_SINCE(5, 50)
    QCOMPARE(output.scale(), 1);
#endif
    QCOMPARE(output.scaleF(), 1.0);
    QCOMPARE(output.colorCurves().red, QVector<quint16>());
    QCOMPARE(output.colorCurves().green, QVector<quint16>());
    QCOMPARE(output.colorCurves().blue, QVector<quint16>());
    QCOMPARE(output.subPixel(), KWayland::Client::OutputDevice::SubPixel::Unknown);
    QCOMPARE(output.transform(), KWayland::Client::OutputDevice::Transform::Normal);
    QCOMPARE(output.enabled(), OutputDevice::Enablement::Enabled);
    QCOMPARE(output.edid(), QByteArray());
    QCOMPARE(output.eisaId(), QString());
    QCOMPARE(output.serialNumber(), QString());

    QSignalSpy outputChanged(&output, &KWayland::Client::OutputDevice::done);
    QVERIFY(outputChanged.isValid());

    output.setup(registry.bindOutputDevice(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());

    QVERIFY(outputChanged.wait());

    QCOMPARE(output.geometry(), QRect(100, 50, 1024, 768));
    QCOMPARE(output.globalPosition(), QPoint(100, 50));
    QCOMPARE(output.manufacturer(), QStringLiteral("org.kde.kwin"));
    QCOMPARE(output.model(), QStringLiteral("none"));
    QCOMPARE(output.physicalSize(), QSize(200, 100));
    QCOMPARE(output.pixelSize(), QSize(1024, 768));
    QCOMPARE(output.refreshRate(), 60000);
#if KWAYLANDSERVER_ENABLE_DEPRECATED_SINCE(5, 50)
    QCOMPARE(output.scale(), 1);
#endif
    QCOMPARE(output.scaleF(), 1.0);
    QCOMPARE(output.colorCurves().red, m_initColorCurves.red);
    QCOMPARE(output.colorCurves().green, m_initColorCurves.green);
    QCOMPARE(output.colorCurves().blue, m_initColorCurves.blue);
    // for xwayland output it's unknown
    QCOMPARE(output.subPixel(), KWayland::Client::OutputDevice::SubPixel::Unknown);
    // for xwayland transform is normal
    QCOMPARE(output.transform(), KWayland::Client::OutputDevice::Transform::Normal);

    QCOMPARE(output.edid(), m_edid);
    QCOMPARE(output.enabled(), OutputDevice::Enablement::Enabled);
    QCOMPARE(output.uuid(), QByteArray("1337"));
    QCOMPARE(output.serialNumber(), m_serialNumber);
    QCOMPARE(output.eisaId(), m_eidaId);
}

void TestWaylandOutputDevice::testModeChanges()
{
    using namespace KWayland::Client;
    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputDeviceAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    KWayland::Client::OutputDevice output;
    QSignalSpy outputChanged(&output, &KWayland::Client::OutputDevice::changed);
    QVERIFY(outputChanged.isValid());
    QSignalSpy modeAddedSpy(&output, &KWayland::Client::OutputDevice::modeAdded);
    QVERIFY(modeAddedSpy.isValid());
    QSignalSpy doneSpy(&output, &KWayland::Client::OutputDevice::done);
    QVERIFY(doneSpy.isValid());
    output.setup(registry.bindOutputDevice(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(doneSpy.wait());
    QCOMPARE(modeAddedSpy.count(), 3);

    QCOMPARE(modeAddedSpy.at(0).first().value<OutputDevice::Mode>().size, QSize(800, 600));
    QCOMPARE(modeAddedSpy.at(0).first().value<OutputDevice::Mode>().refreshRate, 60000);
    QCOMPARE(modeAddedSpy.at(0).first().value<OutputDevice::Mode>().flags, OutputDevice::Mode::Flags(OutputDevice::Mode::Flag::Preferred));
    QCOMPARE(modeAddedSpy.at(0).first().value<OutputDevice::Mode>().output, QPointer<OutputDevice>(&output));
    QVERIFY(modeAddedSpy.at(0).first().value<OutputDevice::Mode>().id > -1);

    QCOMPARE(modeAddedSpy.at(1).first().value<OutputDevice::Mode>().size, QSize(1280, 1024));
    QCOMPARE(modeAddedSpy.at(1).first().value<OutputDevice::Mode>().refreshRate, 90000);
    QCOMPARE(modeAddedSpy.at(1).first().value<OutputDevice::Mode>().flags, OutputDevice::Mode::Flags(OutputDevice::Mode::Flag::None));
    QCOMPARE(modeAddedSpy.at(1).first().value<OutputDevice::Mode>().output, QPointer<OutputDevice>(&output));
    QVERIFY(modeAddedSpy.at(1).first().value<OutputDevice::Mode>().id > -1);

    QCOMPARE(modeAddedSpy.at(2).first().value<OutputDevice::Mode>().size, QSize(1024, 768));
    QCOMPARE(modeAddedSpy.at(2).first().value<OutputDevice::Mode>().refreshRate, 60000);
    QCOMPARE(modeAddedSpy.at(2).first().value<OutputDevice::Mode>().flags, OutputDevice::Mode::Flags(OutputDevice::Mode::Flag::Current));
    QCOMPARE(modeAddedSpy.at(2).first().value<OutputDevice::Mode>().output, QPointer<OutputDevice>(&output));
    const QList<OutputDevice::Mode> &modes = output.modes();
    QVERIFY(modeAddedSpy.at(2).first().value<OutputDevice::Mode>().id > -1);
    QCOMPARE(modes.size(), 3);
    QCOMPARE(modes.at(0), modeAddedSpy.at(0).first().value<OutputDevice::Mode>());
    QCOMPARE(modes.at(1), modeAddedSpy.at(1).first().value<OutputDevice::Mode>());
    QCOMPARE(modes.at(2), modeAddedSpy.at(2).first().value<OutputDevice::Mode>());

    QCOMPARE(output.pixelSize(), QSize(1024, 768));

    // change the current mode
    outputChanged.clear();
    QSignalSpy modeChangedSpy(&output, &KWayland::Client::OutputDevice::modeChanged);
    QVERIFY(modeChangedSpy.isValid());
    m_serverOutputDevice->setCurrentMode(0);
    QVERIFY(doneSpy.wait());
    QCOMPARE(modeChangedSpy.size(), 2);
    // the one which lost the current flag
    QCOMPARE(modeChangedSpy.first().first().value<OutputDevice::Mode>().size, QSize(1024, 768));
    QCOMPARE(modeChangedSpy.first().first().value<OutputDevice::Mode>().refreshRate, 60000);
    QCOMPARE(modeChangedSpy.first().first().value<OutputDevice::Mode>().flags, OutputDevice::Mode::Flags());
    // the one which got the current flag
    QCOMPARE(modeChangedSpy.last().first().value<OutputDevice::Mode>().size, QSize(800, 600));
    QCOMPARE(modeChangedSpy.last().first().value<OutputDevice::Mode>().refreshRate, 60000);
    QCOMPARE(modeChangedSpy.last().first().value<OutputDevice::Mode>().flags, OutputDevice::Mode::Flags(OutputDevice::Mode::Flag::Current | OutputDevice::Mode::Flag::Preferred));
    QVERIFY(!outputChanged.isEmpty());
    QCOMPARE(output.pixelSize(), QSize(800, 600));
    const QList<OutputDevice::Mode> &modes2 = output.modes();
    QCOMPARE(modes2.at(0).size, QSize(1280, 1024));
    QCOMPARE(modes2.at(0).refreshRate, 90000);
    QCOMPARE(modes2.at(0).flags, OutputDevice::Mode::Flag::None);
    QCOMPARE(modes2.at(1).size, QSize(1024, 768));
    QCOMPARE(modes2.at(1).refreshRate, 60000);
    QCOMPARE(modes2.at(1).flags, OutputDevice::Mode::Flag::None);
    QCOMPARE(modes2.at(2).size, QSize(800, 600));
    QCOMPARE(modes2.at(2).refreshRate, 60000);
    QCOMPARE(modes2.at(2).flags, OutputDevice::Mode::Flag::Current | OutputDevice::Mode::Flag::Preferred);

    // change once more
    outputChanged.clear();
    modeChangedSpy.clear();
    m_serverOutputDevice->setCurrentMode(2);
    QVERIFY(doneSpy.wait());
    QCOMPARE(modeChangedSpy.size(), 2);
    // the one which lost the current flag
    QCOMPARE(modeChangedSpy.first().first().value<OutputDevice::Mode>().size, QSize(800, 600));
    QCOMPARE(modeChangedSpy.first().first().value<OutputDevice::Mode>().refreshRate, 60000);
    QCOMPARE(modeChangedSpy.first().first().value<OutputDevice::Mode>().flags, OutputDevice::Mode::Flags(OutputDevice::Mode::Flag::Preferred));
    // the one which got the current flag
    QCOMPARE(modeChangedSpy.last().first().value<OutputDevice::Mode>().size, QSize(1280, 1024));
    QCOMPARE(modeChangedSpy.last().first().value<OutputDevice::Mode>().refreshRate, 90000);
    QCOMPARE(modeChangedSpy.last().first().value<OutputDevice::Mode>().flags, OutputDevice::Mode::Flags(OutputDevice::Mode::Flag::Current));
    QVERIFY(!outputChanged.isEmpty());
    QCOMPARE(output.pixelSize(), QSize(1280, 1024));
}

#if KWAYLANDSERVER_ENABLE_DEPRECATED_SINCE(5, 50)
void TestWaylandOutputDevice::testScaleChange_legacy()
{
    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputDeviceAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    KWayland::Client::OutputDevice output;
    QSignalSpy outputChanged(&output, &KWayland::Client::OutputDevice::done);
    QVERIFY(outputChanged.isValid());
    output.setup(registry.bindOutputDevice(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 1);
    QCOMPARE(output.scaleF(), 1.0);

    // change the scale
    outputChanged.clear();
    m_serverOutputDevice->setScale(2);
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 2);
    QCOMPARE(output.scaleF(), 2.0); //check we're forward compatible


    // change once more
    outputChanged.clear();
    m_serverOutputDevice->setScale(4);
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scale(), 4);
    QCOMPARE(output.scaleF(), 4.0);
}
#endif

void TestWaylandOutputDevice::testScaleChange()
{
    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputDeviceAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    KWayland::Client::OutputDevice output;
    QSignalSpy outputChanged(&output, &KWayland::Client::OutputDevice::done);
    QVERIFY(outputChanged.isValid());
    output.setup(registry.bindOutputDevice(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.scaleF(), 1.0);

    // change the scale
    outputChanged.clear();
    m_serverOutputDevice->setScaleF(2.2);
    QVERIFY(outputChanged.wait());
#if KWAYLANDSERVER_ENABLE_DEPRECATED_SINCE(5, 50)
    QCOMPARE(output.scale(), 2); //check backwards compatibility works
#endif
    QCOMPARE(wl_fixed_from_double(output.scaleF()), wl_fixed_from_double(2.2));

    // change once more
    outputChanged.clear();
    m_serverOutputDevice->setScaleF(4.9);
    QVERIFY(outputChanged.wait());
#if KWAYLANDSERVER_ENABLE_DEPRECATED_SINCE(5, 50)
    QCOMPARE(output.scale(), 5);
#endif
    QCOMPARE(wl_fixed_from_double(output.scaleF()), wl_fixed_from_double(4.9));
}

void TestWaylandOutputDevice::testColorCurvesChange()
{
    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputDeviceAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    KWayland::Client::OutputDevice output;
    QSignalSpy outputChanged(&output, &KWayland::Client::OutputDevice::done);
    QVERIFY(outputChanged.isValid());
    output.setup(registry.bindOutputDevice(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.colorCurves().red, m_initColorCurves.red);
    QCOMPARE(output.colorCurves().green, m_initColorCurves.green);
    QCOMPARE(output.colorCurves().blue, m_initColorCurves.blue);

    // change the color curves
    outputChanged.clear();
    KWaylandServer::OutputDeviceInterface::ColorCurves cc;
    cc.red = QVector<quint16>(256, 0);
    cc.green = QVector<quint16>(256, UINT16_MAX);
    cc.blue = QVector<quint16>(320, 1);
    m_serverOutputDevice->setColorCurves(cc);
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.colorCurves().red, cc.red);
    QCOMPARE(output.colorCurves().green, cc.green);
    QCOMPARE(output.colorCurves().blue, cc.blue);

    // change once more
    outputChanged.clear();
    cc.red = QVector<quint16>(256, 0);
    cc.green = QVector<quint16>(256, UINT16_MAX);
    cc.blue = QVector<quint16>(320, UINT16_MAX);
    m_serverOutputDevice->setColorCurves(cc);
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.colorCurves().red, cc.red);
    QCOMPARE(output.colorCurves().green, cc.green);
    QCOMPARE(output.colorCurves().blue, cc.blue);
}

void TestWaylandOutputDevice::testSubPixel_data()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QTest::addColumn<KWayland::Client::OutputDevice::SubPixel>("expected");
    QTest::addColumn<KWaylandServer::OutputDeviceInterface::SubPixel>("actual");

    QTest::newRow("none") << OutputDevice::SubPixel::None << OutputDeviceInterface::SubPixel::None;
    QTest::newRow("horizontal/rgb") << OutputDevice::SubPixel::HorizontalRGB << OutputDeviceInterface::SubPixel::HorizontalRGB;
    QTest::newRow("horizontal/bgr") << OutputDevice::SubPixel::HorizontalBGR << OutputDeviceInterface::SubPixel::HorizontalBGR;
    QTest::newRow("vertical/rgb") << OutputDevice::SubPixel::VerticalRGB << OutputDeviceInterface::SubPixel::VerticalRGB;
    QTest::newRow("vertical/bgr") << OutputDevice::SubPixel::VerticalBGR << OutputDeviceInterface::SubPixel::VerticalBGR;
}

void TestWaylandOutputDevice::testSubPixel()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QFETCH(OutputDeviceInterface::SubPixel, actual);
    m_serverOutputDevice->setSubPixel(actual);

    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputDeviceAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    KWayland::Client::OutputDevice output;
    QSignalSpy outputChanged(&output, &KWayland::Client::OutputDevice::done);
    QVERIFY(outputChanged.isValid());
    output.setup(registry.bindOutputDevice(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());

    QTEST(output.subPixel(), "expected");

    // change back to unknown
    outputChanged.clear();
    m_serverOutputDevice->setSubPixel(OutputDeviceInterface::SubPixel::Unknown);
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.subPixel(), OutputDevice::SubPixel::Unknown);
}

void TestWaylandOutputDevice::testTransform_data()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QTest::addColumn<KWayland::Client::OutputDevice::Transform>("expected");
    QTest::addColumn<KWaylandServer::OutputDeviceInterface::Transform>("actual");

    QTest::newRow("90")          << OutputDevice::Transform::Rotated90  << OutputDeviceInterface::Transform::Rotated90;
    QTest::newRow("180")         << OutputDevice::Transform::Rotated180 << OutputDeviceInterface::Transform::Rotated180;
    QTest::newRow("270")         << OutputDevice::Transform::Rotated270 << OutputDeviceInterface::Transform::Rotated270;
    QTest::newRow("Flipped")     << OutputDevice::Transform::Flipped    << OutputDeviceInterface::Transform::Flipped;
    QTest::newRow("Flipped 90")  << OutputDevice::Transform::Flipped90  << OutputDeviceInterface::Transform::Flipped90;
    QTest::newRow("Flipped 180") << OutputDevice::Transform::Flipped180 << OutputDeviceInterface::Transform::Flipped180;
    QTest::newRow("Flipped 280") << OutputDevice::Transform::Flipped270 << OutputDeviceInterface::Transform::Flipped270;
}

void TestWaylandOutputDevice::testTransform()
{
    using namespace KWayland::Client;
    using namespace KWaylandServer;
    QFETCH(OutputDeviceInterface::Transform, actual);
    m_serverOutputDevice->setTransform(actual);

    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputDeviceAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    KWayland::Client::OutputDevice *output = registry.createOutputDevice(announced.first().first().value<quint32>(), announced.first().last().value<quint32>(), &registry);
    QSignalSpy outputChanged(output, &KWayland::Client::OutputDevice::done);
    QVERIFY(outputChanged.isValid());
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());

    QTEST(output->transform(), "expected");

    // change back to normal
    outputChanged.clear();
    m_serverOutputDevice->setTransform(OutputDeviceInterface::Transform::Normal);
    QVERIFY(outputChanged.wait());
    QCOMPARE(output->transform(), OutputDevice::Transform::Normal);
}

void TestWaylandOutputDevice::testEnabled()
{
    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputDeviceAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    KWayland::Client::OutputDevice output;
    QSignalSpy outputChanged(&output, &KWayland::Client::OutputDevice::done);
    QVERIFY(outputChanged.isValid());
    output.setup(registry.bindOutputDevice(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());

    QCOMPARE(output.enabled(), OutputDevice::Enablement::Enabled);

    QSignalSpy changed(&output, &KWayland::Client::OutputDevice::changed);
    QSignalSpy enabledChanged(&output, &KWayland::Client::OutputDevice::enabledChanged);
    QVERIFY(enabledChanged.isValid());

    m_serverOutputDevice->setEnabled(OutputDeviceInterface::Enablement::Disabled);
    QVERIFY(enabledChanged.wait());
    QCOMPARE(output.enabled(), OutputDevice::Enablement::Disabled);
    if (changed.count() != enabledChanged.count()) {
        QVERIFY(changed.wait());
    }
    QCOMPARE(changed.count(), enabledChanged.count());

    m_serverOutputDevice->setEnabled(OutputDeviceInterface::Enablement::Enabled);
    QVERIFY(enabledChanged.wait());
    QCOMPARE(output.enabled(), OutputDevice::Enablement::Enabled);
    if (changed.count() != enabledChanged.count()) {
        QVERIFY(changed.wait());
    }
    QCOMPARE(changed.count(), enabledChanged.count());
}

void TestWaylandOutputDevice::testEdid()
{
    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputDeviceAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    KWayland::Client::OutputDevice output;

    QCOMPARE(output.edid(), QByteArray());

    QSignalSpy outputChanged(&output, &KWayland::Client::OutputDevice::done);
    QVERIFY(outputChanged.isValid());
    output.setup(registry.bindOutputDevice(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());
    QCOMPARE(output.edid(), m_edid);
}

void TestWaylandOutputDevice::testId()
{
    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputDeviceAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    KWayland::Client::OutputDevice output;
    QSignalSpy outputChanged(&output, &KWayland::Client::OutputDevice::done);
    QVERIFY(outputChanged.isValid());
    output.setup(registry.bindOutputDevice(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputChanged.wait());

    QCOMPARE(output.uuid(), QByteArray("1337"));

    QSignalSpy idChanged(&output, &KWayland::Client::OutputDevice::uuidChanged);
    QVERIFY(idChanged.isValid());

    m_serverOutputDevice->setUuid("42");
    QVERIFY(idChanged.wait());
    QCOMPARE(idChanged.first().first().toByteArray(), QByteArray("42"));
    idChanged.clear();
    QCOMPARE(output.uuid(), QByteArray("42"));

    m_serverOutputDevice->setUuid("4711");
    QVERIFY(idChanged.wait());
    QCOMPARE(idChanged.first().first().toByteArray(), QByteArray("4711"));
    idChanged.clear();
    QCOMPARE(output.uuid(), QByteArray("4711"));
}

void TestWaylandOutputDevice::testDone()
{
    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy announced(&registry, &KWayland::Client::Registry::outputDeviceAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    wl_display_flush(m_connection->display());
    QVERIFY(interfacesAnnouncedSpy.wait());

    KWayland::Client::OutputDevice output;
    QSignalSpy outputDone(&output, &KWayland::Client::OutputDevice::done);
    QVERIFY(outputDone.isValid());
    output.setup(registry.bindOutputDevice(announced.first().first().value<quint32>(), announced.first().last().value<quint32>()));
    wl_display_flush(m_connection->display());
    QVERIFY(outputDone.wait());
}


QTEST_GUILESS_MAIN(TestWaylandOutputDevice)
#include "test_wayland_outputdevice.moc"
