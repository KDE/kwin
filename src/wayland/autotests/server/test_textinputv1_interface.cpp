/*
    SPDX-FileCopyrightText: 2020 Bhushan Shah <bshah@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QThread>
#include <QtTest>

#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/seat_interface.h"
#include "wayland/surface_interface.h"
#include "wayland/textinput_v1_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/surface.h"

#include "qwayland-text-input-unstable-v1.h"

using namespace KWaylandServer;

Q_DECLARE_METATYPE(QtWayland::zwp_text_input_v1::content_purpose)
Q_DECLARE_METATYPE(QtWayland::zwp_text_input_v1::content_hint)

class TextInputV1 : public QObject, public QtWayland::zwp_text_input_v1
{
    Q_OBJECT
Q_SIGNALS:
    void surface_enter(wl_surface *surface);
    void surface_leave();
    void commit_string(const QString &text);
    void delete_surrounding_text(qint32 index, quint32 length);
    void preedit_string(const QString &text, const QString &commit);

public:
    void zwp_text_input_v1_enter(struct ::wl_surface *surface) override
    {
        Q_EMIT surface_enter(surface);
    }
    void zwp_text_input_v1_leave() override
    {
        Q_EMIT surface_leave();
    }
    void zwp_text_input_v1_commit_string(uint32_t serial, const QString &text) override
    {
        Q_EMIT commit_string(text);
    }
    void zwp_text_input_v1_delete_surrounding_text(int32_t index, uint32_t length) override
    {
        Q_EMIT delete_surrounding_text(index, length);
    }
    void zwp_text_input_v1_preedit_string(uint32_t serial, const QString &text, const QString &commit) override
    {
        Q_EMIT preedit_string(text, commit);
    }
};

class TextInputManagerV1 : public QtWayland::zwp_text_input_manager_v1
{
public:
    ~TextInputManagerV1() override
    {
    }
};

class TestTextInputV1Interface : public QObject
{
    Q_OBJECT

public:
    ~TestTextInputV1Interface() override;

private Q_SLOTS:
    void initTestCase();
    void testEnableDisable();
    void testEvents();
    void testContentPurpose_data();
    void testContentPurpose();
    void testContentHints_data();
    void testContentHints();

private:
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    KWayland::Client::Compositor *m_clientCompositor = nullptr;
    KWayland::Client::Seat *m_clientSeat = nullptr;

    SeatInterface *m_seat;
    QThread *m_thread;
    KWaylandServer::Display m_display;
    TextInputV1 *m_clientTextInputV1;
    CompositorInterface *m_serverCompositor;
    TextInputV1Interface *m_serverTextInputV1;
    TextInputManagerV1 *m_clientTextInputManagerV1;

    quint32 m_totalCommits = 0;
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-text-input-v1-test-0");

void TestTextInputV1Interface::initTestCase()
{
    m_display.addSocketName(s_socketName);
    m_display.start();
    QVERIFY(m_display.isRunning());

    m_seat = new SeatInterface(&m_display, this);
    m_seat->setHasKeyboard(true);

    m_serverCompositor = new CompositorInterface(&m_display, this);
    new TextInputManagerV1Interface(&m_display);

    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());
    QVERIFY(!m_connection->connections().isEmpty());

    m_queue = new KWayland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    auto registry = new KWayland::Client::Registry(this);
    connect(registry, &KWayland::Client::Registry::interfaceAnnounced, this, [this, registry](const QByteArray &interface, quint32 id, quint32 version) {
        if (interface == QByteArrayLiteral("zwp_text_input_manager_v1")) {
            m_clientTextInputManagerV1 = new TextInputManagerV1();
            m_clientTextInputManagerV1->init(*registry, id, version);
        }
    });

    connect(registry, &KWayland::Client::Registry::seatAnnounced, this, [this, registry](quint32 name, quint32 version) {
        m_clientSeat = registry->createSeat(name, version);
    });

    QSignalSpy allAnnouncedSpy(registry, &KWayland::Client::Registry::interfaceAnnounced);
    QSignalSpy compositorSpy(registry, &KWayland::Client::Registry::compositorAnnounced);
    QSignalSpy shmSpy(registry, &KWayland::Client::Registry::shmAnnounced);
    registry->setEventQueue(m_queue);
    registry->create(m_connection->display());
    QVERIFY(registry->isValid());
    registry->setup();
    QVERIFY(allAnnouncedSpy.wait());

    m_clientCompositor = registry->createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_clientCompositor->isValid());
    // create a text input v1
    m_clientTextInputV1 = new TextInputV1();
    m_clientTextInputV1->init(m_clientTextInputManagerV1->create_text_input());
    QVERIFY(m_clientTextInputV1);
}

TestTextInputV1Interface::~TestTextInputV1Interface()
{
    if (m_clientTextInputV1) {
        delete m_clientTextInputV1;
        m_clientTextInputV1 = nullptr;
    }
    if (m_clientTextInputManagerV1) {
        delete m_clientTextInputManagerV1;
        m_clientTextInputManagerV1 = nullptr;
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
    m_connection->deleteLater();
    m_connection = nullptr;
}

// Ensures that enable disable events don't fire without commit
void TestTextInputV1Interface::testEnableDisable()
{
    // create a surface
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    m_serverTextInputV1 = m_seat->textInputV1();
    QVERIFY(m_serverTextInputV1);

    QSignalSpy focusedSurfaceChangedSpy(m_seat, &SeatInterface::focusedTextInputSurfaceChanged);
    QSignalSpy textInputEnabledSpy(m_serverTextInputV1, &TextInputV1Interface::enabledChanged);
    QSignalSpy cursorRectangleChangedSpy(m_serverTextInputV1, &TextInputV1Interface::cursorRectangleChanged);

    QSignalSpy surfaceEnterSpy(m_clientTextInputV1, &TextInputV1::surface_enter);
    QSignalSpy surfaceLeaveSpy(m_clientTextInputV1, &TextInputV1::surface_leave);

    // Enter the textinput

    QCOMPARE(focusedSurfaceChangedSpy.count(), 0);

    // Make sure that entering surface does not trigger the text input
    m_seat->setFocusedTextInputSurface(serverSurface);
    QCOMPARE(focusedSurfaceChangedSpy.count(), 1);
    QCOMPARE(surfaceEnterSpy.count(), 0);
    QCOMPARE(textInputEnabledSpy.count(), 0);

    // Now enable the textInput, we should not get event just yet
    m_clientTextInputV1->activate(*m_clientSeat, *clientSurface);
    m_clientTextInputV1->set_cursor_rectangle(0, 0, 20, 20);
    m_clientTextInputV1->set_surrounding_text("KDE Plasma Desktop", 0, 3);
    QVERIFY(surfaceEnterSpy.wait());

    QCOMPARE(textInputEnabledSpy.count(), 1);
    QCOMPARE(cursorRectangleChangedSpy.count(), 1);
    QCOMPARE(m_serverTextInputV1->cursorRectangle(), QRect(0, 0, 20, 20));
    QCOMPARE(m_serverTextInputV1->surroundingText(), QString("KDE Plasma Desktop"));
    QCOMPARE(m_serverTextInputV1->surroundingTextCursorPosition(), 0);
    QCOMPARE(m_serverTextInputV1->surroundingTextSelectionAnchor(), 3);

    // disabling we should get the event
    m_clientTextInputV1->deactivate(*m_clientSeat);
    QVERIFY(textInputEnabledSpy.wait());
    QCOMPARE(textInputEnabledSpy.count(), 2);
    QVERIFY(surfaceLeaveSpy.wait());

    // Lets try leaving the surface and make sure event propogage
    m_seat->setFocusedTextInputSurface(nullptr);
    QCOMPARE(surfaceLeaveSpy.count(), 1);
}

void TestTextInputV1Interface::testEvents()
{
    // create a surface
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    m_serverTextInputV1 = m_seat->textInputV1();
    QVERIFY(m_serverTextInputV1);

    QSignalSpy focusedSurfaceChangedSpy(m_seat, &SeatInterface::focusedTextInputSurfaceChanged);
    QSignalSpy textInputEnabledSpy(m_serverTextInputV1, &TextInputV1Interface::enabledChanged);

    // Enter the textinput
    QCOMPARE(focusedSurfaceChangedSpy.count(), 0);

    // Make sure that entering surface does not trigger the text input
    m_seat->setFocusedTextInputSurface(serverSurface);
    // FIXME: somehow this triggers BEFORE setFocusedTextInputSurface returns :(
    //  QVERIFY(focusedSurfaceChangedSpy.wait());
    QCOMPARE(focusedSurfaceChangedSpy.count(), 1);

    // Now enable the textInput
    m_clientTextInputV1->activate(*m_clientSeat, *clientSurface);
    QVERIFY(textInputEnabledSpy.wait());

    QSignalSpy preEditSpy(m_clientTextInputV1, &TextInputV1::preedit_string);
    QSignalSpy commitStringSpy(m_clientTextInputV1, &TextInputV1::commit_string);
    QSignalSpy deleteSurroundingSpy(m_clientTextInputV1, &TextInputV1::delete_surrounding_text);

    m_serverTextInputV1->preEdit("Hello KDE community!", "Hello");
    m_serverTextInputV1->deleteSurroundingText(6, 10);
    m_serverTextInputV1->commitString("Plasma");

    // Wait for the last update
    QVERIFY(commitStringSpy.wait());

    QCOMPARE(preEditSpy.last().at(0).value<QString>(), "Hello KDE community!");
    QCOMPARE(preEditSpy.last().at(1).value<QString>(), "Hello");
    QCOMPARE(commitStringSpy.last().at(0).value<QString>(), "Plasma");
    QCOMPARE(deleteSurroundingSpy.last().at(0).value<quint32>(), 6);
    QCOMPARE(deleteSurroundingSpy.last().at(1).value<quint32>(), 10);

    // Now disable the textInput
    m_clientTextInputV1->deactivate(*m_clientSeat);
    QVERIFY(textInputEnabledSpy.wait());
}

void TestTextInputV1Interface::testContentPurpose_data()
{
    QTest::addColumn<QtWayland::zwp_text_input_v1::content_purpose>("clientPurpose");
    QTest::addColumn<KWaylandServer::TextInputContentPurpose>("serverPurpose");

    QTest::newRow("Alpha") << QtWayland::zwp_text_input_v1::content_purpose_alpha << TextInputContentPurpose::Alpha;
    QTest::newRow("Digits") << QtWayland::zwp_text_input_v1::content_purpose_digits << TextInputContentPurpose::Digits;
    QTest::newRow("Number") << QtWayland::zwp_text_input_v1::content_purpose_number << TextInputContentPurpose::Number;
    QTest::newRow("Phone") << QtWayland::zwp_text_input_v1::content_purpose_phone << TextInputContentPurpose::Phone;
    QTest::newRow("Url") << QtWayland::zwp_text_input_v1::content_purpose_url << TextInputContentPurpose::Url;
    QTest::newRow("Email") << QtWayland::zwp_text_input_v1::content_purpose_email << TextInputContentPurpose::Email;
    QTest::newRow("Name") << QtWayland::zwp_text_input_v1::content_purpose_name << TextInputContentPurpose::Name;
    QTest::newRow("Password") << QtWayland::zwp_text_input_v1::content_purpose_password << TextInputContentPurpose::Password;
    QTest::newRow("Date") << QtWayland::zwp_text_input_v1::content_purpose_date << TextInputContentPurpose::Date;
    QTest::newRow("Time") << QtWayland::zwp_text_input_v1::content_purpose_time << TextInputContentPurpose::Time;
    QTest::newRow("DateTime") << QtWayland::zwp_text_input_v1::content_purpose_datetime << TextInputContentPurpose::DateTime;
    QTest::newRow("Terminal") << QtWayland::zwp_text_input_v1::content_purpose_terminal << TextInputContentPurpose::Terminal;
}

void TestTextInputV1Interface::testContentPurpose()
{
    // create a surface
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    m_serverTextInputV1 = m_seat->textInputV1();
    QVERIFY(m_serverTextInputV1);

    QSignalSpy focusedSurfaceChangedSpy(m_seat, &SeatInterface::focusedTextInputSurfaceChanged);
    QSignalSpy textInputEnabledSpy(m_serverTextInputV1, &TextInputV1Interface::enabledChanged);

    // Enter the textinput
    QCOMPARE(focusedSurfaceChangedSpy.count(), 0);

    // Make sure that entering surface does not trigger the text input
    m_seat->setFocusedTextInputSurface(serverSurface);
    // FIXME: somehow this triggers BEFORE setFocusedTextInputSurface returns :(
    //  QVERIFY(focusedSurfaceChangedSpy.wait());
    QCOMPARE(focusedSurfaceChangedSpy.count(), 1);

    // Now enable the textInput
    m_clientTextInputV1->activate(*m_clientSeat, *clientSurface);
    QVERIFY(textInputEnabledSpy.wait());
    m_totalCommits++;

    // Default should be normal content purpose
    QCOMPARE(m_serverTextInputV1->contentPurpose(), TextInputContentPurpose::Normal);

    QSignalSpy contentTypeChangedSpy(m_serverTextInputV1, &TextInputV1Interface::contentTypeChanged);

    QFETCH(QtWayland::zwp_text_input_v1::content_purpose, clientPurpose);
    m_clientTextInputV1->activate(*m_clientSeat, *clientSurface);
    m_clientTextInputV1->set_content_type(QtWayland::zwp_text_input_v1::content_hint_none, clientPurpose);
    QVERIFY(contentTypeChangedSpy.wait());
    QTEST(m_serverTextInputV1->contentPurpose(), "serverPurpose");
    m_totalCommits++;

    // Setting same thing should not trigger update
    m_clientTextInputV1->activate(*m_clientSeat, *clientSurface);
    m_clientTextInputV1->set_content_type(QtWayland::zwp_text_input_v1::content_hint_none, clientPurpose);
    QVERIFY(!contentTypeChangedSpy.wait(100));
    m_totalCommits++;

    // unset to normal
    m_clientTextInputV1->activate(*m_clientSeat, *clientSurface);
    m_clientTextInputV1->set_content_type(QtWayland::zwp_text_input_v1::content_hint_none, QtWayland::zwp_text_input_v1::content_purpose_normal);
    QVERIFY(contentTypeChangedSpy.wait());
    m_totalCommits++;
    QCOMPARE(m_serverTextInputV1->contentPurpose(), TextInputContentPurpose::Normal);

    // Now disable the textInput
    m_clientTextInputV1->deactivate(*m_clientSeat);
    m_totalCommits++;
    QVERIFY(textInputEnabledSpy.wait());
}

void TestTextInputV1Interface::testContentHints_data()
{
    QTest::addColumn<quint32>("clientHint");
    QTest::addColumn<KWaylandServer::TextInputContentHints>("serverHints");

    QTest::addRow("Spellcheck") << quint32(QtWayland::zwp_text_input_v1::content_hint_auto_correction)
                                << TextInputContentHints(TextInputContentHint::AutoCorrection);
    QTest::addRow("Completion") << quint32(QtWayland::zwp_text_input_v1::content_hint_auto_completion)
                                << TextInputContentHints(TextInputContentHint::AutoCompletion);
    QTest::addRow("AutoCapital") << quint32(QtWayland::zwp_text_input_v1::content_hint_auto_capitalization)
                                 << TextInputContentHints(TextInputContentHint::AutoCapitalization);
    QTest::addRow("Lowercase") << quint32(QtWayland::zwp_text_input_v1::content_hint_lowercase) << TextInputContentHints(TextInputContentHint::LowerCase);
    QTest::addRow("Uppercase") << quint32(QtWayland::zwp_text_input_v1::content_hint_uppercase) << TextInputContentHints(TextInputContentHint::UpperCase);
    QTest::addRow("Titlecase") << quint32(QtWayland::zwp_text_input_v1::content_hint_titlecase) << TextInputContentHints(TextInputContentHint::TitleCase);
    QTest::addRow("HiddenText") << quint32(QtWayland::zwp_text_input_v1::content_hint_hidden_text) << TextInputContentHints(TextInputContentHint::HiddenText);
    QTest::addRow("SensitiveData") << quint32(QtWayland::zwp_text_input_v1::content_hint_sensitive_data)
                                   << TextInputContentHints(TextInputContentHint::SensitiveData);
    QTest::addRow("Latin") << quint32(QtWayland::zwp_text_input_v1::content_hint_latin) << TextInputContentHints(TextInputContentHint::Latin);
    QTest::addRow("Multiline") << quint32(QtWayland::zwp_text_input_v1::content_hint_multiline) << TextInputContentHints(TextInputContentHint::MultiLine);
    QTest::addRow("Auto") << quint32(QtWayland::zwp_text_input_v1::content_hint_auto_completion | QtWayland::zwp_text_input_v1::content_hint_auto_correction
                                     | QtWayland::zwp_text_input_v1::content_hint_auto_capitalization)
                          << TextInputContentHints(TextInputContentHint::AutoCompletion | TextInputContentHint::AutoCorrection
                                                   | TextInputContentHint::AutoCapitalization);
}

void TestTextInputV1Interface::testContentHints()
{
    // create a surface
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    std::unique_ptr<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    m_serverTextInputV1 = m_seat->textInputV1();
    QVERIFY(m_serverTextInputV1);

    QSignalSpy focusedSurfaceChangedSpy(m_seat, &SeatInterface::focusedTextInputSurfaceChanged);
    QSignalSpy textInputEnabledSpy(m_serverTextInputV1, &TextInputV1Interface::enabledChanged);

    // Enter the textinput
    QCOMPARE(focusedSurfaceChangedSpy.count(), 0);

    // Make sure that entering surface does not trigger the text input
    m_seat->setFocusedTextInputSurface(serverSurface);
    // FIXME: somehow this triggers BEFORE setFocusedTextInputSurface returns :(
    //  QVERIFY(focusedSurfaceChangedSpy.wait());
    QCOMPARE(focusedSurfaceChangedSpy.count(), 1);

    // Now enable the textInput
    m_clientTextInputV1->activate(*m_clientSeat, *clientSurface);
    QVERIFY(textInputEnabledSpy.wait());
    m_totalCommits++;

    QCOMPARE(m_serverTextInputV1->contentHints(), TextInputContentHint::None);

    // Now disable the textInput
    m_clientTextInputV1->deactivate(*m_clientSeat);
    QVERIFY(textInputEnabledSpy.wait());
    m_totalCommits++;

    QSignalSpy contentTypeChangedSpy(m_serverTextInputV1, &TextInputV1Interface::contentTypeChanged);

    QFETCH(quint32, clientHint);
    m_clientTextInputV1->activate(*m_clientSeat, *clientSurface);
    m_clientTextInputV1->set_content_type(clientHint, QtWayland::zwp_text_input_v1::content_purpose_normal);
    QVERIFY(contentTypeChangedSpy.wait());
    QTEST(m_serverTextInputV1->contentHints(), "serverHints");
    m_totalCommits++;

    // Setting same thing should not trigger update
    m_clientTextInputV1->activate(*m_clientSeat, *clientSurface);
    m_clientTextInputV1->set_content_type(clientHint, QtWayland::zwp_text_input_v1::content_purpose_normal);
    QVERIFY(!contentTypeChangedSpy.wait(100));
    m_totalCommits++;

    // unset to normal
    m_clientTextInputV1->activate(*m_clientSeat, *clientSurface);
    m_clientTextInputV1->set_content_type(QtWayland::zwp_text_input_v1::content_hint_none, QtWayland::zwp_text_input_v1::content_purpose_normal);
    QVERIFY(contentTypeChangedSpy.wait());
    m_totalCommits++;

    // Now disable the textInput
    m_clientTextInputV1->deactivate(*m_clientSeat);
    QVERIFY(textInputEnabledSpy.wait());
    m_totalCommits++;
}

QTEST_GUILESS_MAIN(TestTextInputV1Interface)

#include "test_textinputv1_interface.moc"
