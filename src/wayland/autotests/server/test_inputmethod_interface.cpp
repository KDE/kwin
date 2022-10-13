/*
    SPDX-FileCopyrightText: 2020 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2020 Bhushan Shah <bshah@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QHash>
#include <QThread>
#include <QtTest>

#include "../../tests/fakeoutput.h"

// WaylandServer
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/inputmethod_v1_interface.h"
#include "wayland/seat_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/keyboard.h"
#include "KWayland/Client/output.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/surface.h"

#include "qwayland-input-method-unstable-v1.h"
#include "qwayland-server-text-input-unstable-v1.h"

#include <linux/input-event-codes.h>

using namespace KWaylandServer;

class InputPanelSurface : public QObject, public QtWayland::zwp_input_panel_surface_v1
{
    Q_OBJECT
public:
    InputPanelSurface(::zwp_input_panel_surface_v1 *t)
        : QtWayland::zwp_input_panel_surface_v1(t)
    {
    }
};

class InputPanel : public QtWayland::zwp_input_panel_v1
{
public:
    InputPanel(struct ::wl_registry *registry, int id, int version)
        : QtWayland::zwp_input_panel_v1(registry, id, version)
    {
    }

    InputPanelSurface *panelForSurface(KWayland::Client::Surface *surface)
    {
        auto panelSurface = new InputPanelSurface(get_input_panel_surface(*surface));
        QObject::connect(surface, &QObject::destroyed, panelSurface, &QObject::deleteLater);
        return panelSurface;
    }
};

class InputMethodV1Context : public QObject, public QtWayland::zwp_input_method_context_v1
{
    Q_OBJECT
public:
    quint32 contentPurpose()
    {
        return imPurpose;
    }
    quint32 contentHints()
    {
        return imHint;
    }
Q_SIGNALS:
    void content_type_changed();
    void invoke_action(quint32 button, quint32 index);
    void preferred_language(QString lang);
    void surrounding_text(QString lang, quint32 cursor, quint32 anchor);
    void reset();

protected:
    void zwp_input_method_context_v1_content_type(uint32_t hint, uint32_t purpose) override
    {
        imHint = hint;
        imPurpose = purpose;
        Q_EMIT content_type_changed();
    }
    void zwp_input_method_context_v1_invoke_action(uint32_t button, uint32_t index) override
    {
        Q_EMIT invoke_action(button, index);
    }
    void zwp_input_method_context_v1_preferred_language(const QString &language) override
    {
        Q_EMIT preferred_language(language);
    }
    void zwp_input_method_context_v1_surrounding_text(const QString &text, uint32_t cursor, uint32_t anchor) override
    {
        Q_EMIT surrounding_text(text, cursor, anchor);
    }
    void zwp_input_method_context_v1_reset() override
    {
        Q_EMIT reset();
    }

private:
    quint32 imHint = 0;
    quint32 imPurpose = 0;
};

class InputMethodV1 : public QObject, public QtWayland::zwp_input_method_v1
{
    Q_OBJECT
public:
    InputMethodV1(struct ::wl_registry *registry, int id, int version)
        : QtWayland::zwp_input_method_v1(registry, id, version)
    {
    }
    InputMethodV1Context *context()
    {
        return m_context;
    }

Q_SIGNALS:
    void activated();
    void deactivated();

protected:
    void zwp_input_method_v1_activate(struct ::zwp_input_method_context_v1 *context) override
    {
        m_context = new InputMethodV1Context();
        m_context->init(context);
        Q_EMIT activated();
    };
    void zwp_input_method_v1_deactivate(struct ::zwp_input_method_context_v1 *context) override
    {
        Q_UNUSED(context)
        delete m_context;
        m_context = nullptr;
        Q_EMIT deactivated();
    };

private:
    InputMethodV1Context *m_context;
};

class TestInputMethodInterface : public QObject
{
    Q_OBJECT
public:
    TestInputMethodInterface()
    {
    }
    ~TestInputMethodInterface() override;

private Q_SLOTS:
    void initTestCase();
    void testAdd();
    void testActivate();
    void testContext();
    void testGrabkeyboard();
    void testContentHints_data();
    void testContentHints();
    void testContentPurpose_data();
    void testContentPurpose();
    void testKeyboardGrab();

private:
    std::unique_ptr<KWayland::Client::ConnectionThread> m_connection;
    std::unique_ptr<KWayland::Client::EventQueue> m_queue;
    std::unique_ptr<KWayland::Client::Compositor> m_clientCompositor;
    std::unique_ptr<KWayland::Client::Seat> m_clientSeat;
    std::unique_ptr<KWayland::Client::Output> m_output;
    std::unique_ptr<KWayland::Client::Registry> m_registry;
    std::vector<std::unique_ptr<KWayland::Client::Surface>> m_clientSurfaces;

    std::unique_ptr<InputMethodV1> m_inputMethod;
    std::unique_ptr<InputPanel> m_inputPanel;
    std::unique_ptr<QThread> m_thread;
    std::unique_ptr<KWaylandServer::Display> m_display;
    std::unique_ptr<SeatInterface> m_seat;
    std::unique_ptr<CompositorInterface> m_serverCompositor;
    std::unique_ptr<FakeOutput> m_outputHandle;
    std::unique_ptr<OutputInterface> m_outputInterface;

    std::unique_ptr<KWaylandServer::InputMethodV1Interface> m_inputMethodIface;
    std::unique_ptr<KWaylandServer::InputPanelV1Interface> m_inputPanelIface;

    QVector<SurfaceInterface *> m_surfaces;
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-inputmethod-test-0");

void TestInputMethodInterface::initTestCase()
{
    m_display = std::make_unique<KWaylandServer::Display>();
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_seat = std::make_unique<SeatInterface>(m_display.get());
    m_serverCompositor = std::make_unique<CompositorInterface>(m_display.get());
    m_inputMethodIface = std::make_unique<InputMethodV1Interface>(m_display.get());
    m_inputPanelIface = std::make_unique<InputPanelV1Interface>(m_display.get());

    m_outputHandle = std::make_unique<FakeOutput>();
    m_outputInterface = std::make_unique<OutputInterface>(m_display.get(), m_outputHandle.get());

    connect(m_serverCompositor.get(), &CompositorInterface::surfaceCreated, this, [this](SurfaceInterface *surface) {
        m_surfaces += surface;
    });

    // setup connection
    m_connection = std::make_unique<KWayland::Client::ConnectionThread>();
    QSignalSpy connectedSpy(m_connection.get(), &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = std::make_unique<QThread>();
    m_connection->moveToThread(m_thread.get());
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());
    QVERIFY(!m_connection->connections().isEmpty());

    m_queue = std::make_unique<KWayland::Client::EventQueue>();
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection.get());
    QVERIFY(m_queue->isValid());

    m_registry = std::make_unique<KWayland::Client::Registry>();
    QSignalSpy interfacesSpy(m_registry.get(), &KWayland::Client::Registry::interfacesAnnounced);
    connect(m_registry.get(), &KWayland::Client::Registry::outputAnnounced, this, [this](quint32 name, quint32 version) {
        m_output = std::make_unique<KWayland::Client::Output>();
        m_output->setup(m_registry->bindOutput(name, version));
    });
    connect(m_registry.get(), &KWayland::Client::Registry::interfaceAnnounced, this, [this](const QByteArray &interface, quint32 name, quint32 version) {
        if (interface == "zwp_input_panel_v1") {
            m_inputPanel = std::make_unique<InputPanel>(m_registry->registry(), name, version);
        } else if (interface == "zwp_input_method_v1") {
            m_inputMethod = std::make_unique<InputMethodV1>(m_registry->registry(), name, version);
        }
    });
    connect(m_registry.get(), &KWayland::Client::Registry::seatAnnounced, this, [this](quint32 name, quint32 version) {
        m_clientSeat.reset(m_registry->createSeat(name, version));
    });
    m_registry->setEventQueue(m_queue.get());
    QSignalSpy compositorSpy(m_registry.get(), &KWayland::Client::Registry::compositorAnnounced);
    m_registry->create(m_connection->display());
    QVERIFY(m_registry->isValid());
    m_registry->setup();
    wl_display_flush(m_connection->display());

    QVERIFY(compositorSpy.wait());
    m_clientCompositor.reset(m_registry->createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>()));
    QVERIFY(m_clientCompositor->isValid());

    QVERIFY(interfacesSpy.count() || interfacesSpy.wait());

    QSignalSpy surfaceSpy(m_serverCompositor.get(), &CompositorInterface::surfaceCreated);
    for (int i = 0; i < 3; ++i) {
        m_clientSurfaces.push_back(std::unique_ptr<KWayland::Client::Surface>(m_clientCompositor->createSurface()));
    }
    QVERIFY(surfaceSpy.count() < 3 && surfaceSpy.wait(200));
    QVERIFY(m_surfaces.size() == 3);
    QVERIFY(m_inputPanel);
    QVERIFY(m_output);
}

TestInputMethodInterface::~TestInputMethodInterface()
{
    m_queue.reset();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        m_thread.reset();
    }
    m_clientSurfaces.clear();
    m_output.reset();
    m_inputPanel.reset();
    m_inputMethod.reset();
    m_inputMethodIface.reset();
    m_inputPanelIface.reset();
    m_clientSeat.reset();
    m_clientCompositor.reset();
    m_registry.reset();
    m_connection.reset();
    m_display.reset();
}

void TestInputMethodInterface::testAdd()
{
    QSignalSpy panelSpy(m_inputPanelIface.get(), &InputPanelV1Interface::inputPanelSurfaceAdded);
    QPointer<InputPanelSurfaceV1Interface> panelSurfaceIface;
    connect(m_inputPanelIface.get(), &InputPanelV1Interface::inputPanelSurfaceAdded, this, [&panelSurfaceIface](InputPanelSurfaceV1Interface *surface) {
        panelSurfaceIface = surface;
    });

    auto surface = std::unique_ptr<KWayland::Client::Surface>(m_clientCompositor->createSurface());
    auto panelSurface = m_inputPanel->panelForSurface(surface.get());

    QVERIFY(panelSpy.wait() || panelSurfaceIface);
    Q_ASSERT(panelSurfaceIface);
    Q_ASSERT(panelSurfaceIface->surface() == m_surfaces.constLast());

    QSignalSpy panelTopLevelSpy(panelSurfaceIface, &InputPanelSurfaceV1Interface::topLevel);
    panelSurface->set_toplevel(*m_output, InputPanelSurface::position_center_bottom);
    QVERIFY(panelTopLevelSpy.wait());
}

void TestInputMethodInterface::testActivate()
{
    QVERIFY(m_inputMethodIface);
    QSignalSpy inputMethodActivateSpy(m_inputMethod.get(), &InputMethodV1::activated);
    QSignalSpy inputMethodDeactivateSpy(m_inputMethod.get(), &InputMethodV1::deactivated);

    // before sending activate the context should be null
    QVERIFY(!m_inputMethodIface->context());

    // send activate now
    m_inputMethodIface->sendActivate();
    QVERIFY(inputMethodActivateSpy.wait());
    QCOMPARE(inputMethodActivateSpy.count(), 1);
    QVERIFY(m_inputMethodIface->context());

    // send deactivate and verify server interface resets context
    m_inputMethodIface->sendDeactivate();
    QVERIFY(inputMethodDeactivateSpy.wait());
    QCOMPARE(inputMethodActivateSpy.count(), 1);
    QVERIFY(!m_inputMethodIface->context());
}

void TestInputMethodInterface::testContext()
{
    QVERIFY(m_inputMethodIface);
    QSignalSpy inputMethodActivateSpy(m_inputMethod.get(), &InputMethodV1::activated);
    QSignalSpy inputMethodDeactivateSpy(m_inputMethod.get(), &InputMethodV1::deactivated);

    // before sending activate the context should be null
    QVERIFY(!m_inputMethodIface->context());

    // send activate now
    m_inputMethodIface->sendActivate();
    QVERIFY(inputMethodActivateSpy.wait());
    QCOMPARE(inputMethodActivateSpy.count(), 1);

    KWaylandServer::InputMethodContextV1Interface *serverContext = m_inputMethodIface->context();
    QVERIFY(serverContext);

    InputMethodV1Context *imContext = m_inputMethod->context();
    QVERIFY(imContext);

    quint32 serial = 1;

    // commit some text
    QSignalSpy commitStringSpy(serverContext, &KWaylandServer::InputMethodContextV1Interface::commitString);
    imContext->commit_string(serial, "hello");
    QVERIFY(commitStringSpy.wait());
    QCOMPARE(commitStringSpy.count(), serial);
    QCOMPARE(commitStringSpy.last().at(0).value<quint32>(), serial);
    QCOMPARE(commitStringSpy.last().at(1).value<QString>(), "hello");
    serial++;

    // preedit styling event
    QSignalSpy preeditStylingSpy(serverContext, &KWaylandServer::InputMethodContextV1Interface::preeditStyling);
    // protocol does not document 3rd argument mean in much details (styling)
    imContext->preedit_styling(0, 5, 1);
    QVERIFY(preeditStylingSpy.wait());
    QCOMPARE(preeditStylingSpy.count(), 1);
    QCOMPARE(preeditStylingSpy.last().at(0).value<quint32>(), 0);
    QCOMPARE(preeditStylingSpy.last().at(1).value<quint32>(), 5);
    QCOMPARE(preeditStylingSpy.last().at(2).value<quint32>(), 1);

    // preedit cursor event
    QSignalSpy preeditCursorSpy(serverContext, &KWaylandServer::InputMethodContextV1Interface::preeditCursor);
    imContext->preedit_cursor(3);
    QVERIFY(preeditCursorSpy.wait());
    QCOMPARE(preeditCursorSpy.count(), 1);
    QCOMPARE(preeditCursorSpy.last().at(0).value<quint32>(), 3);

    // commit preedit_string
    QSignalSpy preeditStringSpy(serverContext, &KWaylandServer::InputMethodContextV1Interface::preeditString);
    imContext->preedit_string(serial, "hello", "kde");
    QVERIFY(preeditStringSpy.wait());
    QCOMPARE(preeditStringSpy.count(), 1);
    QCOMPARE(preeditStringSpy.last().at(0).value<quint32>(), serial);
    QCOMPARE(preeditStringSpy.last().at(1).value<QString>(), "hello");
    QCOMPARE(preeditStringSpy.last().at(2).value<QString>(), "kde");
    serial++;

    // delete surrounding text
    QSignalSpy deleteSurroundingSpy(serverContext, &KWaylandServer::InputMethodContextV1Interface::deleteSurroundingText);
    imContext->delete_surrounding_text(0, 5);
    QVERIFY(deleteSurroundingSpy.wait());
    QCOMPARE(deleteSurroundingSpy.count(), 1);
    QCOMPARE(deleteSurroundingSpy.last().at(0).value<quint32>(), 0);
    QCOMPARE(deleteSurroundingSpy.last().at(1).value<quint32>(), 5);

    // set cursor position
    QSignalSpy cursorPositionSpy(serverContext, &KWaylandServer::InputMethodContextV1Interface::cursorPosition);
    imContext->cursor_position(2, 4);
    QVERIFY(cursorPositionSpy.wait());
    QCOMPARE(cursorPositionSpy.count(), 1);
    QCOMPARE(cursorPositionSpy.last().at(0).value<quint32>(), 2);
    QCOMPARE(cursorPositionSpy.last().at(1).value<quint32>(), 4);

    // invoke action
    QSignalSpy invokeActionSpy(imContext, &InputMethodV1Context::invoke_action);
    serverContext->sendInvokeAction(3, 5);
    QVERIFY(invokeActionSpy.wait());
    QCOMPARE(invokeActionSpy.count(), 1);
    QCOMPARE(invokeActionSpy.last().at(0).value<quint32>(), 3);
    QCOMPARE(invokeActionSpy.last().at(1).value<quint32>(), 5);

    // preferred language
    QSignalSpy preferredLanguageSpy(imContext, &InputMethodV1Context::preferred_language);
    serverContext->sendPreferredLanguage("gu_IN");
    QVERIFY(preferredLanguageSpy.wait());
    QCOMPARE(preferredLanguageSpy.count(), 1);
    QCOMPARE(preferredLanguageSpy.last().at(0).value<QString>(), "gu_IN");

    // surrounding text
    QSignalSpy surroundingTextSpy(imContext, &InputMethodV1Context::surrounding_text);
    serverContext->sendSurroundingText("Hello Plasma!", 2, 4);
    QVERIFY(surroundingTextSpy.wait());
    QCOMPARE(surroundingTextSpy.count(), 1);
    QCOMPARE(surroundingTextSpy.last().at(0).value<QString>(), "Hello Plasma!");
    QCOMPARE(surroundingTextSpy.last().at(1).value<quint32>(), 2);
    QCOMPARE(surroundingTextSpy.last().at(2).value<quint32>(), 4);

    // reset
    QSignalSpy resetSpy(imContext, &InputMethodV1Context::reset);
    serverContext->sendReset();
    QVERIFY(resetSpy.wait());
    QCOMPARE(resetSpy.count(), 1);

    // send deactivate and verify server interface resets context
    m_inputMethodIface->sendDeactivate();
    QVERIFY(inputMethodDeactivateSpy.wait());
    QCOMPARE(inputMethodActivateSpy.count(), 1);
    QVERIFY(!m_inputMethodIface->context());
    QVERIFY(!m_inputMethod->context());
}

void TestInputMethodInterface::testGrabkeyboard()
{
    QVERIFY(m_inputMethodIface);
    QSignalSpy inputMethodActivateSpy(m_inputMethod.get(), &InputMethodV1::activated);
    QSignalSpy inputMethodDeactivateSpy(m_inputMethod.get(), &InputMethodV1::deactivated);

    // before sending activate the context should be null
    QVERIFY(!m_inputMethodIface->context());

    // send activate now
    m_inputMethodIface->sendActivate();
    QVERIFY(inputMethodActivateSpy.wait());
    QCOMPARE(inputMethodActivateSpy.count(), 1);

    KWaylandServer::InputMethodContextV1Interface *serverContext = m_inputMethodIface->context();
    QVERIFY(serverContext);

    InputMethodV1Context *imContext = m_inputMethod->context();
    QVERIFY(imContext);

    QSignalSpy keyEventSpy(serverContext, &KWaylandServer::InputMethodContextV1Interface::key);
    imContext->key(0, 123, 56, 1);
    QEXPECT_FAIL("", "We should be not get key event if keyboard is not grabbed", Continue);
    QVERIFY(!keyEventSpy.wait(200));

    QSignalSpy modifierEventSpy(serverContext, &KWaylandServer::InputMethodContextV1Interface::modifiers);
    imContext->modifiers(1234, 0, 0, 0, 0);
    QEXPECT_FAIL("", "We should be not get modifiers event if keyboard is not grabbed", Continue);
    QVERIFY(!modifierEventSpy.wait(200));

    // grab the keyboard
    wl_keyboard *keyboard = imContext->grab_keyboard();
    QVERIFY(keyboard);

    // TODO: add more tests about keyboard grab here

    // send deactivate and verify server interface resets context
    m_inputMethodIface->sendDeactivate();
    QVERIFY(inputMethodDeactivateSpy.wait());
    QCOMPARE(inputMethodActivateSpy.count(), 1);
    QVERIFY(!m_inputMethodIface->context());
    QVERIFY(!m_inputMethod->context());
}

void TestInputMethodInterface::testContentHints_data()
{
    QTest::addColumn<KWaylandServer::TextInputContentHints>("serverHints");
    QTest::addColumn<quint32>("imHint");
    QTest::addRow("Spellcheck") << TextInputContentHints(TextInputContentHint::AutoCorrection)
                                << quint32(QtWaylandServer::zwp_text_input_v1::content_hint_auto_correction);
    QTest::addRow("AutoCapital") << TextInputContentHints(TextInputContentHint::AutoCapitalization)
                                 << quint32(QtWaylandServer::zwp_text_input_v1::content_hint_auto_capitalization);
    QTest::addRow("Lowercase") << TextInputContentHints(TextInputContentHint::LowerCase) << quint32(QtWaylandServer::zwp_text_input_v1::content_hint_lowercase);
    QTest::addRow("Uppercase") << TextInputContentHints(TextInputContentHint::UpperCase) << quint32(QtWaylandServer::zwp_text_input_v1::content_hint_uppercase);
    QTest::addRow("Titlecase") << TextInputContentHints(TextInputContentHint::TitleCase) << quint32(QtWaylandServer::zwp_text_input_v1::content_hint_titlecase);
    QTest::addRow("HiddenText") << TextInputContentHints(TextInputContentHint::HiddenText)
                                << quint32(QtWaylandServer::zwp_text_input_v1::content_hint_hidden_text);
    QTest::addRow("SensitiveData") << TextInputContentHints(TextInputContentHint::SensitiveData)
                                   << quint32(QtWaylandServer::zwp_text_input_v1::content_hint_sensitive_data);
    QTest::addRow("Latin") << TextInputContentHints(TextInputContentHint::Latin) << quint32(QtWaylandServer::zwp_text_input_v1::content_hint_latin);
    QTest::addRow("Multiline") << TextInputContentHints(TextInputContentHint::MultiLine) << quint32(QtWaylandServer::zwp_text_input_v1::content_hint_multiline);
    QTest::addRow("Auto") << TextInputContentHints(TextInputContentHint::AutoCorrection | TextInputContentHint::AutoCapitalization)
                          << quint32(QtWaylandServer::zwp_text_input_v1::content_hint_auto_correction
                                     | QtWaylandServer::zwp_text_input_v1::content_hint_auto_capitalization);
}

void TestInputMethodInterface::testContentHints()
{
    QVERIFY(m_inputMethodIface);
    QSignalSpy inputMethodActivateSpy(m_inputMethod.get(), &InputMethodV1::activated);
    QSignalSpy inputMethodDeactivateSpy(m_inputMethod.get(), &InputMethodV1::deactivated);

    // before sending activate the context should be null
    QVERIFY(!m_inputMethodIface->context());

    // send activate now
    m_inputMethodIface->sendActivate();
    QVERIFY(inputMethodActivateSpy.wait());
    QCOMPARE(inputMethodActivateSpy.count(), 1);

    KWaylandServer::InputMethodContextV1Interface *serverContext = m_inputMethodIface->context();
    QVERIFY(serverContext);

    InputMethodV1Context *imContext = m_inputMethod->context();
    QVERIFY(imContext);

    QSignalSpy contentTypeChangedSpy(imContext, &InputMethodV1Context::content_type_changed);

    QFETCH(KWaylandServer::TextInputContentHints, serverHints);
    serverContext->sendContentType(serverHints, KWaylandServer::TextInputContentPurpose::Normal);
    QVERIFY(contentTypeChangedSpy.wait());
    QCOMPARE(contentTypeChangedSpy.count(), 1);
    QEXPECT_FAIL("SensitiveData", "SensitiveData content hint need fixing", Continue);
    QTEST(imContext->contentHints(), "imHint");

    // send deactivate and verify server interface resets context
    m_inputMethodIface->sendDeactivate();
    QVERIFY(inputMethodDeactivateSpy.wait());
    QCOMPARE(inputMethodActivateSpy.count(), 1);
    QVERIFY(!m_inputMethodIface->context());
    QVERIFY(!m_inputMethod->context());
}

void TestInputMethodInterface::testContentPurpose_data()
{
    QTest::addColumn<KWaylandServer::TextInputContentPurpose>("serverPurpose");
    QTest::addColumn<quint32>("imPurpose");

    QTest::newRow("Alpha") << TextInputContentPurpose::Alpha << quint32(QtWaylandServer::zwp_text_input_v1::content_purpose_alpha);
    QTest::newRow("Digits") << TextInputContentPurpose::Digits << quint32(QtWaylandServer::zwp_text_input_v1::content_purpose_digits);
    QTest::newRow("Number") << TextInputContentPurpose::Number << quint32(QtWaylandServer::zwp_text_input_v1::content_purpose_number);
    QTest::newRow("Phone") << TextInputContentPurpose::Phone << quint32(QtWaylandServer::zwp_text_input_v1::content_purpose_phone);
    QTest::newRow("Url") << TextInputContentPurpose::Url << quint32(QtWaylandServer::zwp_text_input_v1::content_purpose_url);
    QTest::newRow("Email") << TextInputContentPurpose::Email << quint32(QtWaylandServer::zwp_text_input_v1::content_purpose_email);
    QTest::newRow("Name") << TextInputContentPurpose::Name << quint32(QtWaylandServer::zwp_text_input_v1::content_purpose_name);
    QTest::newRow("Password") << TextInputContentPurpose::Password << quint32(QtWaylandServer::zwp_text_input_v1::content_purpose_password);
    QTest::newRow("Date") << TextInputContentPurpose::Date << quint32(QtWaylandServer::zwp_text_input_v1::content_purpose_date);
    QTest::newRow("Time") << TextInputContentPurpose::Time << quint32(QtWaylandServer::zwp_text_input_v1::content_purpose_time);
    QTest::newRow("DateTime") << TextInputContentPurpose::DateTime << quint32(QtWaylandServer::zwp_text_input_v1::content_purpose_datetime);
    QTest::newRow("Terminal") << TextInputContentPurpose::Terminal << quint32(QtWaylandServer::zwp_text_input_v1::content_purpose_terminal);
    QTest::newRow("Normal") << TextInputContentPurpose::Normal << quint32(QtWaylandServer::zwp_text_input_v1::content_purpose_normal);
    QTest::newRow("Pin") << TextInputContentPurpose::Pin << quint32(QtWaylandServer::zwp_text_input_v1::content_purpose_password);
}

void TestInputMethodInterface::testContentPurpose()
{
    QVERIFY(m_inputMethodIface);
    QSignalSpy inputMethodActivateSpy(m_inputMethod.get(), &InputMethodV1::activated);
    QSignalSpy inputMethodDeactivateSpy(m_inputMethod.get(), &InputMethodV1::deactivated);

    // before sending activate the context should be null
    QVERIFY(!m_inputMethodIface->context());

    // send activate now
    m_inputMethodIface->sendActivate();
    QVERIFY(inputMethodActivateSpy.wait());
    QCOMPARE(inputMethodActivateSpy.count(), 1);

    KWaylandServer::InputMethodContextV1Interface *serverContext = m_inputMethodIface->context();
    QVERIFY(serverContext);

    InputMethodV1Context *imContext = m_inputMethod->context();
    QVERIFY(imContext);

    QSignalSpy contentTypeChangedSpy(imContext, &InputMethodV1Context::content_type_changed);

    QFETCH(KWaylandServer::TextInputContentPurpose, serverPurpose);
    serverContext->sendContentType(KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::None), serverPurpose);
    QVERIFY(contentTypeChangedSpy.wait());
    QCOMPARE(contentTypeChangedSpy.count(), 1);
    QEXPECT_FAIL("Pin", "Pin should return content_purpose_password", Continue);
    QTEST(imContext->contentPurpose(), "imPurpose");

    // send deactivate and verify server interface resets context
    m_inputMethodIface->sendDeactivate();
    QVERIFY(inputMethodDeactivateSpy.wait());
    QCOMPARE(inputMethodActivateSpy.count(), 1);
    QVERIFY(!m_inputMethodIface->context());
    QVERIFY(!m_inputMethod->context());
}

void TestInputMethodInterface::testKeyboardGrab()
{
    QVERIFY(m_inputMethodIface);
    QSignalSpy inputMethodActivateSpy(m_inputMethod.get(), &InputMethodV1::activated);

    m_inputMethodIface->sendActivate();
    QVERIFY(inputMethodActivateSpy.wait());

    QSignalSpy keyboardGrabSpy(m_inputMethodIface->context(), &InputMethodContextV1Interface::keyboardGrabRequested);
    InputMethodV1Context *imContext = m_inputMethod->context();
    QVERIFY(imContext);
    const auto keyboard = std::make_unique<KWayland::Client::Keyboard>();
    keyboard->setup(imContext->grab_keyboard());
    QVERIFY(keyboard->isValid());
    QVERIFY(keyboardGrabSpy.count() || keyboardGrabSpy.wait());

    QSignalSpy keyboardSpy(keyboard.get(), &KWayland::Client::Keyboard::keyChanged);
    m_inputMethodIface->context()->keyboardGrab()->sendKey(0, 0, KEY_F1, KeyboardKeyState::Pressed);
    m_inputMethodIface->context()->keyboardGrab()->sendKey(0, 0, KEY_F1, KeyboardKeyState::Released);
    keyboardSpy.wait();
    QCOMPARE(keyboardSpy.count(), 2);

    m_inputMethodIface->sendDeactivate();
}

QTEST_GUILESS_MAIN(TestInputMethodInterface)
#include "test_inputmethod_interface.moc"
