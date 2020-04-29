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
#include "../../src/server/textinput_interface.h"

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
    void testShowHidePanel_data();
    void testShowHidePanel();
    void testCursorRectangle_data();
    void testCursorRectangle();
    void testPreferredLanguage_data();
    void testPreferredLanguage();
    void testReset_data();
    void testReset();
    void testSurroundingText_data();
    void testSurroundingText();
    void testContentHints_data();
    void testContentHints();
    void testContentPurpose_data();
    void testContentPurpose();
    void testTextDirection_data();
    void testTextDirection();
    void testLanguage_data();
    void testLanguage();
    void testKeyEvent_data();
    void testKeyEvent();
    void testPreEdit_data();
    void testPreEdit();
    void testCommit_data();
    void testCommit();

private:
    SurfaceInterface *waitForSurface();
    TextInput *createTextInput(TextInputInterfaceVersion version);
    Display *m_display = nullptr;
    SeatInterface *m_seatInterface = nullptr;
    CompositorInterface *m_compositorInterface = nullptr;
    TextInputManagerInterface *m_textInputManagerV0Interface = nullptr;
    TextInputManagerInterface *m_textInputManagerV2Interface = nullptr;
    ConnectionThread *m_connection = nullptr;
    QThread *m_thread = nullptr;
    EventQueue *m_queue = nullptr;
    Seat *m_seat = nullptr;
    Keyboard *m_keyboard = nullptr;
    Compositor *m_compositor = nullptr;
    TextInputManager *m_textInputManagerV0 = nullptr;
    TextInputManager *m_textInputManagerV2 = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-text-input-0");

void TextInputTest::init()
{
    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
    m_seatInterface = m_display->createSeat();
    m_seatInterface->setHasKeyboard(true);
    m_seatInterface->setHasTouch(true);
    m_seatInterface->create();
    m_compositorInterface = m_display->createCompositor();
    m_compositorInterface->create();
    m_textInputManagerV0Interface = m_display->createTextInputManager(TextInputInterfaceVersion::UnstableV0);
    m_textInputManagerV0Interface->create();
    m_textInputManagerV2Interface = m_display->createTextInputManager(TextInputInterfaceVersion::UnstableV2);
    m_textInputManagerV2Interface->create();

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

    m_textInputManagerV0 = registry.createTextInputManager(registry.interface(Registry::Interface::TextInputManagerUnstableV0).name,
                                                           registry.interface(Registry::Interface::TextInputManagerUnstableV0).version,
                                                           this);
    QVERIFY(m_textInputManagerV0->isValid());

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
    CLEANUP(m_textInputManagerV0)
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

    CLEANUP(m_textInputManagerV0Interface)
    CLEANUP(m_textInputManagerV2Interface)
    CLEANUP(m_compositorInterface)
    CLEANUP(m_seatInterface)
    CLEANUP(m_display)
#undef CLEANUP
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

TextInput *TextInputTest::createTextInput(TextInputInterfaceVersion version)
{
    switch (version) {
    case TextInputInterfaceVersion::UnstableV0:
        return m_textInputManagerV0->createTextInput(m_seat);
    case TextInputInterfaceVersion::UnstableV2:
        return m_textInputManagerV2->createTextInput(m_seat);
    default:
        Q_UNREACHABLE();
        return nullptr;
    }
}

void TextInputTest::testEnterLeave_data()
{
    QTest::addColumn<TextInputInterfaceVersion>("version");
    QTest::addColumn<bool>("updatesDirectly");

    QTest::newRow("UnstableV0") << TextInputInterfaceVersion::UnstableV0 << false;
    QTest::newRow("UnstableV2") << TextInputInterfaceVersion::UnstableV2 << true;
}

void TextInputTest::testEnterLeave()
{
    // this test verifies that enter leave are sent correctly
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    QFETCH(TextInputInterfaceVersion, version);
    QScopedPointer<TextInput> textInput(createTextInput(version));
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QVERIFY(!textInput.isNull());
    QSignalSpy enteredSpy(textInput.data(), &TextInput::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(textInput.data(), &TextInput::left);
    QVERIFY(leftSpy.isValid());
    QSignalSpy textInputChangedSpy(m_seatInterface, &SeatInterface::focusedTextInputChanged);
    QVERIFY(textInputChangedSpy.isValid());

    // now let's try to enter it
    QVERIFY(!m_seatInterface->focusedTextInput());
    QVERIFY(!m_seatInterface->focusedTextInputSurface());
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(m_seatInterface->focusedTextInputSurface(), serverSurface);
    // text input not yet set for the surface
    QFETCH(bool, updatesDirectly);
    QCOMPARE(bool(m_seatInterface->focusedTextInput()), updatesDirectly);
    QCOMPARE(textInputChangedSpy.isEmpty(), !updatesDirectly);
    textInput->enable(surface.data());
    // this should trigger on server side
    if (!updatesDirectly) {
        QVERIFY(textInputChangedSpy.wait());
    }
    QCOMPARE(textInputChangedSpy.count(), 1);
    auto serverTextInput = m_seatInterface->focusedTextInput();
    QVERIFY(serverTextInput);
    QCOMPARE(serverTextInput->interfaceVersion(), version);
    QSignalSpy enabledChangedSpy(serverTextInput, &TextInputInterface::enabledChanged);
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
    QVERIFY(!m_seatInterface->focusedTextInput());
    QVERIFY(leftSpy.wait());
    QVERIFY(!textInput->enteredSurface());
    QVERIFY(serverTextInput->isEnabled());

    // if we enter again we should directly get the text input as it's still activated
    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    QCOMPARE(textInputChangedSpy.count(), 3);
    QVERIFY(m_seatInterface->focusedTextInput());
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
    QCOMPARE(m_seatInterface->focusedTextInput(), serverTextInput);
    //reset
    textInput->enable(surface.data());
    QVERIFY(enabledChangedSpy.wait());

    //trigger an enter again and leave, but this
    //time we try sending an event after the surface is unbound
    //but not yet destroyed. It should work without errors
    QCOMPARE(textInput->enteredSurface(), surface.data());
    connect(serverSurface, &Resource::unbound, [=]() {
        m_seatInterface->setFocusedKeyboardSurface(nullptr);
    });
    //delete the client and wait for the server to catch up
    QSignalSpy unboundSpy(serverSurface, &QObject::destroyed);
    surface.reset();
    QVERIFY(unboundSpy.wait());
    QVERIFY(leftSpy.wait());
    QVERIFY(!textInput->enteredSurface());
}

void TextInputTest::testShowHidePanel_data()
{
    QTest::addColumn<TextInputInterfaceVersion>("version");

    QTest::newRow("UnstableV0") << TextInputInterfaceVersion::UnstableV0;
    QTest::newRow("UnstableV2") << TextInputInterfaceVersion::UnstableV2;
}

void TextInputTest::testShowHidePanel()
{
    // this test verifies that the requests for show/hide panel work
    // and that status is properly sent to the client
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QFETCH(TextInputInterfaceVersion, version);
    QScopedPointer<TextInput> textInput(createTextInput(version));
    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->focusedTextInput();
    QVERIFY(ti);

    QSignalSpy showPanelRequestedSpy(ti, &TextInputInterface::requestShowInputPanel);
    QVERIFY(showPanelRequestedSpy.isValid());
    QSignalSpy hidePanelRequestedSpy(ti, &TextInputInterface::requestHideInputPanel);
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

void TextInputTest::testCursorRectangle_data()
{
    QTest::addColumn<TextInputInterfaceVersion>("version");

    QTest::newRow("UnstableV0") << TextInputInterfaceVersion::UnstableV0;
    QTest::newRow("UnstableV2") << TextInputInterfaceVersion::UnstableV2;
}

void TextInputTest::testCursorRectangle()
{
    // this test verifies that passing the cursor rectangle from client to server works
    // and that setting visibility state from server to client works
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QFETCH(TextInputInterfaceVersion, version);
    QScopedPointer<TextInput> textInput(createTextInput(version));
    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->focusedTextInput();
    QVERIFY(ti);
    QCOMPARE(ti->cursorRectangle(), QRect());
    QSignalSpy cursorRectangleChangedSpy(ti, &TextInputInterface::cursorRectangleChanged);
    QVERIFY(cursorRectangleChangedSpy.isValid());

    textInput->setCursorRectangle(QRect(10, 20, 30, 40));
    QVERIFY(cursorRectangleChangedSpy.wait());
    QCOMPARE(ti->cursorRectangle(), QRect(10, 20, 30, 40));
}

void TextInputTest::testPreferredLanguage_data()
{
    QTest::addColumn<TextInputInterfaceVersion>("version");

    QTest::newRow("UnstableV0") << TextInputInterfaceVersion::UnstableV0;
    QTest::newRow("UnstableV2") << TextInputInterfaceVersion::UnstableV2;
}

void TextInputTest::testPreferredLanguage()
{
    // this test verifies that passing the preferred language from client to server works
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QFETCH(TextInputInterfaceVersion, version);
    QScopedPointer<TextInput> textInput(createTextInput(version));
    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->focusedTextInput();
    QVERIFY(ti);
    QVERIFY(ti->preferredLanguage().isEmpty());

    QSignalSpy preferredLanguageChangedSpy(ti, &TextInputInterface::preferredLanguageChanged);
    QVERIFY(preferredLanguageChangedSpy.isValid());
    textInput->setPreferredLanguage(QStringLiteral("foo"));
    QVERIFY(preferredLanguageChangedSpy.wait());
    QCOMPARE(ti->preferredLanguage(), QStringLiteral("foo").toUtf8());
}

void TextInputTest::testReset_data()
{
    QTest::addColumn<TextInputInterfaceVersion>("version");

    QTest::newRow("UnstableV0") << TextInputInterfaceVersion::UnstableV0;
    QTest::newRow("UnstableV2") << TextInputInterfaceVersion::UnstableV2;
}

void TextInputTest::testReset()
{
    // this test verifies that the reset request is properly passed from client to server
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QFETCH(TextInputInterfaceVersion, version);
    QScopedPointer<TextInput> textInput(createTextInput(version));
    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->focusedTextInput();
    QVERIFY(ti);

    QSignalSpy resetRequestedSpy(ti, &TextInputInterface::requestReset);
    QVERIFY(resetRequestedSpy.isValid());

    textInput->reset();
    QVERIFY(resetRequestedSpy.wait());
}

void TextInputTest::testSurroundingText_data()
{
    QTest::addColumn<TextInputInterfaceVersion>("version");

    QTest::newRow("UnstableV0") << TextInputInterfaceVersion::UnstableV0;
    QTest::newRow("UnstableV2") << TextInputInterfaceVersion::UnstableV2;
}

void TextInputTest::testSurroundingText()
{
    // this test verifies that surrounding text is properly passed around
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QFETCH(TextInputInterfaceVersion, version);
    QScopedPointer<TextInput> textInput(createTextInput(version));
    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->focusedTextInput();
    QVERIFY(ti);
    QVERIFY(ti->surroundingText().isEmpty());
    QCOMPARE(ti->surroundingTextCursorPosition(), 0);
    QCOMPARE(ti->surroundingTextSelectionAnchor(), 0);

    QSignalSpy surroundingTextChangedSpy(ti, &TextInputInterface::surroundingTextChanged);
    QVERIFY(surroundingTextChangedSpy.isValid());

    textInput->setSurroundingText(QStringLiteral("100 €, 100 $"), 5, 6);
    QVERIFY(surroundingTextChangedSpy.wait());
    QCOMPARE(ti->surroundingText(), QStringLiteral("100 €, 100 $").toUtf8());
    QCOMPARE(ti->surroundingTextCursorPosition(), QStringLiteral("100 €, 100 $").toUtf8().indexOf(','));
    QCOMPARE(ti->surroundingTextSelectionAnchor(), QStringLiteral("100 €, 100 $").toUtf8().indexOf(' ', ti->surroundingTextCursorPosition()));
}

void TextInputTest::testContentHints_data()
{
    QTest::addColumn<TextInputInterfaceVersion>("version");
    QTest::addColumn<TextInput::ContentHints>("clientHints");
    QTest::addColumn<TextInputInterface::ContentHints>("serverHints");

    QTest::newRow("completion/v0")     << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentHints(TextInput::ContentHint::AutoCompletion) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::AutoCompletion);
    QTest::newRow("Correction/v0")     << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentHints(TextInput::ContentHint::AutoCorrection) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::AutoCorrection);
    QTest::newRow("Capitalization/v0") << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentHints(TextInput::ContentHint::AutoCapitalization) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::AutoCapitalization);
    QTest::newRow("Lowercase/v0")      << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentHints(TextInput::ContentHint::LowerCase) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::LowerCase);
    QTest::newRow("Uppercase/v0")      << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentHints(TextInput::ContentHint::UpperCase) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::UpperCase);
    QTest::newRow("Titlecase/v0")      << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentHints(TextInput::ContentHint::TitleCase) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::TitleCase);
    QTest::newRow("HiddenText/v0")     << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentHints(TextInput::ContentHint::HiddenText) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::HiddenText);
    QTest::newRow("SensitiveData/v0")  << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentHints(TextInput::ContentHint::SensitiveData) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::SensitiveData);
    QTest::newRow("Latin/v0")          << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentHints(TextInput::ContentHint::Latin) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::Latin);
    QTest::newRow("Multiline/v0")      << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentHints(TextInput::ContentHint::MultiLine) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::MultiLine);

    QTest::newRow("autos/v0") << TextInputInterfaceVersion::UnstableV0
                              << (TextInput::ContentHint::AutoCompletion | TextInput::ContentHint::AutoCorrection | TextInput::ContentHint::AutoCapitalization)
                              << (TextInputInterface::ContentHint::AutoCompletion | TextInputInterface::ContentHint::AutoCorrection | TextInputInterface::ContentHint::AutoCapitalization);

    // all has combinations which don't make sense - what's both lowercase and uppercase?
    QTest::newRow("all/v0") << TextInputInterfaceVersion::UnstableV0
                            << (TextInput::ContentHint::AutoCompletion |
                                TextInput::ContentHint::AutoCorrection |
                                TextInput::ContentHint::AutoCapitalization |
                                TextInput::ContentHint::LowerCase |
                                TextInput::ContentHint::UpperCase |
                                TextInput::ContentHint::TitleCase |
                                TextInput::ContentHint::HiddenText |
                                TextInput::ContentHint::SensitiveData |
                                TextInput::ContentHint::Latin |
                                TextInput::ContentHint::MultiLine)
                            << (TextInputInterface::ContentHint::AutoCompletion |
                                TextInputInterface::ContentHint::AutoCorrection |
                                TextInputInterface::ContentHint::AutoCapitalization |
                                TextInputInterface::ContentHint::LowerCase |
                                TextInputInterface::ContentHint::UpperCase |
                                TextInputInterface::ContentHint::TitleCase |
                                TextInputInterface::ContentHint::HiddenText |
                                TextInputInterface::ContentHint::SensitiveData |
                                TextInputInterface::ContentHint::Latin |
                                TextInputInterface::ContentHint::MultiLine);

    // same for version 2

    QTest::newRow("completion/v2")     << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentHints(TextInput::ContentHint::AutoCompletion) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::AutoCompletion);
    QTest::newRow("Correction/v2")     << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentHints(TextInput::ContentHint::AutoCorrection) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::AutoCorrection);
    QTest::newRow("Capitalization/v2") << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentHints(TextInput::ContentHint::AutoCapitalization) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::AutoCapitalization);
    QTest::newRow("Lowercase/v2")      << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentHints(TextInput::ContentHint::LowerCase) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::LowerCase);
    QTest::newRow("Uppercase/v2")      << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentHints(TextInput::ContentHint::UpperCase) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::UpperCase);
    QTest::newRow("Titlecase/v2")      << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentHints(TextInput::ContentHint::TitleCase) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::TitleCase);
    QTest::newRow("HiddenText/v2")     << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentHints(TextInput::ContentHint::HiddenText) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::HiddenText);
    QTest::newRow("SensitiveData/v2")  << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentHints(TextInput::ContentHint::SensitiveData) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::SensitiveData);
    QTest::newRow("Latin/v2")          << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentHints(TextInput::ContentHint::Latin) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::Latin);
    QTest::newRow("Multiline/v2")      << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentHints(TextInput::ContentHint::MultiLine) << TextInputInterface::ContentHints(TextInputInterface::ContentHint::MultiLine);

    QTest::newRow("autos/v2") << TextInputInterfaceVersion::UnstableV2
                              << (TextInput::ContentHint::AutoCompletion | TextInput::ContentHint::AutoCorrection | TextInput::ContentHint::AutoCapitalization)
                              << (TextInputInterface::ContentHint::AutoCompletion | TextInputInterface::ContentHint::AutoCorrection | TextInputInterface::ContentHint::AutoCapitalization);

    // all has combinations which don't make sense - what's both lowercase and uppercase?
    QTest::newRow("all/v2") << TextInputInterfaceVersion::UnstableV2
                            << (TextInput::ContentHint::AutoCompletion |
                                TextInput::ContentHint::AutoCorrection |
                                TextInput::ContentHint::AutoCapitalization |
                                TextInput::ContentHint::LowerCase |
                                TextInput::ContentHint::UpperCase |
                                TextInput::ContentHint::TitleCase |
                                TextInput::ContentHint::HiddenText |
                                TextInput::ContentHint::SensitiveData |
                                TextInput::ContentHint::Latin |
                                TextInput::ContentHint::MultiLine)
                            << (TextInputInterface::ContentHint::AutoCompletion |
                                TextInputInterface::ContentHint::AutoCorrection |
                                TextInputInterface::ContentHint::AutoCapitalization |
                                TextInputInterface::ContentHint::LowerCase |
                                TextInputInterface::ContentHint::UpperCase |
                                TextInputInterface::ContentHint::TitleCase |
                                TextInputInterface::ContentHint::HiddenText |
                                TextInputInterface::ContentHint::SensitiveData |
                                TextInputInterface::ContentHint::Latin |
                                TextInputInterface::ContentHint::MultiLine);
}

void TextInputTest::testContentHints()
{
    // this test verifies that content hints are properly passed from client to server
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QFETCH(TextInputInterfaceVersion, version);
    QScopedPointer<TextInput> textInput(createTextInput(version));
    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->focusedTextInput();
    QVERIFY(ti);
    QCOMPARE(ti->contentHints(), TextInputInterface::ContentHints());

    QSignalSpy contentTypeChangedSpy(ti, &TextInputInterface::contentTypeChanged);
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
    QCOMPARE(ti->contentHints(), TextInputInterface::ContentHints());
}

void TextInputTest::testContentPurpose_data()
{
    QTest::addColumn<TextInputInterfaceVersion>("version");
    QTest::addColumn<TextInput::ContentPurpose>("clientPurpose");
    QTest::addColumn<TextInputInterface::ContentPurpose>("serverPurpose");

    QTest::newRow("Alpha/v0")    << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentPurpose::Alpha    << TextInputInterface::ContentPurpose::Alpha;
    QTest::newRow("Digits/v0")   << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentPurpose::Digits   << TextInputInterface::ContentPurpose::Digits;
    QTest::newRow("Number/v0")   << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentPurpose::Number   << TextInputInterface::ContentPurpose::Number;
    QTest::newRow("Phone/v0")    << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentPurpose::Phone    << TextInputInterface::ContentPurpose::Phone;
    QTest::newRow("Url/v0")      << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentPurpose::Url      << TextInputInterface::ContentPurpose::Url;
    QTest::newRow("Email/v0")    << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentPurpose::Email    << TextInputInterface::ContentPurpose::Email;
    QTest::newRow("Name/v0")     << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentPurpose::Name     << TextInputInterface::ContentPurpose::Name;
    QTest::newRow("Password/v0") << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentPurpose::Password << TextInputInterface::ContentPurpose::Password;
    QTest::newRow("Date/v0")     << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentPurpose::Date     << TextInputInterface::ContentPurpose::Date;
    QTest::newRow("Time/v0")     << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentPurpose::Time     << TextInputInterface::ContentPurpose::Time;
    QTest::newRow("Datetime/v0") << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentPurpose::DateTime << TextInputInterface::ContentPurpose::DateTime;
    QTest::newRow("Terminal/v0") << TextInputInterfaceVersion::UnstableV0 << TextInput::ContentPurpose::Terminal << TextInputInterface::ContentPurpose::Terminal;

    QTest::newRow("Alpha/v2")    << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentPurpose::Alpha    << TextInputInterface::ContentPurpose::Alpha;
    QTest::newRow("Digits/v2")   << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentPurpose::Digits   << TextInputInterface::ContentPurpose::Digits;
    QTest::newRow("Number/v2")   << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentPurpose::Number   << TextInputInterface::ContentPurpose::Number;
    QTest::newRow("Phone/v2")    << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentPurpose::Phone    << TextInputInterface::ContentPurpose::Phone;
    QTest::newRow("Url/v2")      << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentPurpose::Url      << TextInputInterface::ContentPurpose::Url;
    QTest::newRow("Email/v2")    << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentPurpose::Email    << TextInputInterface::ContentPurpose::Email;
    QTest::newRow("Name/v2")     << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentPurpose::Name     << TextInputInterface::ContentPurpose::Name;
    QTest::newRow("Password/v2") << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentPurpose::Password << TextInputInterface::ContentPurpose::Password;
    QTest::newRow("Date/v2")     << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentPurpose::Date     << TextInputInterface::ContentPurpose::Date;
    QTest::newRow("Time/v2")     << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentPurpose::Time     << TextInputInterface::ContentPurpose::Time;
    QTest::newRow("Datetime/v2") << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentPurpose::DateTime << TextInputInterface::ContentPurpose::DateTime;
    QTest::newRow("Terminal/v2") << TextInputInterfaceVersion::UnstableV2 << TextInput::ContentPurpose::Terminal << TextInputInterface::ContentPurpose::Terminal;
}

void TextInputTest::testContentPurpose()
{
    // this test verifies that content purpose are properly passed from client to server
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QFETCH(TextInputInterfaceVersion, version);
    QScopedPointer<TextInput> textInput(createTextInput(version));
    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->focusedTextInput();
    QVERIFY(ti);
    QCOMPARE(ti->contentPurpose(), TextInputInterface::ContentPurpose::Normal);

    QSignalSpy contentTypeChangedSpy(ti, &TextInputInterface::contentTypeChanged);
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
    QCOMPARE(ti->contentPurpose(), TextInputInterface::ContentPurpose::Normal);
}

void TextInputTest::testTextDirection_data()
{
    QTest::addColumn<TextInputInterfaceVersion>("version");
    QTest::addColumn<Qt::LayoutDirection>("textDirection");

    QTest::newRow("ltr/v0") << TextInputInterfaceVersion::UnstableV0 << Qt::LeftToRight;
    QTest::newRow("rtl/v0") << TextInputInterfaceVersion::UnstableV0 << Qt::RightToLeft;

    QTest::newRow("ltr/v2") << TextInputInterfaceVersion::UnstableV2 << Qt::LeftToRight;
    QTest::newRow("rtl/v2") << TextInputInterfaceVersion::UnstableV2 << Qt::RightToLeft;
}

void TextInputTest::testTextDirection()
{
    // this test verifies that the text direction is sent from server to client
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QFETCH(TextInputInterfaceVersion, version);
    QScopedPointer<TextInput> textInput(createTextInput(version));
    QVERIFY(!textInput.isNull());
    // default should be auto
    QCOMPARE(textInput->textDirection(), Qt::LayoutDirectionAuto);
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->focusedTextInput();
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

void TextInputTest::testLanguage_data()
{
    QTest::addColumn<TextInputInterfaceVersion>("version");

    QTest::newRow("UnstableV0") << TextInputInterfaceVersion::UnstableV0;
    QTest::newRow("UnstableV2") << TextInputInterfaceVersion::UnstableV2;
}

void TextInputTest::testLanguage()
{
    // this test verifies that language is sent from server to client
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QFETCH(TextInputInterfaceVersion, version);
    QScopedPointer<TextInput> textInput(createTextInput(version));
    QVERIFY(!textInput.isNull());
    // default should be empty
    QVERIFY(textInput->language().isEmpty());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->focusedTextInput();
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

void TextInputTest::testKeyEvent_data()
{
    QTest::addColumn<TextInputInterfaceVersion>("version");

    QTest::newRow("UnstableV0") << TextInputInterfaceVersion::UnstableV0;
    QTest::newRow("UnstableV2") << TextInputInterfaceVersion::UnstableV2;
}

void TextInputTest::testKeyEvent()
{
    qRegisterMetaType<Qt::KeyboardModifiers>();
    qRegisterMetaType<TextInput::KeyState>();
    // this test verifies that key events are properly sent to the client
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QFETCH(TextInputInterfaceVersion, version);
    QScopedPointer<TextInput> textInput(createTextInput(version));
    QVERIFY(!textInput.isNull());
    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->focusedTextInput();
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

void TextInputTest::testPreEdit_data()
{
    QTest::addColumn<TextInputInterfaceVersion>("version");

    QTest::newRow("UnstableV0") << TextInputInterfaceVersion::UnstableV0;
    QTest::newRow("UnstableV2") << TextInputInterfaceVersion::UnstableV2;
}

void TextInputTest::testPreEdit()
{
    // this test verifies that pre-edit is correctly passed to the client
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QFETCH(TextInputInterfaceVersion, version);
    QScopedPointer<TextInput> textInput(createTextInput(version));
    QVERIFY(!textInput.isNull());
    // verify default values
    QVERIFY(textInput->composingText().isEmpty());
    QVERIFY(textInput->composingFallbackText().isEmpty());
    QCOMPARE(textInput->composingTextCursorPosition(), 0);

    textInput->enable(surface.data());
    m_connection->flush();
    m_display->dispatchEvents();

    m_seatInterface->setFocusedKeyboardSurface(serverSurface);
    auto ti = m_seatInterface->focusedTextInput();
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

void TextInputTest::testCommit_data()
{
    QTest::addColumn<TextInputInterfaceVersion>("version");

    QTest::newRow("UnstableV0") << TextInputInterfaceVersion::UnstableV0;
    QTest::newRow("UnstableV2") << TextInputInterfaceVersion::UnstableV2;
}

void TextInputTest::testCommit()
{
    // this test verifies that the commit is handled correctly by the client
    QScopedPointer<Surface> surface(m_compositor->createSurface());
    auto serverSurface = waitForSurface();
    QVERIFY(serverSurface);
    QFETCH(TextInputInterfaceVersion, version);
    QScopedPointer<TextInput> textInput(createTextInput(version));
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
    auto ti = m_seatInterface->focusedTextInput();
    QVERIFY(ti);

    // now let's commit
    QSignalSpy committedSpy(textInput.data(), &TextInput::committed);
    QVERIFY(committedSpy.isValid());
    ti->setCursorPosition(3, 4);
    ti->deleteSurroundingText(2, 1);
    ti->commit(QByteArrayLiteral("foo"));

    QVERIFY(committedSpy.wait());
    QCOMPARE(textInput->commitText(), QByteArrayLiteral("foo"));
    QCOMPARE(textInput->cursorPosition(), 3);
    QCOMPARE(textInput->anchorPosition(), 4);
    QCOMPARE(textInput->deleteSurroundingText().beforeLength, 2u);
    QCOMPARE(textInput->deleteSurroundingText().afterLength, 1u);
}

QTEST_GUILESS_MAIN(TextInputTest)
#include "test_text_input.moc"
