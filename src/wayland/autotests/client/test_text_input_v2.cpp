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
#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/seat_interface.h"
#include "../../src/server/textinput.h"
#include "../../src/server/textinput_v2_interface.h"

using namespace KWayland::Client;
using namespace KWaylandServer;

class TextInputTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testEnterLeave_data();
    void testEnterLeave();
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
    TextInput *createTextInput();
    Display *m_display = nullptr;
    SeatInterface *m_seatInterface = nullptr;
    CompositorInterface *m_compositorInterface = nullptr;
    TextInputManagerV2Interface *m_textInputManagerV2Interface = nullptr;
    ConnectionThread *m_connection = nullptr;
    QThread *m_thread = nullptr;
    EventQueue *m_queue = nullptr;
    Seat *m_seat = nullptr;
    Keyboard *m_keyboard = nullptr;
    Compositor *m_compositor = nullptr;
    TextInputManager *m_textInputManagerV2 = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-text-input-0");

void TextInputTest::init()
{
    delete m_display;
    m_display = new Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
    m_seatInterface = new SeatInterface(m_display, m_display);
    m_seatInterface->setHasKeyboard(true);
    m_seatInterface->setHasTouch(true);
    m_seatInterface->create();
    m_compositorInterface = new CompositorInterface(m_display, m_display);
    m_textInputManagerV2Interface = new TextInputManagerV2Interface(m_display, m_display);

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new EventQueue(this);
    m_queue->setup(m_connection);

    Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    m_seat = registry.createSeat(registry.interface(Registry::Interface::Seat).name,
                                 registry.interface(Registry::Interface::Seat).version,
                                 this);
    QVERIFY(m_seat->isValid());
    QSignalSpy hasKeyboardSpy(m_seat, &Seat::hasKeyboardChanged);
    QVERIFY(hasKeyboardSpy.isValid());
    QVERIFY(hasKeyboardSpy.wait());
    m_keyboard = m_seat->createKeyboard(this);
    QVERIFY(m_keyboard->isValid());

    m_compositor = registry.createCompositor(registry.interface(Registry::Interface::Compositor).name,
                                             registry.interface(Registry::Interface::Compositor).version,
                                             this);
    QVERIFY(m_compositor->isValid());



    m_textInputManagerV2 = registry.createTextInputManager(registry.interface(Registry::Interface::TextInputManagerUnstableV2).name,
                                                           registry.interface(Registry::Interface::TextInputManagerUnstableV2).version,
                                                           this);
    QVERIFY(m_textInputManagerV2->isValid());
}

void TextInputTest::cleanup()
{
#define CLEANUP(variable) \
    if (variable) { \
        delete variable; \
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
    return surfaceCreatedSpy.first().first().value<SurfaceInterface*>();
}

TextInput *TextInputTest::createTextInput()
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
    QScopedPointer<Surface> surface(m_compositor->createSurface());

    QScopedPointer<TextInput> textInput(createTextInput());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QVERIFY(!textInput.isNull());
    QSignalSpy enteredSpy(textInput.data(), &TextInput::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(textInput.data(), &TextInput::left);
    QVERIFY(leftSpy.isValid());
    QSignalSpy textInputChangedSpy(m_seatInterface, &SeatInterface::focusedTextInputSurfaceChanged);
    QVERIFY(textInputChangedSpy.isValid());

    // now let's try to enter it
    QVERIFY(!m_seatInterface->focusedTextInputSurface());
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_seatInterface->focusedTextInputSurface(), serverSurface);
    // text input not yet set for the surface
    QFETCH(bool, updatesDirectly);
    QCOMPARE(bool(m_seatInterface->textInputV2()), updatesDirectly);
    QCOMPARE(textInputChangedSpy.isEmpty(), !updatesDirectly);
    textInput->enable(surface.data());
    // this should trigger on server side
    if (!updatesDirectly) {
        QVERIFY(textInputChangedSpy.wait());
    }
    QCOMPARE(textInputChangedSpy.count(), 1);
    auto serverTextInput = m_seatInterface->textInputV2();
    QVERIFY(serverTextInput);
    QSignalSpy enabledChangedSpy(serverTextInput, &TextInputV2Interface::enabledChanged);
    QVERIFY(enabledChangedSpy.isValid());
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
    QCOMPARE(textInput->enteredSurface(), surface.data());

    // now trigger a leave
    m_seatInterface->setFocusedKeyboardSurface(nullptr);
    QCOMPARE(textInputChangedSpy.count(), 2);
    QVERIFY(leftSpy.wait());
    QVERIFY(!textInput->enteredSurface());
    QVERIFY(serverTextInput->isEnabled());

    // if we enter again we should directly get the text input as it's still activated
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(textInputChangedSpy.count(), 3);
    QVERIFY(m_seatInterface->textInputV2());
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(textInput->enteredSurface(), surface.data());
    QVERIFY(serverTextInput->isEnabled());

    // let's deactivate on client side
    textInput->disable(surface.data());
    QVERIFY(enabledChangedSpy.wait());
    QCOMPARE(enabledChangedSpy.count(), 1);
    QVERIFY(!serverTextInput->isEnabled());
    // does not trigger a leave
    QCOMPARE(textInputChangedSpy.count(), 3);
    // should still be the same text input
    QCOMPARE(m_seatInterface->textInputV2(), serverTextInput);
    //reset
    textInput->enable(surface.data());
    QVERIFY(enabledChangedSpy.wait());

    //delete the client and wait for the server to catch up
    QSignalSpy unboundSpy(serverSurface, &QObject::destroyed);
    surface.reset();
    QVERIFY(unboundSpy.wait());
    QVERIFY(leftSpy.wait());
    QVERIFY(!textInput->enteredSurface());
}

void TextInputTest::testShowHidePanel()
{
    // this test verifies that the requests for show/hide panel work
    // and that status is properly sent to the client
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    QScopedPointer<TextInput> textInput(createTextInput());
    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);

    QSignalSpy showPanelRequestedSpy(ti, &TextInputV2Interface::requestShowInputPanel);
    QVERIFY(showPanelRequestedSpy.isValid());
    QSignalSpy hidePanelRequestedSpy(ti, &TextInputV2Interface::requestHideInputPanel);
    QVERIFY(hidePanelRequestedSpy.isValid());
    QSignalSpy inputPanelStateChangedSpy(textInput.data(), &TextInput::inputPanelStateChanged);
    QVERIFY(inputPanelStateChangedSpy.isValid());

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
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    QScopedPointer<TextInput> textInput(createTextInput());
    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);
    QCOMPARE(ti->cursorRectangle(), QRect());
    QSignalSpy cursorRectangleChangedSpy(ti, &TextInputV2Interface::cursorRectangleChanged);
    QVERIFY(cursorRectangleChangedSpy.isValid());

    textInput->setCursorRectangle(QRect(10, 20, 30, 40));
    QVERIFY(cursorRectangleChangedSpy.wait());
    QCOMPARE(ti->cursorRectangle(), QRect(10, 20, 30, 40));
}

void TextInputTest::testPreferredLanguage()
{
    // this test verifies that passing the preferred language from client to server works
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    QScopedPointer<TextInput> textInput(createTextInput());
    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);
    QVERIFY(ti->preferredLanguage().isEmpty());

    QSignalSpy preferredLanguageChangedSpy(ti, &TextInputV2Interface::preferredLanguageChanged);
    QVERIFY(preferredLanguageChangedSpy.isValid());
    textInput->setPreferredLanguage(QStringLiteral("foo"));
    QVERIFY(preferredLanguageChangedSpy.wait());
    QCOMPARE(ti->preferredLanguage(), QStringLiteral("foo").toUtf8());
}

void TextInputTest::testReset()
{
    // this test verifies that the reset request is properly passed from client to server
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    QScopedPointer<TextInput> textInput(createTextInput());
    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);

    QSignalSpy stateUpdatedSpy(ti, &TextInputV2Interface::stateUpdated);
    QVERIFY(stateUpdatedSpy.isValid());

    textInput->reset();
    QVERIFY(stateUpdatedSpy.wait());
}

void TextInputTest::testSurroundingText()
{
    // this test verifies that surrounding text is properly passed around
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    QScopedPointer<TextInput> textInput(createTextInput());
    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);
    QVERIFY(ti->surroundingText().isEmpty());
    QCOMPARE(ti->surroundingTextCursorPosition(), 0);
    QCOMPARE(ti->surroundingTextSelectionAnchor(), 0);

    QSignalSpy surroundingTextChangedSpy(ti, &TextInputV2Interface::surroundingTextChanged);
    QVERIFY(surroundingTextChangedSpy.isValid());

    textInput->setSurroundingText(QStringLiteral("100 €, 100 $"), 5, 6);
    QVERIFY(surroundingTextChangedSpy.wait());
    QCOMPARE(ti->surroundingText(), QStringLiteral("100 €, 100 $").toUtf8());
    QCOMPARE(ti->surroundingTextCursorPosition(), QStringLiteral("100 €, 100 $").toUtf8().indexOf(','));
    QCOMPARE(ti->surroundingTextSelectionAnchor(), QStringLiteral("100 €, 100 $").toUtf8().indexOf(' ', ti->surroundingTextCursorPosition()));
}

void TextInputTest::testContentHints_data()
{
    QTest::addColumn<TextInput::ContentHints>("clientHints");
    QTest::addColumn<KWaylandServer::TextInputContentHints>("serverHints");

    QTest::newRow("completion/v2")     << TextInput::ContentHints(TextInput::ContentHint::AutoCompletion) << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::AutoCompletion);
    QTest::newRow("Correction/v2")     << TextInput::ContentHints(TextInput::ContentHint::AutoCorrection) << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::AutoCorrection);
    QTest::newRow("Capitalization/v2") << TextInput::ContentHints(TextInput::ContentHint::AutoCapitalization) << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::AutoCapitalization);
    QTest::newRow("Lowercase/v2")      << TextInput::ContentHints(TextInput::ContentHint::LowerCase) << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::LowerCase);
    QTest::newRow("Uppercase/v2")      << TextInput::ContentHints(TextInput::ContentHint::UpperCase) << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::UpperCase);
    QTest::newRow("Titlecase/v2")      << TextInput::ContentHints(TextInput::ContentHint::TitleCase) << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::TitleCase);
    QTest::newRow("HiddenText/v2")     << TextInput::ContentHints(TextInput::ContentHint::HiddenText) << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::HiddenText);
    QTest::newRow("SensitiveData/v2")  << TextInput::ContentHints(TextInput::ContentHint::SensitiveData) << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::SensitiveData);
    QTest::newRow("Latin/v2")          << TextInput::ContentHints(TextInput::ContentHint::Latin) << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::Latin);
    QTest::newRow("Multiline/v2")      << TextInput::ContentHints(TextInput::ContentHint::MultiLine) << KWaylandServer::TextInputContentHints(KWaylandServer::TextInputContentHint::MultiLine);

    QTest::newRow("autos/v2") << (TextInput::ContentHint::AutoCompletion | TextInput::ContentHint::AutoCorrection | TextInput::ContentHint::AutoCapitalization)
                              << (KWaylandServer::TextInputContentHint::AutoCompletion | KWaylandServer::TextInputContentHint::AutoCorrection | KWaylandServer::TextInputContentHint::AutoCapitalization);

    // all has combinations which don't make sense - what's both lowercase and uppercase?
    QTest::newRow("all/v2") << (TextInput::ContentHint::AutoCompletion |
                                TextInput::ContentHint::AutoCorrection |
                                TextInput::ContentHint::AutoCapitalization |
                                TextInput::ContentHint::LowerCase |
                                TextInput::ContentHint::UpperCase |
                                TextInput::ContentHint::TitleCase |
                                TextInput::ContentHint::HiddenText |
                                TextInput::ContentHint::SensitiveData |
                                TextInput::ContentHint::Latin |
                                TextInput::ContentHint::MultiLine)
                            << (KWaylandServer::TextInputContentHint::AutoCompletion |
                                KWaylandServer::TextInputContentHint::AutoCorrection |
                                KWaylandServer::TextInputContentHint::AutoCapitalization |
                                KWaylandServer::TextInputContentHint::LowerCase |
                                KWaylandServer::TextInputContentHint::UpperCase |
                                KWaylandServer::TextInputContentHint::TitleCase |
                                KWaylandServer::TextInputContentHint::HiddenText |
                                KWaylandServer::TextInputContentHint::SensitiveData |
                                KWaylandServer::TextInputContentHint::Latin |
                                KWaylandServer::TextInputContentHint::MultiLine);

}

void TextInputTest::testContentHints()
{
    // this test verifies that content hints are properly passed from client to server
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    QScopedPointer<TextInput> textInput(createTextInput());
    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);
    QCOMPARE(ti->contentHints(), KWaylandServer::TextInputContentHints());

    QSignalSpy contentTypeChangedSpy(ti, &TextInputV2Interface::contentTypeChanged);
    QVERIFY(contentTypeChangedSpy.isValid());
    QFETCH(TextInput::ContentHints, clientHints);
    textInput->setContentType(clientHints, TextInput::ContentPurpose::Normal);
    QVERIFY(contentTypeChangedSpy.wait());
    QTEST(ti->contentHints(), "serverHints");

    // setting to same should not trigger an update
    textInput->setContentType(clientHints, TextInput::ContentPurpose::Normal);
    QVERIFY(!contentTypeChangedSpy.wait(100));

    // unsetting should work
    textInput->setContentType(TextInput::ContentHints(), TextInput::ContentPurpose::Normal);
    QVERIFY(contentTypeChangedSpy.wait());
    QCOMPARE(ti->contentHints(), KWaylandServer::TextInputContentHints());
}

void TextInputTest::testContentPurpose_data()
{

    QTest::addColumn<TextInput::ContentPurpose>("clientPurpose");
    QTest::addColumn<KWaylandServer::TextInputContentPurpose>("serverPurpose");

    QTest::newRow("Alpha/v2")    << TextInput::ContentPurpose::Alpha    << KWaylandServer::TextInputContentPurpose::Alpha;
    QTest::newRow("Digits/v2")   << TextInput::ContentPurpose::Digits   << KWaylandServer::TextInputContentPurpose::Digits;
    QTest::newRow("Number/v2")   << TextInput::ContentPurpose::Number   << KWaylandServer::TextInputContentPurpose::Number;
    QTest::newRow("Phone/v2")    << TextInput::ContentPurpose::Phone    << KWaylandServer::TextInputContentPurpose::Phone;
    QTest::newRow("Url/v2")      << TextInput::ContentPurpose::Url      << KWaylandServer::TextInputContentPurpose::Url;
    QTest::newRow("Email/v2")    << TextInput::ContentPurpose::Email    << KWaylandServer::TextInputContentPurpose::Email;
    QTest::newRow("Name/v2")     << TextInput::ContentPurpose::Name     << KWaylandServer::TextInputContentPurpose::Name;
    QTest::newRow("Password/v2") << TextInput::ContentPurpose::Password << KWaylandServer::TextInputContentPurpose::Password;
    QTest::newRow("Date/v2")     << TextInput::ContentPurpose::Date     << KWaylandServer::TextInputContentPurpose::Date;
    QTest::newRow("Time/v2")     << TextInput::ContentPurpose::Time     << KWaylandServer::TextInputContentPurpose::Time;
    QTest::newRow("Datetime/v2") << TextInput::ContentPurpose::DateTime << KWaylandServer::TextInputContentPurpose::DateTime;
    QTest::newRow("Terminal/v2") << TextInput::ContentPurpose::Terminal << KWaylandServer::TextInputContentPurpose::Terminal;
}

void TextInputTest::testContentPurpose()
{
    // this test verifies that content purpose are properly passed from client to server
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    QScopedPointer<TextInput> textInput(createTextInput());
    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);
    QCOMPARE(ti->contentPurpose(), KWaylandServer::TextInputContentPurpose::Normal);

    QSignalSpy contentTypeChangedSpy(ti, &TextInputV2Interface::contentTypeChanged);
    QVERIFY(contentTypeChangedSpy.isValid());
    QFETCH(TextInput::ContentPurpose, clientPurpose);
    textInput->setContentType(TextInput::ContentHints(), clientPurpose);
    QVERIFY(contentTypeChangedSpy.wait());
    QTEST(ti->contentPurpose(), "serverPurpose");

    // setting to same should not trigger an update
    textInput->setContentType(TextInput::ContentHints(), clientPurpose);
    QVERIFY(!contentTypeChangedSpy.wait(100));

    // unsetting should work
    textInput->setContentType(TextInput::ContentHints(), TextInput::ContentPurpose::Normal);
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
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    QScopedPointer<TextInput> textInput(createTextInput());
    QVERIFY(!textInput.isNull());
    // default should be auto
    QCOMPARE(textInput->textDirection(), Qt::LayoutDirectionAuto);
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);

    // let's send the new text direction
    QSignalSpy textDirectionChangedSpy(textInput.data(), &TextInput::textDirectionChanged);
    QVERIFY(textDirectionChangedSpy.isValid());
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
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    QScopedPointer<TextInput> textInput(createTextInput());
    QVERIFY(!textInput.isNull());
    // default should be empty
    QVERIFY(textInput->language().isEmpty());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);

    // let's send the new language
    QSignalSpy langugageChangedSpy(textInput.data(), &TextInput::languageChanged);
    QVERIFY(langugageChangedSpy.isValid());
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
    qRegisterMetaType<TextInput::KeyState>();
    // this test verifies that key events are properly sent to the client
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    QScopedPointer<TextInput> textInput(createTextInput());
    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);

    // TODO: test modifiers
    QSignalSpy keyEventSpy(textInput.data(), &TextInput::keyEvent);
    QVERIFY(keyEventSpy.isValid());
    m_seatInterface->setTimestamp(100);
    ti->keysymPressed(2);
    QVERIFY(keyEventSpy.wait());
    QCOMPARE(keyEventSpy.count(), 1);
    QCOMPARE(keyEventSpy.last().at(0).value<quint32>(), 2u);
    QCOMPARE(keyEventSpy.last().at(1).value<TextInput::KeyState>(), TextInput::KeyState::Pressed);
    QCOMPARE(keyEventSpy.last().at(2).value<Qt::KeyboardModifiers>(), Qt::KeyboardModifiers());
    QCOMPARE(keyEventSpy.last().at(3).value<quint32>(), 100u);
    m_seatInterface->setTimestamp(101);
    ti->keysymReleased(2);
    QVERIFY(keyEventSpy.wait());
    QCOMPARE(keyEventSpy.count(), 2);
    QCOMPARE(keyEventSpy.last().at(0).value<quint32>(), 2u);
    QCOMPARE(keyEventSpy.last().at(1).value<TextInput::KeyState>(), TextInput::KeyState::Released);
    QCOMPARE(keyEventSpy.last().at(2).value<Qt::KeyboardModifiers>(), Qt::KeyboardModifiers());
    QCOMPARE(keyEventSpy.last().at(3).value<quint32>(), 101u);
}

void TextInputTest::testPreEdit()
{
    // this test verifies that pre-edit is correctly passed to the client
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    QScopedPointer<TextInput> textInput(createTextInput());
    QVERIFY(!textInput.isNull());
    // verify default values
    QVERIFY(textInput->composingText().isEmpty());
    QVERIFY(textInput->composingFallbackText().isEmpty());
    QCOMPARE(textInput->composingTextCursorPosition(), 0);

    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);

    // now let's pass through some pre-edit events
    QSignalSpy composingTextChangedSpy(textInput.data(), &TextInput::composingTextChanged);
    QVERIFY(composingTextChangedSpy.isValid());
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
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);

    QScopedPointer<TextInput> textInput(createTextInput());
    QVERIFY(!textInput.isNull());
    // verify default values
    QCOMPARE(textInput->commitText(), QByteArray());
    QCOMPARE(textInput->cursorPosition(), 0);
    QCOMPARE(textInput->anchorPosition(), 0);
    QCOMPARE(textInput->deleteSurroundingText().beforeLength, 0u);
    QCOMPARE(textInput->deleteSurroundingText().afterLength, 0u);

    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->textInputV2();
    QVERIFY(ti);

    // now let's commit
    QSignalSpy committedSpy(textInput.data(), &TextInput::committed);
    QVERIFY(committedSpy.isValid());
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
