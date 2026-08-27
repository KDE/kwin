/*
    SPDX-FileCopyrightText: 2026 Sebastian Müller <thxgiving@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QObject>
#include <QSignalSpy>
#include <QTest>
#include <QThread>

#include "wayland/compositor.h"
#include "wayland/display.h"
#include "wayland/seat.h"
#include "wayland/textinput_v2.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"

#include "qwayland-text-input-unstable-v2.h"

using namespace KWin;

class TextInputV2 : public QtWayland::zwp_text_input_v2
{
public:
    ~TextInputV2() override
    {
        destroy();
    }
};

class TextInputManagerV2 : public QtWayland::zwp_text_input_manager_v2
{
public:
    ~TextInputManagerV2() override
    {
        destroy();
    }
};

class TestTextInputV2Interface : public QObject
{
    Q_OBJECT

public:
    ~TestTextInputV2Interface() override;

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testSurroundingTextOffsetsClamped();

private:
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    KWayland::Client::Compositor *m_clientCompositor = nullptr;
    KWayland::Client::Seat *m_clientSeat = nullptr;

    SeatInterface *m_seat = nullptr;
    QThread *m_thread = nullptr;
    KWin::Display m_display;
    TextInputV2 *m_clientTextInputV2 = nullptr;
    CompositorInterface *m_serverCompositor = nullptr;
    TextInputV2Interface *m_serverTextInputV2 = nullptr;
    TextInputManagerV2 *m_clientTextInputManagerV2 = nullptr;
};

void TestTextInputV2Interface::initTestCase()
{
    m_display.addSocketName(qAppName());
    m_display.start();
    QVERIFY(m_display.isRunning());

    m_seat = new SeatInterface(&m_display, QStringLiteral("seat0"), this);
    m_seat->setHasKeyboard(true);

    m_serverCompositor = new CompositorInterface(&m_display, this);
    new TextInputManagerV2Interface(&m_display);
}

void TestTextInputV2Interface::init()
{
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(qAppName());

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());
    QVERIFY(!m_connection->connections().isEmpty());

    m_queue = new KWayland::Client::EventQueue(this);
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    auto registry = new KWayland::Client::Registry(this);
    connect(registry, &KWayland::Client::Registry::interfaceAnnounced, this, [this, registry](const QByteArray &interface, quint32 id, quint32 version) {
        if (interface == QByteArrayLiteral("zwp_text_input_manager_v2")) {
            m_clientTextInputManagerV2 = new TextInputManagerV2();
            m_clientTextInputManagerV2->init(*registry, id, version);
        }
    });
    connect(registry, &KWayland::Client::Registry::seatAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_clientSeat = registry->createSeat(name, version);
    });

    QSignalSpy allAnnouncedSpy(registry, &KWayland::Client::Registry::interfaceAnnounced);
    QSignalSpy compositorSpy(registry, &KWayland::Client::Registry::compositorAnnounced);
    registry->setEventQueue(m_queue);
    registry->create(m_connection->display());
    QVERIFY(registry->isValid());
    registry->setup();
    QVERIFY(allAnnouncedSpy.wait());

    m_clientCompositor = registry->createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_clientCompositor->isValid());
    QVERIFY(m_clientTextInputManagerV2);

    m_clientTextInputV2 = new TextInputV2();
    m_clientTextInputV2->init(m_clientTextInputManagerV2->get_text_input(*m_clientSeat));

    delete registry;
}

void TestTextInputV2Interface::cleanup()
{
    m_serverTextInputV2 = nullptr;
    if (m_clientTextInputV2) {
        delete m_clientTextInputV2;
        m_clientTextInputV2 = nullptr;
    }
    if (m_clientTextInputManagerV2) {
        delete m_clientTextInputManagerV2;
        m_clientTextInputManagerV2 = nullptr;
    }
    if (m_clientCompositor) {
        delete m_clientCompositor;
        m_clientCompositor = nullptr;
    }
    if (m_clientSeat) {
        delete m_clientSeat;
        m_clientSeat = nullptr;
    }
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
    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
}

TestTextInputV2Interface::~TestTextInputV2Interface() = default;

void TestTextInputV2Interface::testSurroundingTextOffsetsClamped()
{
    m_serverTextInputV2 = m_seat->textInputV2();
    QVERIFY(m_serverTextInputV2);

    QSignalSpy surroundingTextChangedSpy(m_serverTextInputV2, &TextInputV2Interface::surroundingTextChanged);

    // cursor and anchor outside the text must never end up in the stored state
    m_clientTextInputV2->set_surrounding_text(QStringLiteral("KDE"), 1000000, -7);
    QVERIFY(surroundingTextChangedSpy.wait());

    QCOMPARE(m_serverTextInputV2->surroundingText(), QStringLiteral("KDE"));
    QCOMPARE(m_serverTextInputV2->surroundingTextCursorPosition(), 3);
    QCOMPARE(m_serverTextInputV2->surroundingTextSelectionAnchor(), 0);

    // offsets are clamped to the UTF-8 byte length, not the UTF-16 length
    m_clientTextInputV2->set_surrounding_text(QStringLiteral("KDÉ"), 1000000, 4);
    QVERIFY(surroundingTextChangedSpy.wait());

    QCOMPARE(m_serverTextInputV2->surroundingTextCursorPosition(), 4);
    QCOMPARE(m_serverTextInputV2->surroundingTextSelectionAnchor(), 4);
}

QTEST_GUILESS_MAIN(TestTextInputV2Interface)

#include "test_textinputv2_interface.moc"
