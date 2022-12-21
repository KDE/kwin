/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// client
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/keyboard.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/seat.h"
#include "KWayland/Client/surface.h"
#include "KWayland/Client/textinput.h"
// server
#include "wayland/compositor_interface.h"
#include "wayland/display.h"
#include "wayland/seat_interface.h"
#include "wayland/textinput.h"
#include "wayland/textinput_v2_interface.h"

using namespace KWaylandServer;
using namespace std::literals;

class TextInputTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testEnterLeave_data();
    void testEnterLeave();
    void testFocusedBeforeCreateTextInput();
    void testShowHidePanel();
    void testCursorRectangle();
    void testPreferredLanguage();
    void testReset();
    void testSurroundingText();
    void testContentHints_data();
    void testContentHints();
    void testContentPurpose_data();
    void testContentPurpose();
    void testTextDirection_data();
    void testTextDirection();
    void testLanguage();
    void testKeyEvent();
    void testPreEdit();
    void testCommit();

private:
    SurfaceInterface *waitForSurface();
    KWayland::Client::TextInput *createTextInput();
    KWaylandServer::Display *m_display = nullptr;
    SeatInterface *m_seatInterface = nullptr;
    CompositorInterface *m_compositorInterface = nullptr;
    TextInputManagerV2Interface *m_textInputManagerV2Interface = nullptr;
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    QThread *m_thread = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
    KWayland::Client::Keyboard *m_keyboard = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::TextInputManager *m_textInputManagerV2 = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-text-input-0");

void TextInputTest::init()
{
    delete m_display;
    m_display = new KWaylandServer::Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
    m_seatInterface = new SeatInterface(m_display, m_display);
    m_seatInterface->setHasKeyboard(true);
    m_seatInterface->setHasTouch(true);
    m_compositorInterface = new CompositorInterface(m_display, m_display);
    m_textInputManagerV2Interface = new TextInputManagerV2Interface(m_display, m_display);

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
    m_queue->setup(m_connection);

    KWayland::Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_seat = registry.createSeat(registry.interface(KWayland::Client::Registry::Interface::Seat).name, registry.interface(KWayland::Client::Registry::Interface::Seat).version, this);
    QVERIFY(m_seat->isValid());
    QSignalSpy hasKeyboardSpy(m_seat, &KWayland::Client::Seat::hasKeyboardChanged);
    QVERIFY(hasKeyboardSpy.wait());
    m_keyboard = m_seat->createKeyboard(this);
    QVERIFY(m_keyboard->isValid());

    m_compositor =
        registry.createCompositor(registry.interface(KWayland::Client::Registry::Interface::Compositor).name, registry.interface(KWayland::Client::Registry::Interface::Compositor).version, this);
    QVERIFY(m_compositor->isValid());

    m_textInputManagerV2 = registry.createTextInputManager(registry.interface(KWayland::Client::Registry::Interface::TextInputManagerUnstableV2).name,
                                                           registry.interface(KWayland::Client::Registry::Interface::TextInputManagerUnstableV2).version,
                                                           this);
    QVERIFY(m_textInputManagerV2->isValid());
}

void TextInputTest::cleanup()
{
#define CLEANUP(variable)   \
    if (variable) {         \
        delete variable;    \
        variable = nullptr; \
    }
    CLEANUP(m_textInputManagerV2)
    CLEANUP(m_keyboard)
    CLEANUP(m_seat)
    CLEANUP(m_compositor)
    CLEANUP(m_queue)
    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }

    CLEANUP(m_display)
#undef CLEANUP

    // these are the children of the display
    m_textInputManagerV2Interface = nullptr;
    m_compositorInterface = nullptr;
    m_seatInterface = nullptr;
}

SurfaceInterface *TextInputTest::waitForSurface()
{
    QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
    if (!surfaceCreatedSpy.isValid()) {
        return nullptr;
    }
    if (!surfaceCreatedSpy.wait(500)) {
        return nullptr;
    }
    if (surfaceCreatedSpy.count() != 1) {
        return nullptr;
    }
    return surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
}

KWayland::Client::TextInput *TextInputTest::createTextInput()
{
    return m_textInputManagerV2->createTextInput(m_seat);
}

void TextInputTest::testEnterLeave_data()
{
    QTest::addColumn<bool>("updatesDirectly");
    QTest::newRow("UnstableV2") << true;
}

void TextInputTest::testEnterLeave()
{
    // this test verifies that enter leave are sent correctly
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());

    std::unique_ptr<KWayland::Client::TextInput> textInput(createTextInput());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QVERIFY(textInput != nullptr);
    QSignalSpy enteredSpy(textInput.get(), &KWayland::Client::TextInput::entered);
    QSignalSpy leftSpy(textInput.get(), &KWayland::Client::TextInput::left);
    QSignalSpy textInputChangedSpy(m_seatInterface, &SeatInterface::focusedTextInputSurfaceChanged);

    // now let's try to enter it
    QVERIFY(!m_seatInterface->focusedTextInputSurface());
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_seatInterface->focusedTextInputSurface(), serverSurface);
    // text input not yet set for the surface
    QFETCH(bool, updatesDirectly);
    QCOMPARE(bool(m_seatInterface->textInputV2()), updatesDirectly);
    QCOMPARE(textInputChangedSpy.isEmpty(), !updatesDirectly);
    textInput->enable(surface.get());
    // this should trigger on server side
    if (!updatesDirectly) {
        QVERIFY(textInputChangedSpy.wait());
    }
    QCOMPARE(textInputChangedSpy.count(), 1);
    auto serverTextInput = m_seatInterface->textInputV2();
    QVERIFY(serverTextInput);
    QSignalSpy enabledChangedSpy(serverTextInput, &TextInputV2Interface::enabledChanged);
    if (updatesDirectly) {
        QVERIFY(enabledChangedSpy.wait());
        enabledChangedSpy.clear();
    }
    QCOMPARE(serverTextInput->surface().data(), serverSurface);
    QVERIFY(serverTextInput->isEnabled());

    // and trigger an enter
    if (enteredSpy.isEmpty()) {
        QVERIFY(enteredSpy.wait());
    }
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(textInput->enteredSurface(), surface.get());

    // now trigger a leave
    m_seatInterface->setFocusedKeyboardSurface(nullptr);
    QCOMPARE(textInputChangedSpy.count(), 2);
    QVERIFY(leftSpy.wait());
    QVERIFY(!textInput->enteredSurface());
    QVERIFY(!serverTextInput->isEnabled());

    // if we enter again we should directly get the text input as it's still activated
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(textInputChangedSpy.count(), 3);
    QVERIFY(m_seatInterface->textInputV2());
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(textInput->enteredSurface(), surface.get());
    QVERIFY(serverTextInput->isEnabled());

    // let's deactivate on client side
    textInput->disable(surface.get());
    QVERIFY(enabledChangedSpy.wait());
    QCOMPARE(enabledChangedSpy.count(), 3);
    QVERIFY(!serverTextInput->isEnabled());
    // does not trigger a leave
    QCOMPARE(textInputChangedSpy.count(), 3);
    // should still be the same text input
    QCOMPARE(m_seatInterface->textInputV2(), serverTextInput);
    // reset
    textInput->enable(surface.get());
    QVERIFY(enabledChangedSpy.wait());

    // delete the client and wait for the server to catch up
    QSignalSpy unboundSpy(serverSurface, &QObject::destroyed);
    surface.reset();
    QVERIFY(unboundSpy.wait());
    QVERIFY(leftSpy.wait());
    QVERIFY(!textInput->enteredSurface());
}

void TextInputTest::testFocusedBeforeCreateTextInput()
{
    // this test verifies that enter leave are sent correctly
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    // now let's try to enter it
    QSignalSpy textInputChangedSpy(m_seatInterface, &SeatInterface::focusedTextInputSurfaceChanged);
    QVERIFY(!m_seatInterface->focusedTextInputSurface());
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_seatInterface->focusedTextInputSurface(), serverSurface);
    QCOMPARE(textInputChangedSpy.count(), 1);

    // This is null because there is no text input object for this client.
    QCOMPARE(m_seatInterface->textInputV2()->surface(), nullptr);

    QVERIFY(serverSurface);
    std::unique_ptr<KWayland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    QSignalSpy enteredSpy(textInput.get(), &KWayland::Client::TextInput::entered);
    QSignalSpy leftSpy(textInput.get(), &KWayland::Client::TextInput::left);

    // and trigger an enter
    if (enteredSpy.isEmpty()) {
        QVERIFY(enteredSpy.wait());
    }
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(textInput->enteredSurface(), surface.get());

    // This is not null anymore because there is a text input object associated with it.
    QCOMPARE(m_seatInterface->textInputV2()->surface(), serverSurface);

    // now trigger a leave
    m_seatInterface->setFocusedKeyboardSurface(nullptr);
    QCOMPARE(textInputChangedSpy.count(), 2);
    QVERIFY(leftSpy.wait());
    QVERIFY(!textInput->enteredSurface());

    QCOMPARE(m_seatInterface->textInputV2()->surface(), nullptr);
    QCOMPARE(m_seatInterface->focusedTextInputSurface(), nullptr);
}

void TextInputTest::testShowHidePanel()
{
    // this test verifies that the requests for show/hide panel work
    // and that status is properly sent to the client
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    std::unique_ptr<KWayland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);

    QSignalSpy showPanelRequestedSpy(ti, &TextInputV2Interface::requestShowInputPanel);
    QSignalSpy hidePanelRequestedSpy(ti, &TextInputV2Interface::requestHideInputPanel);
    QSignalSpy inputPanelStateChangedSpy(textInput.get(), &KWayland::Client::TextInput::inputPanelStateChanged);

    QCOMPARE(textInput->isInputPanelVisible(), false);
    textInput->showInputPanel();
    QVERIFY(showPanelRequestedSpy.wait());
    ti->setInputPanelState(true, QRect(0, 0, 0, 0));
    QVERIFY(inputPanelStateChangedSpy.wait());
    QCOMPARE(textInput->isInputPanelVisible(), true);

    textInput->hideInputPanel();
    QVERIFY(hidePanelRequestedSpy.wait());
    ti->setInputPanelState(false, QRect(0, 0, 0, 0));
    QVERIFY(inputPanelStateChangedSpy.wait());
    QCOMPARE(textInput->isInputPanelVisible(), false);
}

void TextInputTest::testCursorRectangle()
{
    // this test verifies that passing the cursor rectangle from client to server works
    // and that setting visibility state from server to client works
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    std::unique_ptr<KWayland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);
    QCOMPARE(ti->cursorRectangle(), QRect());
    QSignalSpy cursorRectangleChangedSpy(ti, &TextInputV2Interface::cursorRectangleChanged);

    textInput->setCursorRectangle(QRect(10, 20, 30, 40));
    QVERIFY(cursorRectangleChangedSpy.wait());
    QCOMPARE(ti->cursorRectangle(), QRect(10, 20, 30, 40));
}

void TextInputTest::testPreferredLanguage()
{
    // this test verifies that passing the preferred language from client to server works
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    std::unique_ptr<KWayland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);
    QVERIFY(ti->preferredLanguage().isEmpty());

    QSignalSpy preferredLanguageChangedSpy(ti, &TextInputV2Interface::preferredLanguageChanged);
    textInput->setPreferredLanguage(QStringLiteral("foo"));
    QVERIFY(preferredLanguageChangedSpy.wait());
    QCOMPARE(ti->preferredLanguage(), QStringLiteral("foo").toUtf8());
}

void TextInputTest::testReset()
{
    // this test verifies that the reset request is properly passed from client to server
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    std::unique_ptr<KWayland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);

    QSignalSpy stateUpdatedSpy(ti, &TextInputV2Interface::stateUpdated);

    textInput->reset();
    QVERIFY(stateUpdatedSpy.wait());
}

void TextInputTest::testSurroundingText()
{
    // this test verifies that surrounding text is properly passed around
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    std::unique_ptr<KWayland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);
    QVERIFY(ti->surroundingText().isEmpty());
    QCOMPARE(ti->surroundingTextCursorPosition(), 0);
    QCOMPARE(ti->surroundingTextSelectionAnchor(), 0);

    QSignalSpy surroundingTextChangedSpy(ti, &TextInputV2Interface::surroundingTextChanged);

    textInput->setSurroundingText(QStringLiteral("100 €, 100 $"), 5, 6);
    QVERIFY(surroundingTextChangedSpy.wait());
    QCOMPARE(ti->surroundingText(), QStringLiteral("100 €, 100 $").toUtf8());
    QCOMPARE(ti->surroundingTextCursorPosition(), QStringLiteral("100 €, 100 $").toUtf8().indexOf(','));
    QCOMPARE(ti->surroundingTextSelectionAnchor(), QStringLiteral("100 €, 100 $").toUtf8().indexOf(' ', ti->surroundingTextCursorPosition()));
}

void TextInputTest::testContentHints_data()
{
    QTest::addColumn<KWayland::Client::TextInput::ContentHints>("clientHints");
    QTest::addColumn<KWaylandServer::TextInputContentHints>("serverHints");

    QTest::newRow("completion/v2") << KWayland::Client::TextInput::ContentHints(KWayland::Client::TextInput::ContentHint::AutoCompletion)
                                   << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::AutoCompletion);
    QTest::newRow("Correction/v2") << KWayland::Client::TextInput::ContentHints(KWayland::Client::TextInput::ContentHint::AutoCorrection)
                                   << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::AutoCorrection);
    QTest::newRow("Capitalization/v2") << KWayland::Client::TextInput::ContentHints(KWayland::Client::TextInput::ContentHint::AutoCapitalization)
                                       << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::AutoCapitalization);
    QTest::newRow("Lowercase/v2") << KWayland::Client::TextInput::ContentHints(KWayland::Client::TextInput::ContentHint::LowerCase)
                                  << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::LowerCase);
    QTest::newRow("Uppercase/v2") << KWayland::Client::TextInput::ContentHints(KWayland::Client::TextInput::ContentHint::UpperCase)
                                  << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::UpperCase);
    QTest::newRow("Titlecase/v2") << KWayland::Client::TextInput::ContentHints(KWayland::Client::TextInput::ContentHint::TitleCase)
                                  << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::TitleCase);
    QTest::newRow("HiddenText/v2") << KWayland::Client::TextInput::ContentHints(KWayland::Client::TextInput::ContentHint::HiddenText)
                                   << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::HiddenText);
    QTest::newRow("SensitiveData/v2") << KWayland::Client::TextInput::ContentHints(KWayland::Client::TextInput::ContentHint::SensitiveData)
                                      << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::SensitiveData);
    QTest::newRow("Latin/v2") << KWayland::Client::TextInput::ContentHints(KWayland::Client::TextInput::ContentHint::Latin)
                              << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::Latin);
    QTest::newRow("Multiline/v2") << KWayland::Client::TextInput::ContentHints(KWayland::Client::TextInput::ContentHint::MultiLine)
                                  << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::MultiLine);

    QTest::newRow("autos/v2") << (KWayland::Client::TextInput::ContentHint::AutoCompletion | KWayland::Client::TextInput::ContentHint::AutoCorrection | KWayland::Client::TextInput::ContentHint::AutoCapitalization)
                              << (KWaylandServer::TextInputContentHint::AutoCompletion | KWaylandServer::TextInputContentHint::AutoCorrection
                                  | KWaylandServer::TextInputContentHint::AutoCapitalization);

    // all has combinations which don't make sense - what's both lowercase and uppercase?
    QTest::newRow("all/v2") << (KWayland::Client::TextInput::ContentHint::AutoCompletion | KWayland::Client::TextInput::ContentHint::AutoCorrection | KWayland::Client::TextInput::ContentHint::AutoCapitalization
                                | KWayland::Client::TextInput::ContentHint::LowerCase | KWayland::Client::TextInput::ContentHint::UpperCase | KWayland::Client::TextInput::ContentHint::TitleCase
                                | KWayland::Client::TextInput::ContentHint::HiddenText | KWayland::Client::TextInput::ContentHint::SensitiveData | KWayland::Client::TextInput::ContentHint::Latin
                                | KWayland::Client::TextInput::ContentHint::MultiLine)
                            << (KWaylandServer::TextInputContentHint::AutoCompletion | KWaylandServer::TextInputContentHint::AutoCorrection
                                | KWaylandServer::TextInputContentHint::AutoCapitalization | KWaylandServer::TextInputContentHint::LowerCase
                                | KWaylandServer::TextInputContentHint::UpperCase | KWaylandServer::TextInputContentHint::TitleCase
                                | KWaylandServer::TextInputContentHint::HiddenText | KWaylandServer::TextInputContentHint::SensitiveData
                                | KWaylandServer::TextInputContentHint::Latin | KWaylandServer::TextInputContentHint::MultiLine);
}

void TextInputTest::testContentHints()
{
    // this test verifies that content hints are properly passed from client to server
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    std::unique_ptr<KWayland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);
    QCOMPARE(ti->contentHints(), KWaylandServer::TextInputContentHints());

    QSignalSpy contentTypeChangedSpy(ti, &TextInputV2Interface::contentTypeChanged);
    QFETCH(KWayland::Client::TextInput::ContentHints, clientHints);
    textInput->setContentType(clientHints, KWayland::Client::TextInput::ContentPurpose::Normal);
    QVERIFY(contentTypeChangedSpy.wait());
    QTEST(ti->contentHints(), "serverHints");

    // setting to same should not trigger an update
    textInput->setContentType(clientHints, KWayland::Client::TextInput::ContentPurpose::Normal);
    QVERIFY(!contentTypeChangedSpy.wait(100));

    // unsetting should work
    textInput->setContentType(KWayland::Client::TextInput::ContentHints(), KWayland::Client::TextInput::ContentPurpose::Normal);
    QVERIFY(contentTypeChangedSpy.wait());
    QCOMPARE(ti->contentHints(), KWaylandServer::TextInputContentHints());
}

void TextInputTest::testContentPurpose_data()
{
    QTest::addColumn<KWayland::Client::TextInput::ContentPurpose>("clientPurpose");
    QTest::addColumn<KWaylandServer::TextInputContentPurpose>("serverPurpose");

    QTest::newRow("Alpha/v2") << KWayland::Client::TextInput::ContentPurpose::Alpha << KWaylandServer::TextInputContentPurpose::Alpha;
    QTest::newRow("Digits/v2") << KWayland::Client::TextInput::ContentPurpose::Digits << KWaylandServer::TextInputContentPurpose::Digits;
    QTest::newRow("Number/v2") << KWayland::Client::TextInput::ContentPurpose::Number << KWaylandServer::TextInputContentPurpose::Number;
    QTest::newRow("Phone/v2") << KWayland::Client::TextInput::ContentPurpose::Phone << KWaylandServer::TextInputContentPurpose::Phone;
    QTest::newRow("Url/v2") << KWayland::Client::TextInput::ContentPurpose::Url << KWaylandServer::TextInputContentPurpose::Url;
    QTest::newRow("Email/v2") << KWayland::Client::TextInput::ContentPurpose::Email << KWaylandServer::TextInputContentPurpose::Email;
    QTest::newRow("Name/v2") << KWayland::Client::TextInput::ContentPurpose::Name << KWaylandServer::TextInputContentPurpose::Name;
    QTest::newRow("Password/v2") << KWayland::Client::TextInput::ContentPurpose::Password << KWaylandServer::TextInputContentPurpose::Password;
    QTest::newRow("Date/v2") << KWayland::Client::TextInput::ContentPurpose::Date << KWaylandServer::TextInputContentPurpose::Date;
    QTest::newRow("Time/v2") << KWayland::Client::TextInput::ContentPurpose::Time << KWaylandServer::TextInputContentPurpose::Time;
    QTest::newRow("Datetime/v2") << KWayland::Client::TextInput::ContentPurpose::DateTime << KWaylandServer::TextInputContentPurpose::DateTime;
    QTest::newRow("Terminal/v2") << KWayland::Client::TextInput::ContentPurpose::Terminal << KWaylandServer::TextInputContentPurpose::Terminal;
}

void TextInputTest::testContentPurpose()
{
    // this test verifies that content purpose are properly passed from client to server
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    std::unique_ptr<KWayland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);
    QCOMPARE(ti->contentPurpose(), KWaylandServer::TextInputContentPurpose::Normal);

    QSignalSpy contentTypeChangedSpy(ti, &TextInputV2Interface::contentTypeChanged);
    QFETCH(KWayland::Client::TextInput::ContentPurpose, clientPurpose);
    textInput->setContentType(KWayland::Client::TextInput::ContentHints(), clientPurpose);
    QVERIFY(contentTypeChangedSpy.wait());
    QTEST(ti->contentPurpose(), "serverPurpose");

    // setting to same should not trigger an update
    textInput->setContentType(KWayland::Client::TextInput::ContentHints(), clientPurpose);
    QVERIFY(!contentTypeChangedSpy.wait(100));

    // unsetting should work
    textInput->setContentType(KWayland::Client::TextInput::ContentHints(), KWayland::Client::TextInput::ContentPurpose::Normal);
    QVERIFY(contentTypeChangedSpy.wait());
    QCOMPARE(ti->contentPurpose(), KWaylandServer::TextInputContentPurpose::Normal);
}

void TextInputTest::testTextDirection_data()
{
    QTest::addColumn<Qt::LayoutDirection>("textDirection");

    QTest::newRow("ltr/v0") << Qt::LeftToRight;
    QTest::newRow("rtl/v0") << Qt::RightToLeft;

    QTest::newRow("ltr/v2") << Qt::LeftToRight;
    QTest::newRow("rtl/v2") << Qt::RightToLeft;
}

void TextInputTest::testTextDirection()
{
    // this test verifies that the text direction is sent from server to client
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    std::unique_ptr<KWayland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    // default should be auto
    QCOMPARE(textInput->textDirection(), Qt::LayoutDirectionAuto);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);

    // let's send the new text direction
    QSignalSpy textDirectionChangedSpy(textInput.get(), &KWayland::Client::TextInput::textDirectionChanged);
    QFETCH(Qt::LayoutDirection, textDirection);
    ti->setTextDirection(textDirection);
    QVERIFY(textDirectionChangedSpy.wait());
    QCOMPARE(textInput->textDirection(), textDirection);
    // setting again should not change
    ti->setTextDirection(textDirection);
    QVERIFY(!textDirectionChangedSpy.wait(100));

    // setting back to auto
    ti->setTextDirection(Qt::LayoutDirectionAuto);
    QVERIFY(textDirectionChangedSpy.wait());
    QCOMPARE(textInput->textDirection(), Qt::LayoutDirectionAuto);
}

void TextInputTest::testLanguage()
{
    // this test verifies that language is sent from server to client
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    std::unique_ptr<KWayland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    // default should be empty
    QVERIFY(textInput->language().isEmpty());
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);

    // let's send the new language
    QSignalSpy langugageChangedSpy(textInput.get(), &KWayland::Client::TextInput::languageChanged);
    ti->setLanguage(QByteArrayLiteral("foo"));
    QVERIFY(langugageChangedSpy.wait());
    QCOMPARE(textInput->language(), QByteArrayLiteral("foo"));
    // setting to same should not trigger
    ti->setLanguage(QByteArrayLiteral("foo"));
    QVERIFY(!langugageChangedSpy.wait(100));
    // but to something else should trigger again
    ti->setLanguage(QByteArrayLiteral("bar"));
    QVERIFY(langugageChangedSpy.wait());
    QCOMPARE(textInput->language(), QByteArrayLiteral("bar"));
}

void TextInputTest::testKeyEvent()
{
    qRegisterMetaType<Qt::KeyboardModifiers>();
    qRegisterMetaType<KWayland::Client::TextInput::KeyState>();
    // this test verifies that key events are properly sent to the client
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    std::unique_ptr<KWayland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);

    // TODO: test modifiers
    QSignalSpy keyEventSpy(textInput.get(), &KWayland::Client::TextInput::keyEvent);
    m_seatInterface->setTimestamp(100ms);
    ti->keysymPressed(2);
    QVERIFY(keyEventSpy.wait());
    QCOMPARE(keyEventSpy.count(), 1);
    QCOMPARE(keyEventSpy.last().at(0).value<quint32>(), 2u);
    QCOMPARE(keyEventSpy.last().at(1).value<KWayland::Client::TextInput::KeyState>(), KWayland::Client::TextInput::KeyState::Pressed);
    QCOMPARE(keyEventSpy.last().at(2).value<Qt::KeyboardModifiers>(), Qt::KeyboardModifiers());
    QCOMPARE(keyEventSpy.last().at(3).value<quint32>(), 100u);
    m_seatInterface->setTimestamp(101ms);
    ti->keysymReleased(2);
    QVERIFY(keyEventSpy.wait());
    QCOMPARE(keyEventSpy.count(), 2);
    QCOMPARE(keyEventSpy.last().at(0).value<quint32>(), 2u);
    QCOMPARE(keyEventSpy.last().at(1).value<KWayland::Client::TextInput::KeyState>(), KWayland::Client::TextInput::KeyState::Released);
    QCOMPARE(keyEventSpy.last().at(2).value<Qt::KeyboardModifiers>(), Qt::KeyboardModifiers());
    QCOMPARE(keyEventSpy.last().at(3).value<quint32>(), 101u);
}

void TextInputTest::testPreEdit()
{
    // this test verifies that pre-edit is correctly passed to the client
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    std::unique_ptr<KWayland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    // verify default values
    QVERIFY(textInput->composingText().isEmpty());
    QVERIFY(textInput->composingFallbackText().isEmpty());
    QCOMPARE(textInput->composingTextCursorPosition(), 0);

    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);

    // now let's pass through some pre-edit events
    QSignalSpy composingTextChangedSpy(textInput.get(), &KWayland::Client::TextInput::composingTextChanged);
    ti->setPreEditCursor(1);
    ti->preEdit(QByteArrayLiteral("foo"), QByteArrayLiteral("bar"));
    QVERIFY(composingTextChangedSpy.wait());
    QCOMPARE(composingTextChangedSpy.count(), 1);
    QCOMPARE(textInput->composingText(), QByteArrayLiteral("foo"));
    QCOMPARE(textInput->composingFallbackText(), QByteArrayLiteral("bar"));
    QCOMPARE(textInput->composingTextCursorPosition(), 1);

    // when no pre edit cursor is sent, it's at end of text
    ti->preEdit(QByteArrayLiteral("foobar"), QByteArray());
    QVERIFY(composingTextChangedSpy.wait());
    QCOMPARE(composingTextChangedSpy.count(), 2);
    QCOMPARE(textInput->composingText(), QByteArrayLiteral("foobar"));
    QCOMPARE(textInput->composingFallbackText(), QByteArray());
    QCOMPARE(textInput->composingTextCursorPosition(), 6);
}

void TextInputTest::testCommit()
{
    // this test verifies that the commit is handled correctly by the client
    std::unique_ptr<KWayland::Client::Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    std::unique_ptr<KWayland::Client::TextInput> textInput(createTextInput());
    QVERIFY(textInput != nullptr);
    // verify default values
    QCOMPARE(textInput->commitText(), QByteArray());
    QCOMPARE(textInput->cursorPosition(), 0);
    QCOMPARE(textInput->anchorPosition(), 0);
    QCOMPARE(textInput->deleteSurroundingText().beforeLength, 0u);
    QCOMPARE(textInput->deleteSurroundingText().afterLength, 0u);

    textInput->enable(surface.get());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);

    // now let's commit
    QSignalSpy committedSpy(textInput.get(), &KWayland::Client::TextInput::committed);
    ti->setCursorPosition(3, 4);
    ti->deleteSurroundingText(2, 1);
    ti->commitString(QByteArrayLiteral("foo"));

    QVERIFY(committedSpy.wait());
    QCOMPARE(textInput->commitText(), QByteArrayLiteral("foo"));
    QCOMPARE(textInput->cursorPosition(), 3);
    QCOMPARE(textInput->anchorPosition(), 4);
    QCOMPARE(textInput->deleteSurroundingText().beforeLength, 2u);
    QCOMPARE(textInput->deleteSurroundingText().afterLength, 1u);
}

QTEST_GUILESS_MAIN(TextInputTest)
#include "test_text_input_v2.moc"
