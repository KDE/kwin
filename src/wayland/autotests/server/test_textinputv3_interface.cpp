/*
    SPDX-FileCopyrightText: 2020 Bhushan Shah <bshah@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QThread>
#include <QtTest>

#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/surface_interface.h"
#include "../../src/server/seat_interface.h"
#include "../../src/server/textinput_v3_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/surface.h"

#include "qwayland-text-input-unstable-v3.h"

using namespace KWaylandServer;

Q_DECLARE_METATYPE(QtWayland::zwp_text_input_v3::content_purpose)
Q_DECLARE_METATYPE(QtWayland::zwp_text_input_v3::content_hint)

class TextInputV3 : public QObject, public QtWayland::zwp_text_input_v3
{
    Q_OBJECT
Q_SIGNALS:
    void surface_enter(wl_surface *surface);
    void surface_leave(wl_surface *surface);
    void commit_string(const QString & text);
    void delete_surrounding_text(quint32 before_length, quint32 after_length);
    void preedit_string(const QString &text, quint32 cursor_begin, quint32 cursor_end);
    void done(quint32 serial);

public:
    ~TextInputV3() override { destroy(); }
    void zwp_text_input_v3_enter(struct ::wl_surface * surface) override {
        emit surface_enter(surface);
    }
    void zwp_text_input_v3_leave(struct ::wl_surface * surface) override {
        emit surface_leave(surface);
    }
    void zwp_text_input_v3_commit_string(const QString & text) override {
        commitText = text;
    }
    void zwp_text_input_v3_delete_surrounding_text(uint32_t before_length, uint32_t after_length) override {
        before = before_length;
        after = after_length;
    }
    void zwp_text_input_v3_done(uint32_t serial) override {
        emit commit_string(commitText);
        emit preedit_string(preeditText, cursorBegin, cursorEnd);
        emit delete_surrounding_text(before, after);
        emit done(serial);
    }
    void zwp_text_input_v3_preedit_string(const QString &text, int32_t cursor_begin, int32_t cursor_end) override {
        preeditText = text;
        cursorBegin = cursor_begin;
        cursorEnd = cursor_end;
    }
private:
    QString preeditText;
    QString commitText;
    uint32_t cursorBegin, cursorEnd;
    uint32_t before, after;
};

class TextInputManagerV3 : public QtWayland::zwp_text_input_manager_v3
{
public:
    ~TextInputManagerV3() override { destroy(); }
};

class TestTextInputV3Interface : public QObject
{
    Q_OBJECT

public:
    ~TestTextInputV3Interface() override;

private Q_SLOTS:
    void initTestCase();
    void testEnableDisable();
    void testEvents();
    void testContentPurpose_data();
    void testContentPurpose();
    void testContentHints_data();
    void testContentHints();
    void testMultipleTextinputs();

private:
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::Compositor *m_clientCompositor;
    KWayland::Client::Seat *m_clientSeat = nullptr;

    SeatInterface *m_seat;
    QThread *m_thread;
    Display m_display;
    TextInputV3 *m_clientTextInputV3;
    CompositorInterface *m_serverCompositor;
    TextInputV3Interface *m_serverTextInputV3;
    TextInputManagerV3 *m_clientTextInputManagerV3;

    quint32 m_totalCommits = 0;
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-text-input-v3-test-0");

void TestTextInputV3Interface::initTestCase()
{
    m_display.addSocketName(s_socketName);
    m_display.start();
    QVERIFY(m_display.isRunning());

    m_seat = new SeatInterface(&m_display, this);
    m_seat->setHasKeyboard(true);
    m_seat->create();

    m_serverCompositor = new CompositorInterface(&m_display, this);
    new TextInputManagerV3Interface(&m_display);

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
        if (interface == QByteArrayLiteral("zwp_text_input_manager_v3")) {
            m_clientTextInputManagerV3 = new TextInputManagerV3();
            m_clientTextInputManagerV3->init(*registry, id, version);
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

    m_clientCompositor = registry->createCompositor(compositorSpy.first().first().value<quint32>(),
                                                    compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_clientCompositor->isValid());
    // create a text input v3
    m_clientTextInputV3 = new TextInputV3();
    m_clientTextInputV3->init(m_clientTextInputManagerV3->get_text_input(*m_clientSeat));
    QVERIFY(m_clientTextInputV3);
}

TestTextInputV3Interface::~TestTextInputV3Interface()
{
    if (m_clientTextInputV3) {
        delete m_clientTextInputV3;
        m_clientTextInputV3 = nullptr;
    }
    if (m_clientTextInputManagerV3) {
        delete m_clientTextInputManagerV3;
        m_clientTextInputManagerV3 = nullptr;
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
void TestTextInputV3Interface::testEnableDisable()
{
    // create a surface
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreatedSpy.isValid());
    QScopedPointer<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    m_serverTextInputV3 = m_seat->textInputV3();
    QVERIFY(m_serverTextInputV3);

    QSignalSpy focusedSurfaceChangedSpy(m_seat, &SeatInterface::focusedTextInputSurfaceChanged);
    QSignalSpy textInputEnabledSpy(m_serverTextInputV3, &TextInputV3Interface::enabledChanged);
    QSignalSpy cursorRectangleChangedSpy(m_serverTextInputV3, &TextInputV3Interface::cursorRectangleChanged);

    QSignalSpy surfaceEnterSpy(m_clientTextInputV3, &TextInputV3::surface_enter);
    QSignalSpy surfaceLeaveSpy(m_clientTextInputV3, &TextInputV3::surface_leave);

    // Enter the textinput
    QVERIFY(focusedSurfaceChangedSpy.isValid());
    QVERIFY(textInputEnabledSpy.isValid());

    QCOMPARE(focusedSurfaceChangedSpy.count(), 0);

    // Make sure that entering surface does not trigger the text input
    m_seat->setFocusedTextInputSurface(serverSurface);
    QVERIFY(surfaceEnterSpy.wait());
    QCOMPARE(surfaceEnterSpy.count(), 1);
    QCOMPARE(focusedSurfaceChangedSpy.count(), 1);
    QCOMPARE(textInputEnabledSpy.count(), 0);

    // Now enable the textInput, we should not get event just yet
    m_clientTextInputV3->enable();
    m_clientTextInputV3->set_cursor_rectangle(0, 0, 20, 20);
    m_clientTextInputV3->set_surrounding_text("KDE Plasma Desktop", 0, 3);
    QCOMPARE(textInputEnabledSpy.count(), 0);
    QCOMPARE(cursorRectangleChangedSpy.count(), 0);

    // after we do commit we should get event
    m_clientTextInputV3->commit();
    QVERIFY(textInputEnabledSpy.wait());
    m_totalCommits++;

    QCOMPARE(textInputEnabledSpy.count(), 1);
    QCOMPARE(cursorRectangleChangedSpy.count(), 1);
    QCOMPARE(m_serverTextInputV3->cursorRectangle(), QRect(0, 0, 20, 20));
    QCOMPARE(m_serverTextInputV3->surroundingText(), QString("KDE Plasma Desktop"));
    QCOMPARE(m_serverTextInputV3->surroundingTextCursorPosition(), 0);
    QCOMPARE(m_serverTextInputV3->surroundingTextSelectionAnchor(), 3);

    // disabling we should not get the event
    m_clientTextInputV3->disable();
    QCOMPARE(textInputEnabledSpy.count(), 1);

    // after we do commit we should get event
    m_clientTextInputV3->commit();
    QVERIFY(textInputEnabledSpy.wait());
    QCOMPARE(textInputEnabledSpy.count(), 2);
    m_totalCommits++;

    // Lets try leaving the surface and make sure event propogage
    m_seat->setFocusedTextInputSurface(nullptr);
    QVERIFY(surfaceLeaveSpy.wait());
    QCOMPARE(surfaceLeaveSpy.count(), 1);
}

void TestTextInputV3Interface::testEvents()
{
    // create a surface
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreatedSpy.isValid());
    QScopedPointer<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    m_serverTextInputV3 = m_seat->textInputV3();
    QVERIFY(m_serverTextInputV3);

    QSignalSpy focusedSurfaceChangedSpy(m_seat, &SeatInterface::focusedTextInputSurfaceChanged);
    QSignalSpy textInputEnabledSpy(m_serverTextInputV3, &TextInputV3Interface::enabledChanged);

    // Enter the textinput
    QVERIFY(focusedSurfaceChangedSpy.isValid());
    QCOMPARE(focusedSurfaceChangedSpy.count(), 0);

    // Make sure that entering surface does not trigger the text input
    m_seat->setFocusedTextInputSurface(serverSurface);
    // FIXME: somehow this triggers BEFORE setFocusedTextInputSurface returns :(
    //  QVERIFY(focusedSurfaceChangedSpy.wait());
    QCOMPARE(focusedSurfaceChangedSpy.count(), 1);

    // Now enable the textInput
    m_clientTextInputV3->enable();
    m_clientTextInputV3->commit();
    m_totalCommits++;
    QVERIFY(textInputEnabledSpy.wait());

    QSignalSpy preEditSpy(m_clientTextInputV3, &TextInputV3::preedit_string);
    QSignalSpy commitStringSpy(m_clientTextInputV3, &TextInputV3::commit_string);
    QSignalSpy deleteSurroundingSpy(m_clientTextInputV3, &TextInputV3::delete_surrounding_text);
    QSignalSpy doneSpy(m_clientTextInputV3, &TextInputV3::done);

    m_serverTextInputV3->sendPreEditString("Hello KDE community!", 1, 2);
    m_serverTextInputV3->deleteSurroundingText(6, 10);
    m_serverTextInputV3->commitString("Plasma");
    m_serverTextInputV3->done();

    QVERIFY(doneSpy.wait());
    QCOMPARE(doneSpy.count(), 1);
    QCOMPARE(preEditSpy.count(), 1);
    QCOMPARE(commitStringSpy.count(), 1);
    QCOMPARE(deleteSurroundingSpy.count(), 1);

    QCOMPARE(preEditSpy.last().at(0).value<QString>(), "Hello KDE community!");
    QCOMPARE(preEditSpy.last().at(1).value<quint32>(), 1);
    QCOMPARE(preEditSpy.last().at(2).value<quint32>(), 2);
    QCOMPARE(commitStringSpy.last().at(0).value<QString>(), "Plasma");
    QCOMPARE(deleteSurroundingSpy.last().at(0).value<quint32>(), 6);
    QCOMPARE(deleteSurroundingSpy.last().at(1).value<quint32>(), 10);

    // zwp_text_input_v3.done event have serial of total commits
    QCOMPARE(doneSpy.last().at(0).value<quint32>(), m_totalCommits);

    // Now disable the textInput
    m_clientTextInputV3->disable();
    m_clientTextInputV3->commit();
    m_totalCommits++;
    QVERIFY(textInputEnabledSpy.wait());
}

void TestTextInputV3Interface::testContentPurpose_data()
{
    QTest::addColumn<QtWayland::zwp_text_input_v3::content_purpose>("clientPurpose");
    QTest::addColumn<KWaylandServer::TextInputContentPurpose>("serverPurpose");

    QTest::newRow("Alpha") << QtWayland::zwp_text_input_v3::content_purpose_alpha << TextInputContentPurpose::Alpha;
    QTest::newRow("Digits") << QtWayland::zwp_text_input_v3::content_purpose_digits << TextInputContentPurpose::Digits;
    QTest::newRow("Number") << QtWayland::zwp_text_input_v3::content_purpose_number << TextInputContentPurpose::Number;
    QTest::newRow("Phone") << QtWayland::zwp_text_input_v3::content_purpose_phone << TextInputContentPurpose::Phone;
    QTest::newRow("Url") << QtWayland::zwp_text_input_v3::content_purpose_url << TextInputContentPurpose::Url;
    QTest::newRow("Email") << QtWayland::zwp_text_input_v3::content_purpose_email << TextInputContentPurpose::Email;
    QTest::newRow("Name") << QtWayland::zwp_text_input_v3::content_purpose_name << TextInputContentPurpose::Name;
    QTest::newRow("Password") << QtWayland::zwp_text_input_v3::content_purpose_password << TextInputContentPurpose::Password;
    QTest::newRow("Pin") << QtWayland::zwp_text_input_v3::content_purpose_pin << TextInputContentPurpose::Pin;
    QTest::newRow("Date") << QtWayland::zwp_text_input_v3::content_purpose_date << TextInputContentPurpose::Date;
    QTest::newRow("Time") << QtWayland::zwp_text_input_v3::content_purpose_time << TextInputContentPurpose::Time;
    QTest::newRow("DateTime") << QtWayland::zwp_text_input_v3::content_purpose_datetime << TextInputContentPurpose::DateTime;
    QTest::newRow("Terminal") << QtWayland::zwp_text_input_v3::content_purpose_terminal << TextInputContentPurpose::Terminal;
}

void TestTextInputV3Interface::testContentPurpose()
{
    // create a surface
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreatedSpy.isValid());
    QScopedPointer<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    m_serverTextInputV3 = m_seat->textInputV3();
    QVERIFY(m_serverTextInputV3);

    QSignalSpy focusedSurfaceChangedSpy(m_seat, &SeatInterface::focusedTextInputSurfaceChanged);
    QSignalSpy textInputEnabledSpy(m_serverTextInputV3, &TextInputV3Interface::enabledChanged);

    // Enter the textinput
    QVERIFY(focusedSurfaceChangedSpy.isValid());
    QCOMPARE(focusedSurfaceChangedSpy.count(), 0);

    // Make sure that entering surface does not trigger the text input
    m_seat->setFocusedTextInputSurface(serverSurface);
    // FIXME: somehow this triggers BEFORE setFocusedTextInputSurface returns :(
    //  QVERIFY(focusedSurfaceChangedSpy.wait());
    QCOMPARE(focusedSurfaceChangedSpy.count(), 1);

    // Now enable the textInput
    m_clientTextInputV3->enable();
    m_clientTextInputV3->commit();
    QVERIFY(textInputEnabledSpy.wait());
    m_totalCommits++;

    // Default should be normal content purpose
    QCOMPARE(m_serverTextInputV3->contentPurpose(), TextInputContentPurpose::Normal);

    QSignalSpy contentTypeChangedSpy(m_serverTextInputV3, &TextInputV3Interface::contentTypeChanged);
    QVERIFY(contentTypeChangedSpy.isValid());

    QFETCH(QtWayland::zwp_text_input_v3::content_purpose, clientPurpose);
    m_clientTextInputV3->enable();
    m_clientTextInputV3->set_content_type(QtWayland::zwp_text_input_v3::content_hint_none, clientPurpose);
    m_clientTextInputV3->commit();
    QVERIFY(contentTypeChangedSpy.wait());
    QTEST(m_serverTextInputV3->contentPurpose(), "serverPurpose");
    m_totalCommits++;

    // Setting same thing should not trigger update
    m_clientTextInputV3->enable();
    m_clientTextInputV3->set_content_type(QtWayland::zwp_text_input_v3::content_hint_none, clientPurpose);
    m_clientTextInputV3->commit();
    QVERIFY(!contentTypeChangedSpy.wait(100));
    m_totalCommits++;

    // unset to normal
    m_clientTextInputV3->enable();
    m_clientTextInputV3->set_content_type(QtWayland::zwp_text_input_v3::content_hint_none, QtWayland::zwp_text_input_v3::content_purpose_normal);
    m_clientTextInputV3->commit();
    QVERIFY(contentTypeChangedSpy.wait());
    m_totalCommits++;
    QCOMPARE(m_serverTextInputV3->contentPurpose(), TextInputContentPurpose::Normal);

    // Now disable the textInput
    m_clientTextInputV3->disable();
    m_clientTextInputV3->commit();
    m_totalCommits++;
    QVERIFY(textInputEnabledSpy.wait());
}

void TestTextInputV3Interface::testContentHints_data()
{
    QTest::addColumn<quint32>("clientHint");
    QTest::addColumn<KWaylandServer::TextInputContentHints>("serverHints");

    QTest::addRow("Spellcheck") << quint32(QtWayland::zwp_text_input_v3::content_hint_spellcheck) << TextInputContentHints(TextInputContentHint::AutoCorrection);
    QTest::addRow("Completion") << quint32(QtWayland::zwp_text_input_v3::content_hint_completion) << TextInputContentHints(TextInputContentHint::AutoCompletion);
    QTest::addRow("AutoCapital") << quint32(QtWayland::zwp_text_input_v3::content_hint_auto_capitalization) << TextInputContentHints(TextInputContentHint::AutoCapitalization);
    QTest::addRow("Lowercase") << quint32(QtWayland::zwp_text_input_v3::content_hint_lowercase) << TextInputContentHints(TextInputContentHint::LowerCase);
    QTest::addRow("Uppercase") << quint32(QtWayland::zwp_text_input_v3::content_hint_uppercase) << TextInputContentHints(TextInputContentHint::UpperCase);
    QTest::addRow("Titlecase") << quint32(QtWayland::zwp_text_input_v3::content_hint_titlecase) << TextInputContentHints(TextInputContentHint::TitleCase);
    QTest::addRow("HiddenText") << quint32(QtWayland::zwp_text_input_v3::content_hint_hidden_text) << TextInputContentHints(TextInputContentHint::HiddenText);
    QTest::addRow("SensitiveData") << quint32(QtWayland::zwp_text_input_v3::content_hint_sensitive_data) << TextInputContentHints(TextInputContentHint::SensitiveData);
    QTest::addRow("Latin") << quint32(QtWayland::zwp_text_input_v3::content_hint_latin) << TextInputContentHints(TextInputContentHint::Latin);
    QTest::addRow("Multiline") << quint32(QtWayland::zwp_text_input_v3::content_hint_multiline) << TextInputContentHints(TextInputContentHint::MultiLine);
    QTest::addRow("Auto") << quint32(QtWayland::zwp_text_input_v3::content_hint_completion | QtWayland::zwp_text_input_v3::content_hint_spellcheck | QtWayland::zwp_text_input_v3::content_hint_auto_capitalization)
                          << TextInputContentHints(TextInputContentHint::AutoCompletion | TextInputContentHint::AutoCorrection | TextInputContentHint::AutoCapitalization);
}

void TestTextInputV3Interface::testContentHints()
{
    // create a surface
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreatedSpy.isValid());
    QScopedPointer<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    m_serverTextInputV3 = m_seat->textInputV3();
    QVERIFY(m_serverTextInputV3);

    QSignalSpy focusedSurfaceChangedSpy(m_seat, &SeatInterface::focusedTextInputSurfaceChanged);
    QSignalSpy textInputEnabledSpy(m_serverTextInputV3, &TextInputV3Interface::enabledChanged);

    // Enter the textinput
    QVERIFY(focusedSurfaceChangedSpy.isValid());
    QCOMPARE(focusedSurfaceChangedSpy.count(), 0);

    // Make sure that entering surface does not trigger the text input
    m_seat->setFocusedTextInputSurface(serverSurface);
    // FIXME: somehow this triggers BEFORE setFocusedTextInputSurface returns :(
    //  QVERIFY(focusedSurfaceChangedSpy.wait());
    QCOMPARE(focusedSurfaceChangedSpy.count(), 1);

    // Now enable the textInput
    m_clientTextInputV3->enable();
    m_clientTextInputV3->commit();
    QVERIFY(textInputEnabledSpy.wait());
    m_totalCommits++;

    QCOMPARE(m_serverTextInputV3->contentHints(), TextInputContentHint::None);

    // Now disable the textInput
    m_clientTextInputV3->disable();
    m_clientTextInputV3->commit();
    QVERIFY(textInputEnabledSpy.wait());
    m_totalCommits++;

    QSignalSpy contentTypeChangedSpy(m_serverTextInputV3, &TextInputV3Interface::contentTypeChanged);
    QVERIFY(contentTypeChangedSpy.isValid());

    QFETCH(quint32, clientHint);
    m_clientTextInputV3->enable();
    m_clientTextInputV3->set_content_type(clientHint, QtWayland::zwp_text_input_v3::content_purpose_normal);
    m_clientTextInputV3->commit();
    QVERIFY(contentTypeChangedSpy.wait());
    QTEST(m_serverTextInputV3->contentHints(), "serverHints");
    m_totalCommits++;

    // Setting same thing should not trigger update
    m_clientTextInputV3->enable();
    m_clientTextInputV3->set_content_type(clientHint, QtWayland::zwp_text_input_v3::content_purpose_normal);
    m_clientTextInputV3->commit();
    QVERIFY(!contentTypeChangedSpy.wait(100));
    m_totalCommits++;

    // unset to normal
    m_clientTextInputV3->enable();
    m_clientTextInputV3->set_content_type(QtWayland::zwp_text_input_v3::content_hint_none, QtWayland::zwp_text_input_v3::content_purpose_normal);
    m_clientTextInputV3->commit();
    QVERIFY(contentTypeChangedSpy.wait());
    m_totalCommits++;

    // Now disable the textInput
    m_clientTextInputV3->disable();
    m_clientTextInputV3->commit();
    QVERIFY(textInputEnabledSpy.wait());
    m_totalCommits++;
}

void TestTextInputV3Interface::testMultipleTextinputs()
{
    // create two more text inputs
    TextInputV3 *ti1 = new TextInputV3();
    ti1->init(m_clientTextInputManagerV3->get_text_input(*m_clientSeat));
    QVERIFY(ti1);

    TextInputV3 *ti2 = new TextInputV3();
    ti2->init(m_clientTextInputManagerV3->get_text_input(*m_clientSeat));
    QVERIFY(ti2);

    // create a surface
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreatedSpy.isValid());
    QScopedPointer<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    QSignalSpy focusedSurfaceChangedSpy(m_seat, &SeatInterface::focusedTextInputSurfaceChanged);
    // Make sure that entering surface does not trigger the text input
    m_seat->setFocusedTextInputSurface(serverSurface);
    QCOMPARE(focusedSurfaceChangedSpy.count(), 1);

    m_serverTextInputV3 = m_seat->textInputV3();
    QVERIFY(m_serverTextInputV3);

    QSignalSpy committedSpy(m_serverTextInputV3, &TextInputV3Interface::stateCommitted);
    // Enable ti1
    ti1->enable();
    ti1->commit();
    QVERIFY(committedSpy.wait());
    QCOMPARE(committedSpy.last().at(0).value<quint32>(), 1);

    // Send another three commits on ti1
    ti1->enable();
    ti1->set_surrounding_text("hello", 0, 1);
    ti1->commit();
    QVERIFY(committedSpy.wait());
    QCOMPARE(committedSpy.last().at(0).value<quint32>(), 2);

    ti1->enable();
    ti1->set_content_type(QtWayland::zwp_text_input_v3::content_hint_none, QtWayland::zwp_text_input_v3::content_purpose_normal);
    ti1->commit();
    QVERIFY(committedSpy.wait());
    QCOMPARE(committedSpy.last().at(0).value<quint32>(), 3);

    // at this point total commit count to ti1 is 3
    QSignalSpy doneSpy1(ti1, &TextInputV3::done);
    QSignalSpy doneSpy2(ti2, &TextInputV3::done);

    m_serverTextInputV3->commitString("Hello");
    m_serverTextInputV3->done();
    QVERIFY(doneSpy1.wait());

    // zwp_text_input_v3.done event have serial of total commits
    QCOMPARE(doneSpy1.last().at(0).value<quint32>(), 3);

    // now ti1 is at 4 commit, while ti2 is still 0
    ti1->disable();
    ti1->commit();
    QVERIFY(committedSpy.wait());
    QCOMPARE(committedSpy.last().at(0).value<quint32>(), 4);

    // first commit to ti2
    ti2->enable();
    ti2->commit();
    QVERIFY(committedSpy.wait());
    QCOMPARE(committedSpy.last().at(0).value<quint32>(), 1);

    // send commit string
    m_serverTextInputV3->commitString("Hello world");
    m_serverTextInputV3->done();
    QVERIFY(doneSpy2.wait());

    // ti2 is at one commit
    QCOMPARE(doneSpy2.last().at(0).value<quint32>(), 1);
    ti2->disable();
    ti2->commit();
    QVERIFY(committedSpy.wait());
    QCOMPARE(committedSpy.last().at(0).value<quint32>(), 2);

    // now re-enable the ti1 and verify sending commits to t2 hasn't affected it's serial
    // Enable ti1 : 5 commits now
    ti1->enable();
    ti1->commit();
    QVERIFY(committedSpy.wait());
    QCOMPARE(committedSpy.last().at(0).value<quint32>(), 5);

    // send done signal
    m_serverTextInputV3->commitString("Hello");
    m_serverTextInputV3->done();
    QVERIFY(doneSpy1.wait());
    QCOMPARE(doneSpy1.last().at(0).value<quint32>(), 5);

    // cleanup
    if (ti1) {
        delete ti1;
        ti1 = nullptr;
    }
    if (ti2) {
        delete ti2;
        ti2 = nullptr;
    }
}


QTEST_GUILESS_MAIN(TestTextInputV3Interface)

#include "test_textinputv3_interface.moc"
