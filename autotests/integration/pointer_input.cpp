/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "core/outputbackend.h"
#include "cursor.h"
#include "deleted.h"
#include "effects.h"
#include "options.h"
#include "pointer_input.h"
#include "screenedge.h"
#include "utils/xcursortheme.h"
#include "virtualdesktops.h"
#include "wayland/clientconnection.h"
#include "wayland/seat_interface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
#include <kwineffects.h>

#include <KWayland/Client/buffer.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/region.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

#include <linux/input.h>

namespace KWin
{

static PlatformCursorImage loadReferenceThemeCursor_helper(const KXcursorTheme &theme,
                                                           const QByteArray &name)
{
    const QVector<KXcursorSprite> sprites = theme.shape(name);
    if (sprites.isEmpty()) {
        return PlatformCursorImage();
    }

    return PlatformCursorImage(sprites.constFirst().data(), sprites.constFirst().hotspot());
}

static PlatformCursorImage loadReferenceThemeCursor(const QByteArray &name)
{
    const Cursor *pointerCursor = Cursors::self()->mouse();

    const KXcursorTheme theme(pointerCursor->themeName(), pointerCursor->themeSize(), kwinApp()->devicePixelRatio());
    if (theme.isEmpty()) {
        return PlatformCursorImage();
    }

    PlatformCursorImage platformCursorImage = loadReferenceThemeCursor_helper(theme, name);
    if (!platformCursorImage.isNull()) {
        return platformCursorImage;
    }

    const QVector<QByteArray> alternativeNames = Cursor::cursorAlternativeNames(name);
    for (const QByteArray &alternativeName : alternativeNames) {
        platformCursorImage = loadReferenceThemeCursor_helper(theme, alternativeName);
        if (!platformCursorImage.isNull()) {
            break;
        }
    }

    return platformCursorImage;
}

static PlatformCursorImage loadReferenceThemeCursor(const CursorShape &shape)
{
    return loadReferenceThemeCursor(shape.name());
}

static const QString s_socketName = QStringLiteral("wayland_test_kwin_pointer_input-0");

class PointerInputTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testWarpingUpdatesFocus();
    void testWarpingGeneratesPointerMotion();
    void testWarpingDuringFilter();
    void testWarpingBetweenWindows();
    void testUpdateFocusAfterScreenChange();
    void testUpdateFocusOnDecorationDestroy();
    void testModifierClickUnrestrictedMove_data();
    void testModifierClickUnrestrictedMove();
    void testModifierClickUnrestrictedFullscreenMove();
    void testModifierClickUnrestrictedMoveGlobalShortcutsDisabled();
    void testModifierScrollOpacity_data();
    void testModifierScrollOpacity();
    void testModifierScrollOpacityGlobalShortcutsDisabled();
    void testScrollAction();
    void testFocusFollowsMouse();
    void testMouseActionInactiveWindow_data();
    void testMouseActionInactiveWindow();
    void testMouseActionActiveWindow_data();
    void testMouseActionActiveWindow();
    void testCursorImage();
    void testEffectOverrideCursorImage();
    void testPopup();
    void testDecoCancelsPopup();
    void testWindowUnderCursorWhileButtonPressed();
    void testConfineToScreenGeometry_data();
    void testConfineToScreenGeometry();
    void testResizeCursor_data();
    void testResizeCursor();
    void testMoveCursor();
    void testHideShowCursor();
    void testDefaultInputRegion();
    void testEmptyInputRegion();

private:
    void render(KWayland::Client::Surface *surface, const QSize &size = QSize(100, 50));
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
};

void PointerInputTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::Deleted *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    if (!QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("icons/DMZ-White/index.theme")).isEmpty()) {
        qputenv("XCURSOR_THEME", QByteArrayLiteral("DMZ-White"));
    } else {
        // might be vanilla-dmz (e.g. Arch, FreeBSD)
        qputenv("XCURSOR_THEME", QByteArrayLiteral("Vanilla-DMZ"));
    }
    qputenv("XCURSOR_SIZE", QByteArrayLiteral("24"));
    qputenv("XKB_DEFAULT_RULES", "evdev");
    qputenv("KWIN_COMPOSE", QByteArrayLiteral("O2"));

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
}

void PointerInputTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::Decoration | Test::AdditionalWaylandInterface::XdgDecorationV1));
    QVERIFY(Test::waitForWaylandPointer());
    m_compositor = Test::waylandCompositor();
    m_seat = Test::waylandSeat();

    workspace()->setActiveOutput(QPoint(640, 512));
    Cursors::self()->mouse()->setPos(QPoint(640, 512));
}

void PointerInputTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void PointerInputTest::render(KWayland::Client::Surface *surface, const QSize &size)
{
    Test::render(surface, size, Qt::blue);
    Test::flushWaylandConnection();
}

void PointerInputTest::testWarpingUpdatesFocus()
{
    // this test verifies that warping the pointer creates pointer enter and leave events

    // create pointer and signal spy for enter and leave signals
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &KWayland::Client::Pointer::entered);
    QSignalSpy leftSpy(pointer, &KWayland::Client::Pointer::left);

    // create a window
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface.get(), surface.get());
    QVERIFY(shellSurface);
    render(surface.get());
    QVERIFY(windowAddedSpy.wait());
    Window *window = workspace()->activeWindow();
    QVERIFY(window);

    // currently there should not be a focused pointer surface
    QVERIFY(!waylandServer()->seat()->focusedPointerSurface());
    QVERIFY(!pointer->enteredSurface());

    // enter
    Cursors::self()->mouse()->setPos(QPoint(25, 25));
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(enteredSpy.first().at(1).toPointF(), QPointF(25, 25));
    // window should have focus
    QCOMPARE(pointer->enteredSurface(), surface.get());
    // also on the server
    QCOMPARE(waylandServer()->seat()->focusedPointerSurface(), window->surface());

    // and out again
    Cursors::self()->mouse()->setPos(QPoint(250, 250));
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.count(), 1);
    // there should not be a focused pointer surface anymore
    QVERIFY(!waylandServer()->seat()->focusedPointerSurface());
    QVERIFY(!pointer->enteredSurface());
}

void PointerInputTest::testWarpingGeneratesPointerMotion()
{
    // this test verifies that warping the pointer creates pointer motion events

    // create pointer and signal spy for enter and motion
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &KWayland::Client::Pointer::entered);
    QSignalSpy movedSpy(pointer, &KWayland::Client::Pointer::motion);

    // create a window
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface.get(), surface.get());
    QVERIFY(shellSurface);
    render(surface.get());
    QVERIFY(windowAddedSpy.wait());
    Window *window = workspace()->activeWindow();
    QVERIFY(window);

    // enter
    Test::pointerMotion(QPointF(25, 25), 1);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.first().at(1).toPointF(), QPointF(25, 25));

    // now warp
    Cursors::self()->mouse()->setPos(QPoint(26, 26));
    QVERIFY(movedSpy.wait());
    QCOMPARE(movedSpy.count(), 1);
    QCOMPARE(movedSpy.last().first().toPointF(), QPointF(26, 26));
}

void PointerInputTest::testWarpingDuringFilter()
{
    // this test verifies that pointer motion is handled correctly if
    // the pointer gets warped during processing of input events

    // create pointer
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy movedSpy(pointer, &KWayland::Client::Pointer::motion);

    // warp cursor into expected geometry
    Cursors::self()->mouse()->setPos(10, 10);

    // create a window
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface.get(), surface.get());
    QVERIFY(shellSurface);
    render(surface.get());
    QVERIFY(windowAddedSpy.wait());
    Window *window = workspace()->activeWindow();
    QVERIFY(window);

    QCOMPARE(window->pos(), QPoint(0, 0));
    QVERIFY(window->frameGeometry().contains(Cursors::self()->mouse()->pos()));

    // is WindowView effect for top left screen edge loaded
    QVERIFY(static_cast<EffectsHandlerImpl *>(effects)->isEffectLoaded("windowview"));
    QVERIFY(movedSpy.isEmpty());
    quint32 timestamp = 0;
    Test::pointerMotion(QPoint(0, 0), timestamp++);
    // screen edges push back
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(1, 1));
    QVERIFY(movedSpy.wait());
    QCOMPARE(movedSpy.count(), 2);
    QCOMPARE(movedSpy.at(0).first().toPoint(), QPoint(0, 0));
    QCOMPARE(movedSpy.at(1).first().toPoint(), QPoint(1, 1));
}

void PointerInputTest::testWarpingBetweenWindows()
{
    // This test verifies that the compositor will send correct events when the pointer
    // leaves one window and enters another window.

    std::unique_ptr<KWayland::Client::Pointer> pointer(m_seat->createPointer(m_seat));
    QSignalSpy enteredSpy(pointer.get(), &KWayland::Client::Pointer::entered);
    QSignalSpy leftSpy(pointer.get(), &KWayland::Client::Pointer::left);
    QSignalSpy motionSpy(pointer.get(), &KWayland::Client::Pointer::motion);

    // create windows
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    auto window1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::cyan);
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    auto window2 = Test::renderAndWaitForShown(surface2.get(), QSize(200, 100), Qt::red);

    // place windows side by side
    window1->move(QPoint(0, 0));
    window2->move(QPoint(100, 0));

    quint32 timestamp = 0;

    // put the pointer at the center of the first window
    Test::pointerMotion(window1->frameGeometry().center(), timestamp++);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);
    QCOMPARE(enteredSpy.last().at(1).toPointF(), QPointF(50, 25));
    QCOMPARE(leftSpy.count(), 0);
    QCOMPARE(motionSpy.count(), 0);
    QCOMPARE(pointer->enteredSurface(), surface1.get());

    // put the pointer at the center of the second window
    Test::pointerMotion(window2->frameGeometry().center(), timestamp++);
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(enteredSpy.last().at(1).toPointF(), QPointF(100, 50));
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(motionSpy.count(), 0);
    QCOMPARE(pointer->enteredSurface(), surface2.get());
}

void PointerInputTest::testUpdateFocusAfterScreenChange()
{
    // this test verifies that a pointer enter event is generated when the cursor changes to another
    // screen due to removal of screen

    // create pointer and signal spy for enter and motion
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &KWayland::Client::Pointer::entered);
    QSignalSpy leftSpy(pointer, &KWayland::Client::Pointer::left);

    // create a window
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface.get(), surface.get());
    QVERIFY(shellSurface);
    render(surface.get(), QSize(1280, 1024));
    QVERIFY(windowAddedSpy.wait());
    Window *window = workspace()->activeWindow();
    QVERIFY(window);
    QVERIFY(window->frameGeometry().contains(Cursors::self()->mouse()->pos()));
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 1);

    // move the cursor to the second screen
    Cursors::self()->mouse()->setPos(1500, 300);
    QVERIFY(!window->frameGeometry().contains(Cursors::self()->mouse()->pos()));
    QVERIFY(leftSpy.wait());

    // now let's remove the screen containing the cursor
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs",
                              Qt::DirectConnection,
                              Q_ARG(QVector<QRect>, QVector<QRect>{QRect(0, 0, 1280, 1024)}));
    QCOMPARE(workspace()->outputs().count(), 1);

    // this should have warped the cursor
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(639, 511));
    QVERIFY(window->frameGeometry().contains(Cursors::self()->mouse()->pos()));

    // and we should get an enter event
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
}

void PointerInputTest::testUpdateFocusOnDecorationDestroy()
{
    // This test verifies that a maximized window gets it's pointer focus
    // if decoration was focused and then destroyed on maximize with BorderlessMaximizedWindows option.

    // create pointer for focus tracking
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy buttonStateChangedSpy(pointer, &KWayland::Client::Pointer::buttonStateChanged);

    // Enable the borderless maximized windows option.
    auto group = kwinApp()->config()->group("Windows");
    group.writeEntry("BorderlessMaximizedWindows", true);
    group.sync();
    Workspace::self()->slotReconfigure();
    QCOMPARE(options->borderlessMaximizedWindows(), true);

    // Create the test window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    std::unique_ptr<Test::XdgToplevelDecorationV1> decoration(Test::createXdgToplevelDecorationV1(shellSurface.get()));

    QSignalSpy toplevelConfigureRequestedSpy(shellSurface.get(), &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QSignalSpy decorationConfigureRequestedSpy(decoration.get(), &Test::XdgToplevelDecorationV1::configureRequested);
    decoration->set_mode(Test::XdgToplevelDecorationV1::mode_server_side);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);

    // Wait for the initial configure event.
    Test::XdgToplevel::States states;
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 1);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(0, 0));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Map the window.
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->isActive());
    QCOMPARE(window->maximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeMode::MaximizeRestore);
    QCOMPARE(window->isDecorated(), true);

    // We should receive a configure event when the window becomes active.
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 2);
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(!states.testFlag(Test::XdgToplevel::State::Maximized));

    // Simulate decoration hover
    quint32 timestamp = 0;
    Test::pointerMotion(window->frameGeometry().topLeft(), timestamp++);
    QVERIFY(input()->pointer()->decoration());

    // Maximize when on decoration
    workspace()->slotWindowMaximize();
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    QCOMPARE(surfaceConfigureRequestedSpy.count(), 3);
    QCOMPARE(toplevelConfigureRequestedSpy.last().at(0).toSize(), QSize(1280, 1024));
    states = toplevelConfigureRequestedSpy.last().at(1).value<Test::XdgToplevel::States>();
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Activated));
    QVERIFY(states.testFlag(Test::XdgToplevel::State::Maximized));

    QSignalSpy frameGeometryChangedSpy(window, &Window::frameGeometryChanged);
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Test::render(surface.get(), QSize(1280, 1024), Qt::blue);
    QVERIFY(frameGeometryChangedSpy.wait());
    QCOMPARE(window->frameGeometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(window->maximizeMode(), MaximizeFull);
    QCOMPARE(window->requestedMaximizeMode(), MaximizeFull);
    QCOMPARE(window->isDecorated(), false);

    // Window should have focus, BUG 411884
    QVERIFY(!input()->pointer()->decoration());
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(buttonStateChangedSpy.wait());
    QCOMPARE(pointer->enteredSurface(), surface.get());

    // Destroy the window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void PointerInputTest::testModifierClickUnrestrictedMove_data()
{
    QTest::addColumn<int>("modifierKey");
    QTest::addColumn<int>("mouseButton");
    QTest::addColumn<QString>("modKey");
    QTest::addColumn<bool>("capsLock");

    const QString alt = QStringLiteral("Alt");
    const QString meta = QStringLiteral("Meta");

    QTest::newRow("Left Alt + Left Click") << KEY_LEFTALT << BTN_LEFT << alt << false;
    QTest::newRow("Left Alt + Right Click") << KEY_LEFTALT << BTN_RIGHT << alt << false;
    QTest::newRow("Left Alt + Middle Click") << KEY_LEFTALT << BTN_MIDDLE << alt << false;
    QTest::newRow("Right Alt + Left Click") << KEY_RIGHTALT << BTN_LEFT << alt << false;
    QTest::newRow("Right Alt + Right Click") << KEY_RIGHTALT << BTN_RIGHT << alt << false;
    QTest::newRow("Right Alt + Middle Click") << KEY_RIGHTALT << BTN_MIDDLE << alt << false;
    // now everything with meta
    QTest::newRow("Left Meta + Left Click") << KEY_LEFTMETA << BTN_LEFT << meta << false;
    QTest::newRow("Left Meta + Right Click") << KEY_LEFTMETA << BTN_RIGHT << meta << false;
    QTest::newRow("Left Meta + Middle Click") << KEY_LEFTMETA << BTN_MIDDLE << meta << false;
    QTest::newRow("Right Meta + Left Click") << KEY_RIGHTMETA << BTN_LEFT << meta << false;
    QTest::newRow("Right Meta + Right Click") << KEY_RIGHTMETA << BTN_RIGHT << meta << false;
    QTest::newRow("Right Meta + Middle Click") << KEY_RIGHTMETA << BTN_MIDDLE << meta << false;

    // and with capslock
    QTest::newRow("Left Alt + Left Click/CapsLock") << KEY_LEFTALT << BTN_LEFT << alt << true;
    QTest::newRow("Left Alt + Right Click/CapsLock") << KEY_LEFTALT << BTN_RIGHT << alt << true;
    QTest::newRow("Left Alt + Middle Click/CapsLock") << KEY_LEFTALT << BTN_MIDDLE << alt << true;
    QTest::newRow("Right Alt + Left Click/CapsLock") << KEY_RIGHTALT << BTN_LEFT << alt << true;
    QTest::newRow("Right Alt + Right Click/CapsLock") << KEY_RIGHTALT << BTN_RIGHT << alt << true;
    QTest::newRow("Right Alt + Middle Click/CapsLock") << KEY_RIGHTALT << BTN_MIDDLE << alt << true;
    // now everything with meta
    QTest::newRow("Left Meta + Left Click/CapsLock") << KEY_LEFTMETA << BTN_LEFT << meta << true;
    QTest::newRow("Left Meta + Right Click/CapsLock") << KEY_LEFTMETA << BTN_RIGHT << meta << true;
    QTest::newRow("Left Meta + Middle Click/CapsLock") << KEY_LEFTMETA << BTN_MIDDLE << meta << true;
    QTest::newRow("Right Meta + Left Click/CapsLock") << KEY_RIGHTMETA << BTN_LEFT << meta << true;
    QTest::newRow("Right Meta + Right Click/CapsLock") << KEY_RIGHTMETA << BTN_RIGHT << meta << true;
    QTest::newRow("Right Meta + Middle Click/CapsLock") << KEY_RIGHTMETA << BTN_MIDDLE << meta << true;
}

void PointerInputTest::testModifierClickUnrestrictedMove()
{
    // this test ensures that Alt+mouse button press triggers unrestricted move

    // create pointer and signal spy for button events
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy buttonSpy(pointer, &KWayland::Client::Pointer::buttonStateChanged);

    // first modify the config for this run
    QFETCH(QString, modKey);
    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandAllKey", modKey);
    group.writeEntry("CommandAll1", "Move");
    group.writeEntry("CommandAll2", "Move");
    group.writeEntry("CommandAll3", "Move");
    group.sync();
    workspace()->slotReconfigure();
    QCOMPARE(options->commandAllModifier(), modKey == QStringLiteral("Alt") ? Qt::AltModifier : Qt::MetaModifier);
    QCOMPARE(options->commandAll1(), Options::MouseUnrestrictedMove);
    QCOMPARE(options->commandAll2(), Options::MouseUnrestrictedMove);
    QCOMPARE(options->commandAll3(), Options::MouseUnrestrictedMove);

    // create a window
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface.get(), surface.get());
    QVERIFY(shellSurface);
    render(surface.get());
    QVERIFY(windowAddedSpy.wait());
    Window *window = workspace()->activeWindow();
    QVERIFY(window);

    // move cursor on window
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());

    // simulate modifier+click
    quint32 timestamp = 1;
    QFETCH(bool, capsLock);
    if (capsLock) {
        Test::keyboardKeyPressed(KEY_CAPSLOCK, timestamp++);
    }
    QFETCH(int, modifierKey);
    QFETCH(int, mouseButton);
    Test::keyboardKeyPressed(modifierKey, timestamp++);
    QVERIFY(!window->isInteractiveMove());
    Test::pointerButtonPressed(mouseButton, timestamp++);
    QVERIFY(window->isInteractiveMove());
    // release modifier should not change it
    Test::keyboardKeyReleased(modifierKey, timestamp++);
    QVERIFY(window->isInteractiveMove());
    // but releasing the key should end move/resize
    Test::pointerButtonReleased(mouseButton, timestamp++);
    QVERIFY(!window->isInteractiveMove());
    if (capsLock) {
        Test::keyboardKeyReleased(KEY_CAPSLOCK, timestamp++);
    }

    // all of that should not have triggered button events on the surface
    QCOMPARE(buttonSpy.count(), 0);
    // also waiting shouldn't give us the event
    QVERIFY(!buttonSpy.wait(100));
}

void PointerInputTest::testModifierClickUnrestrictedFullscreenMove()
{
    // this test ensures that Meta+mouse button press triggers unrestricted move for fullscreen windows
    if (workspace()->outputs().size() < 2) {
        QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, QVector<QRect>() << QRect(0, 0, 1280, 1024) << QRect(1280, 0, 1280, 1024)));
    }

    // first modify the config for this run
    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandAllKey", "Meta");
    group.writeEntry("CommandAll1", "Move");
    group.writeEntry("CommandAll2", "Move");
    group.writeEntry("CommandAll3", "Move");
    group.sync();
    workspace()->slotReconfigure();
    QCOMPARE(options->commandAllModifier(), Qt::MetaModifier);
    QCOMPARE(options->commandAll1(), Options::MouseUnrestrictedMove);
    QCOMPARE(options->commandAll2(), Options::MouseUnrestrictedMove);
    QCOMPARE(options->commandAll3(), Options::MouseUnrestrictedMove);

    // create a window
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface.get(), surface.get());
    QVERIFY(shellSurface);
    shellSurface->set_fullscreen(nullptr);
    QSignalSpy toplevelConfigureRequestedSpy(shellSurface, &Test::XdgToplevel::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    Window *window = Test::renderAndWaitForShown(surface.get(), toplevelConfigureRequestedSpy.last().at(0).value<QSize>(), Qt::blue);
    QVERIFY(window);
    QVERIFY(window->isFullScreen());

    // move cursor on window
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());

    // simulate modifier+click
    quint32 timestamp = 1;
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    QVERIFY(!window->isInteractiveMove());
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(window->isInteractiveMove());
    // release modifier should not change it
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    QVERIFY(window->isInteractiveMove());
    // but releasing the key should end move/resize
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(!window->isInteractiveMove());
}

void PointerInputTest::testModifierClickUnrestrictedMoveGlobalShortcutsDisabled()
{
    // this test ensures that Alt+mouse button press triggers unrestricted move

    // create pointer and signal spy for button events
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy buttonSpy(pointer, &KWayland::Client::Pointer::buttonStateChanged);

    // first modify the config for this run
    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandAllKey", "Meta");
    group.writeEntry("CommandAll1", "Move");
    group.writeEntry("CommandAll2", "Move");
    group.writeEntry("CommandAll3", "Move");
    group.sync();
    workspace()->slotReconfigure();
    QCOMPARE(options->commandAllModifier(), Qt::MetaModifier);
    QCOMPARE(options->commandAll1(), Options::MouseUnrestrictedMove);
    QCOMPARE(options->commandAll2(), Options::MouseUnrestrictedMove);
    QCOMPARE(options->commandAll3(), Options::MouseUnrestrictedMove);

    // create a window
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface.get(), surface.get());
    QVERIFY(shellSurface);
    render(surface.get());
    QVERIFY(windowAddedSpy.wait());
    Window *window = workspace()->activeWindow();
    QVERIFY(window);

    // disable global shortcuts
    QVERIFY(!workspace()->globalShortcutsDisabled());
    workspace()->disableGlobalShortcutsForClient(true);
    QVERIFY(workspace()->globalShortcutsDisabled());

    // move cursor on window
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());

    // simulate modifier+click
    quint32 timestamp = 1;
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    QVERIFY(!window->isInteractiveMove());
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(!window->isInteractiveMove());
    // release modifier should not change it
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    QVERIFY(!window->isInteractiveMove());
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);

    workspace()->disableGlobalShortcutsForClient(false);
}

void PointerInputTest::testModifierScrollOpacity_data()
{
    QTest::addColumn<int>("modifierKey");
    QTest::addColumn<QString>("modKey");
    QTest::addColumn<bool>("capsLock");

    const QString alt = QStringLiteral("Alt");
    const QString meta = QStringLiteral("Meta");

    QTest::newRow("Left Alt") << KEY_LEFTALT << alt << false;
    QTest::newRow("Right Alt") << KEY_RIGHTALT << alt << false;
    QTest::newRow("Left Meta") << KEY_LEFTMETA << meta << false;
    QTest::newRow("Right Meta") << KEY_RIGHTMETA << meta << false;
    QTest::newRow("Left Alt/CapsLock") << KEY_LEFTALT << alt << true;
    QTest::newRow("Right Alt/CapsLock") << KEY_RIGHTALT << alt << true;
    QTest::newRow("Left Meta/CapsLock") << KEY_LEFTMETA << meta << true;
    QTest::newRow("Right Meta/CapsLock") << KEY_RIGHTMETA << meta << true;
}

void PointerInputTest::testModifierScrollOpacity()
{
    // this test verifies that mod+wheel performs a window operation and does not
    // pass the wheel to the window

    // create pointer and signal spy for button events
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy axisSpy(pointer, &KWayland::Client::Pointer::axisChanged);

    // first modify the config for this run
    QFETCH(QString, modKey);
    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandAllKey", modKey);
    group.writeEntry("CommandAllWheel", "change opacity");
    group.sync();
    workspace()->slotReconfigure();

    // create a window
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface.get(), surface.get());
    QVERIFY(shellSurface);
    render(surface.get());
    QVERIFY(windowAddedSpy.wait());
    Window *window = workspace()->activeWindow();
    QVERIFY(window);
    // set the opacity to 0.5
    window->setOpacity(0.5);
    QCOMPARE(window->opacity(), 0.5);

    // move cursor on window
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());

    // simulate modifier+wheel
    quint32 timestamp = 1;
    QFETCH(bool, capsLock);
    if (capsLock) {
        Test::keyboardKeyPressed(KEY_CAPSLOCK, timestamp++);
    }
    QFETCH(int, modifierKey);
    Test::keyboardKeyPressed(modifierKey, timestamp++);
    Test::pointerAxisVertical(-5, timestamp++);
    QCOMPARE(window->opacity(), 0.6);
    Test::pointerAxisVertical(5, timestamp++);
    QCOMPARE(window->opacity(), 0.5);
    Test::keyboardKeyReleased(modifierKey, timestamp++);
    if (capsLock) {
        Test::keyboardKeyReleased(KEY_CAPSLOCK, timestamp++);
    }

    // axis should have been filtered out
    QCOMPARE(axisSpy.count(), 0);
    QVERIFY(!axisSpy.wait(100));
}

void PointerInputTest::testModifierScrollOpacityGlobalShortcutsDisabled()
{
    // this test verifies that mod+wheel performs a window operation and does not
    // pass the wheel to the window

    // create pointer and signal spy for button events
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy axisSpy(pointer, &KWayland::Client::Pointer::axisChanged);

    // first modify the config for this run
    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandAllKey", "Meta");
    group.writeEntry("CommandAllWheel", "change opacity");
    group.sync();
    workspace()->slotReconfigure();

    // create a window
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface.get(), surface.get());
    QVERIFY(shellSurface);
    render(surface.get());
    QVERIFY(windowAddedSpy.wait());
    Window *window = workspace()->activeWindow();
    QVERIFY(window);
    // set the opacity to 0.5
    window->setOpacity(0.5);
    QCOMPARE(window->opacity(), 0.5);

    // move cursor on window
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());

    // disable global shortcuts
    QVERIFY(!workspace()->globalShortcutsDisabled());
    workspace()->disableGlobalShortcutsForClient(true);
    QVERIFY(workspace()->globalShortcutsDisabled());

    // simulate modifier+wheel
    quint32 timestamp = 1;
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    Test::pointerAxisVertical(-5, timestamp++);
    QCOMPARE(window->opacity(), 0.5);
    Test::pointerAxisVertical(5, timestamp++);
    QCOMPARE(window->opacity(), 0.5);
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);

    workspace()->disableGlobalShortcutsForClient(false);
}

void PointerInputTest::testScrollAction()
{
    // this test verifies that scroll on inactive window performs a mouse action
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy axisSpy(pointer, &KWayland::Client::Pointer::axisChanged);

    // first modify the config for this run
    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandWindowWheel", "activate and scroll");
    group.sync();
    workspace()->slotReconfigure();
    // create two windows
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface1 = Test::createSurface();
    QVERIFY(surface1);
    Test::XdgToplevel *shellSurface1 = Test::createXdgToplevelSurface(surface1.get(), surface1.get());
    QVERIFY(shellSurface1);
    render(surface1.get());
    QVERIFY(windowAddedSpy.wait());
    Window *window1 = workspace()->activeWindow();
    QVERIFY(window1);
    std::unique_ptr<KWayland::Client::Surface> surface2 = Test::createSurface();
    QVERIFY(surface2);
    Test::XdgToplevel *shellSurface2 = Test::createXdgToplevelSurface(surface2.get(), surface2.get());
    QVERIFY(shellSurface2);
    render(surface2.get());
    QVERIFY(windowAddedSpy.wait());
    Window *window2 = workspace()->activeWindow();
    QVERIFY(window2);
    QVERIFY(window1 != window2);

    // move cursor to the inactive window
    Cursors::self()->mouse()->setPos(window1->frameGeometry().center());

    quint32 timestamp = 1;
    QVERIFY(!window1->isActive());
    Test::pointerAxisVertical(5, timestamp++);
    QVERIFY(window1->isActive());

    // but also the wheel event should be passed to the window
    QVERIFY(axisSpy.wait());
}

void PointerInputTest::testFocusFollowsMouse()
{
    // need to create a pointer, otherwise it doesn't accept focus
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    // move cursor out of the way of first window to be created
    Cursors::self()->mouse()->setPos(900, 900);

    // first modify the config for this run
    KConfigGroup group = kwinApp()->config()->group("Windows");
    group.writeEntry("AutoRaise", true);
    group.writeEntry("AutoRaiseInterval", 20);
    group.writeEntry("DelayFocusInterval", 200);
    group.writeEntry("FocusPolicy", "FocusFollowsMouse");
    group.sync();
    workspace()->slotReconfigure();
    // verify the settings
    QCOMPARE(options->focusPolicy(), Options::FocusFollowsMouse);
    QVERIFY(options->isAutoRaise());
    QCOMPARE(options->autoRaiseInterval(), 20);
    QCOMPARE(options->delayFocusInterval(), 200);

    // create two windows
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface1 = Test::createSurface();
    QVERIFY(surface1);
    Test::XdgToplevel *shellSurface1 = Test::createXdgToplevelSurface(surface1.get(), surface1.get());
    QVERIFY(shellSurface1);
    render(surface1.get(), QSize(800, 800));
    QVERIFY(windowAddedSpy.wait());
    Window *window1 = workspace()->activeWindow();
    QVERIFY(window1);
    std::unique_ptr<KWayland::Client::Surface> surface2 = Test::createSurface();
    QVERIFY(surface2);
    Test::XdgToplevel *shellSurface2 = Test::createXdgToplevelSurface(surface2.get(), surface2.get());
    QVERIFY(shellSurface2);
    render(surface2.get(), QSize(800, 800));
    QVERIFY(windowAddedSpy.wait());
    Window *window2 = workspace()->activeWindow();
    QVERIFY(window2);
    QVERIFY(window1 != window2);
    QCOMPARE(workspace()->topWindowOnDesktop(VirtualDesktopManager::self()->currentDesktop()), window2);
    // geometry of the two windows should be overlapping
    QVERIFY(window1->frameGeometry().intersects(window2->frameGeometry()));

    // signal spies for active window changed and stacking order changed
    QSignalSpy activeWindowChangedSpy(workspace(), &Workspace::windowActivated);
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);

    QVERIFY(!window1->isActive());
    QVERIFY(window2->isActive());

    // move on top of first window
    QVERIFY(window1->frameGeometry().contains(10, 10));
    QVERIFY(!window2->frameGeometry().contains(10, 10));
    Cursors::self()->mouse()->setPos(10, 10);
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(stackingOrderChangedSpy.count(), 1);
    QCOMPARE(workspace()->topWindowOnDesktop(VirtualDesktopManager::self()->currentDesktop()), window1);
    QTRY_VERIFY(window1->isActive());

    // move on second window, but move away before active window change delay hits
    Cursors::self()->mouse()->setPos(810, 810);
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(stackingOrderChangedSpy.count(), 2);
    QCOMPARE(workspace()->topWindowOnDesktop(VirtualDesktopManager::self()->currentDesktop()), window2);
    Cursors::self()->mouse()->setPos(10, 10);
    QVERIFY(!activeWindowChangedSpy.wait(250));
    QVERIFY(window1->isActive());
    QCOMPARE(workspace()->topWindowOnDesktop(VirtualDesktopManager::self()->currentDesktop()), window1);
    // as we moved back on window 1 that should been raised in the mean time
    QCOMPARE(stackingOrderChangedSpy.count(), 3);

    // quickly move on window 2 and back on window 1 should not raise window 2
    Cursors::self()->mouse()->setPos(810, 810);
    Cursors::self()->mouse()->setPos(10, 10);
    QVERIFY(!stackingOrderChangedSpy.wait(250));
}

void PointerInputTest::testMouseActionInactiveWindow_data()
{
    QTest::addColumn<quint32>("button");

    QTest::newRow("Left") << quint32(BTN_LEFT);
    QTest::newRow("Middle") << quint32(BTN_MIDDLE);
    QTest::newRow("Right") << quint32(BTN_RIGHT);
}

void PointerInputTest::testMouseActionInactiveWindow()
{
    // this test performs the mouse button window action on an inactive window
    // it should activate the window and raise it

    // first modify the config for this run - disable FocusFollowsMouse
    KConfigGroup group = kwinApp()->config()->group("Windows");
    group.writeEntry("FocusPolicy", "ClickToFocus");
    group.sync();
    group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandWindow1", "Activate, raise and pass click");
    group.writeEntry("CommandWindow2", "Activate, raise and pass click");
    group.writeEntry("CommandWindow3", "Activate, raise and pass click");
    group.sync();
    workspace()->slotReconfigure();

    // create two windows
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface1 = Test::createSurface();
    QVERIFY(surface1);
    Test::XdgToplevel *shellSurface1 = Test::createXdgToplevelSurface(surface1.get(), surface1.get());
    QVERIFY(shellSurface1);
    render(surface1.get(), QSize(800, 800));
    QVERIFY(windowAddedSpy.wait());
    Window *window1 = workspace()->activeWindow();
    QVERIFY(window1);
    std::unique_ptr<KWayland::Client::Surface> surface2 = Test::createSurface();
    QVERIFY(surface2);
    Test::XdgToplevel *shellSurface2 = Test::createXdgToplevelSurface(surface2.get(), surface2.get());
    QVERIFY(shellSurface2);
    render(surface2.get(), QSize(800, 800));
    QVERIFY(windowAddedSpy.wait());
    Window *window2 = workspace()->activeWindow();
    QVERIFY(window2);
    QVERIFY(window1 != window2);
    QCOMPARE(workspace()->topWindowOnDesktop(VirtualDesktopManager::self()->currentDesktop()), window2);
    // geometry of the two windows should be overlapping
    QVERIFY(window1->frameGeometry().intersects(window2->frameGeometry()));

    // signal spies for active window changed and stacking order changed
    QSignalSpy activeWindowChangedSpy(workspace(), &Workspace::windowActivated);
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);

    QVERIFY(!window1->isActive());
    QVERIFY(window2->isActive());

    // move on top of first window
    QVERIFY(window1->frameGeometry().contains(10, 10));
    QVERIFY(!window2->frameGeometry().contains(10, 10));
    Cursors::self()->mouse()->setPos(10, 10);
    // no focus follows mouse
    QVERIFY(!stackingOrderChangedSpy.wait(200));
    QVERIFY(stackingOrderChangedSpy.isEmpty());
    QVERIFY(activeWindowChangedSpy.isEmpty());
    QVERIFY(window2->isActive());
    // and click
    quint32 timestamp = 1;
    QFETCH(quint32, button);
    Test::pointerButtonPressed(button, timestamp++);
    // should raise window1 and activate it
    QCOMPARE(stackingOrderChangedSpy.count(), 1);
    QVERIFY(!activeWindowChangedSpy.isEmpty());
    QCOMPARE(workspace()->topWindowOnDesktop(VirtualDesktopManager::self()->currentDesktop()), window1);
    QVERIFY(window1->isActive());
    QVERIFY(!window2->isActive());

    // release again
    Test::pointerButtonReleased(button, timestamp++);
}

void PointerInputTest::testMouseActionActiveWindow_data()
{
    QTest::addColumn<bool>("clickRaise");
    QTest::addColumn<quint32>("button");

    for (quint32 i = BTN_LEFT; i < BTN_JOYSTICK; i++) {
        QByteArray number = QByteArray::number(i, 16);
        QTest::newRow(QByteArrayLiteral("click raise/").append(number).constData()) << true << i;
        QTest::newRow(QByteArrayLiteral("no click raise/").append(number).constData()) << false << i;
    }
}

void PointerInputTest::testMouseActionActiveWindow()
{
    // this test verifies the mouse action performed on an active window
    // for all buttons it should trigger a window raise depending on the
    // click raise option

    // create a button spy - all clicks should be passed through
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy buttonSpy(pointer, &KWayland::Client::Pointer::buttonStateChanged);

    // adjust config for this run
    QFETCH(bool, clickRaise);
    KConfigGroup group = kwinApp()->config()->group("Windows");
    group.writeEntry("ClickRaise", clickRaise);
    group.sync();
    workspace()->slotReconfigure();
    QCOMPARE(options->isClickRaise(), clickRaise);

    // create two windows
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface1 = Test::createSurface();
    QVERIFY(surface1);
    Test::XdgToplevel *shellSurface1 = Test::createXdgToplevelSurface(surface1.get(), surface1.get());
    QVERIFY(shellSurface1);
    render(surface1.get(), QSize(800, 800));
    QVERIFY(windowAddedSpy.wait());
    Window *window1 = workspace()->activeWindow();
    QVERIFY(window1);
    QSignalSpy window1DestroyedSpy(window1, &QObject::destroyed);
    std::unique_ptr<KWayland::Client::Surface> surface2 = Test::createSurface();
    QVERIFY(surface2);
    Test::XdgToplevel *shellSurface2 = Test::createXdgToplevelSurface(surface2.get(), surface2.get());
    QVERIFY(shellSurface2);
    render(surface2.get(), QSize(800, 800));
    QVERIFY(windowAddedSpy.wait());
    Window *window2 = workspace()->activeWindow();
    QVERIFY(window2);
    QVERIFY(window1 != window2);
    QSignalSpy window2DestroyedSpy(window2, &QObject::destroyed);
    QCOMPARE(workspace()->topWindowOnDesktop(VirtualDesktopManager::self()->currentDesktop()), window2);
    // geometry of the two windows should be overlapping
    QVERIFY(window1->frameGeometry().intersects(window2->frameGeometry()));
    // lower the currently active window
    workspace()->lowerWindow(window2);
    QCOMPARE(workspace()->topWindowOnDesktop(VirtualDesktopManager::self()->currentDesktop()), window1);

    // signal spy for stacking order spy
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);

    // move on top of second window
    QVERIFY(!window1->frameGeometry().contains(900, 900));
    QVERIFY(window2->frameGeometry().contains(900, 900));
    Cursors::self()->mouse()->setPos(900, 900);

    // and click
    quint32 timestamp = 1;
    QFETCH(quint32, button);
    Test::pointerButtonPressed(button, timestamp++);
    QVERIFY(buttonSpy.wait());
    if (clickRaise) {
        QCOMPARE(stackingOrderChangedSpy.count(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(workspace()->topWindowOnDesktop(VirtualDesktopManager::self()->currentDesktop()), window2, 200);
    } else {
        QCOMPARE(stackingOrderChangedSpy.count(), 0);
        QVERIFY(!stackingOrderChangedSpy.wait(100));
        QCOMPARE(workspace()->topWindowOnDesktop(VirtualDesktopManager::self()->currentDesktop()), window1);
    }

    // release again
    Test::pointerButtonReleased(button, timestamp++);

    surface1.reset();
    QVERIFY(window1DestroyedSpy.wait());
    surface2.reset();
    QVERIFY(window2DestroyedSpy.wait());
}

void PointerInputTest::testCursorImage()
{
    // this test verifies that the pointer image gets updated correctly from the client provided data

    // we need a pointer to get the enter event
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &KWayland::Client::Pointer::entered);

    // move cursor somewhere the new window won't open
    auto cursor = Cursors::self()->mouse();
    cursor->setPos(800, 800);
    auto p = input()->pointer();
    // at the moment it should be the fallback cursor
    const QImage fallbackCursor = cursor->image();
    QVERIFY(!fallbackCursor.isNull());

    // create a window
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface.get(), surface.get());
    QVERIFY(shellSurface);
    render(surface.get());
    QVERIFY(windowAddedSpy.wait());
    Window *window = workspace()->activeWindow();
    QVERIFY(window);

    // move cursor to center of window, this should first set a null pointer, so we still show old cursor
    cursor->setPos(window->frameGeometry().center());
    QCOMPARE(p->focus(), window);
    QCOMPARE(cursor->image(), fallbackCursor);
    QVERIFY(enteredSpy.wait());

    // create a cursor on the pointer
    auto cursorSurface = Test::createSurface();
    QVERIFY(cursorSurface);
    QSignalSpy cursorRenderedSpy(cursorSurface.get(), &KWayland::Client::Surface::frameRendered);
    QImage red = QImage(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    red.fill(Qt::red);
    cursorSurface->attachBuffer(Test::waylandShmPool()->createBuffer(red));
    cursorSurface->damage(QRect(0, 0, 10, 10));
    cursorSurface->commit();
    pointer->setCursor(cursorSurface.get(), QPoint(5, 5));
    QVERIFY(cursorRenderedSpy.wait());
    QCOMPARE(cursor->image(), red);
    QCOMPARE(cursor->hotspot(), QPoint(5, 5));
    // change hotspot
    pointer->setCursor(cursorSurface.get(), QPoint(6, 6));
    Test::flushWaylandConnection();
    QTRY_COMPARE(cursor->hotspot(), QPoint(6, 6));
    QCOMPARE(cursor->image(), red);

    // change the buffer
    QImage blue = QImage(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    blue.fill(Qt::blue);
    auto b = Test::waylandShmPool()->createBuffer(blue);
    cursorSurface->attachBuffer(b);
    cursorSurface->damage(QRect(0, 0, 10, 10));
    cursorSurface->commit();
    QVERIFY(cursorRenderedSpy.wait());
    QTRY_COMPARE(cursor->image(), blue);
    QCOMPARE(cursor->hotspot(), QPoint(6, 6));

    // scaled cursor
    QImage blueScaled = QImage(QSize(20, 20), QImage::Format_ARGB32_Premultiplied);
    blueScaled.setDevicePixelRatio(2);
    blueScaled.fill(Qt::blue);
    auto bs = Test::waylandShmPool()->createBuffer(blueScaled);
    cursorSurface->attachBuffer(bs);
    cursorSurface->setScale(2);
    cursorSurface->damage(QRect(0, 0, 20, 20));
    cursorSurface->commit();
    QVERIFY(cursorRenderedSpy.wait());
    QTRY_COMPARE(cursor->image(), blueScaled);
    QCOMPARE(cursor->hotspot(), QPoint(6, 6)); // surface-local (so not changed)

    // hide the cursor
    pointer->setCursor(nullptr);
    Test::flushWaylandConnection();
    QTRY_VERIFY(cursor->image().isNull());

    // move cursor somewhere else, should reset to fallback cursor
    Cursors::self()->mouse()->setPos(window->frameGeometry().bottomLeft() + QPoint(20, 20));
    QVERIFY(!p->focus());
    QVERIFY(!cursor->image().isNull());
    QCOMPARE(cursor->image(), fallbackCursor);
}

class HelperEffect : public Effect
{
    Q_OBJECT
public:
    HelperEffect()
    {
    }
    ~HelperEffect() override
    {
    }
};

void PointerInputTest::testEffectOverrideCursorImage()
{
    // this test verifies the effect cursor override handling

    // we need a pointer to get the enter event and set a cursor
    auto pointer = m_seat->createPointer(m_seat);
    auto cursor = Cursors::self()->mouse();
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &KWayland::Client::Pointer::entered);
    QSignalSpy leftSpy(pointer, &KWayland::Client::Pointer::left);
    // move cursor somewhere the new window won't open
    cursor->setPos(800, 800);
    // here we should have the fallback cursor
    const QImage fallback = cursor->image();
    QVERIFY(!fallback.isNull());

    // now let's create a window
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface.get(), surface.get());
    QVERIFY(shellSurface);
    render(surface.get());
    QVERIFY(windowAddedSpy.wait());
    Window *window = workspace()->activeWindow();
    QVERIFY(window);

    // and move cursor to the window
    QVERIFY(!window->frameGeometry().contains(QPoint(800, 800)));
    cursor->setPos(window->frameGeometry().center());
    QVERIFY(enteredSpy.wait());
    // cursor image should still be fallback
    QCOMPARE(cursor->image(), fallback);

    // now create an effect and set an override cursor
    std::unique_ptr<HelperEffect> effect(new HelperEffect);
    effects->startMouseInterception(effect.get(), Qt::SizeAllCursor);
    const QImage sizeAll = cursor->image();
    QVERIFY(!sizeAll.isNull());
    QVERIFY(sizeAll != fallback);
    QVERIFY(leftSpy.wait());

    // let's change to arrow cursor, this should be our fallback
    effects->defineCursor(Qt::ArrowCursor);
    QCOMPARE(cursor->image(), fallback);

    // back to size all
    effects->defineCursor(Qt::SizeAllCursor);
    QCOMPARE(cursor->image(), sizeAll);

    // move cursor outside the window area
    Cursors::self()->mouse()->setPos(800, 800);
    // and end the override, which should switch to fallback
    effects->stopMouseInterception(effect.get());
    QCOMPARE(cursor->image(), fallback);

    // start mouse interception again
    effects->startMouseInterception(effect.get(), Qt::SizeAllCursor);
    QCOMPARE(cursor->image(), sizeAll);

    // move cursor to area of window
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());
    // this should not result in an enter event
    QVERIFY(!enteredSpy.wait(100));

    // after ending the interception we should get an enter event
    effects->stopMouseInterception(effect.get());
    QVERIFY(enteredSpy.wait());
    QVERIFY(cursor->image().isNull());
}

void PointerInputTest::testPopup()
{
    // this test validates the basic popup behavior
    // a button press outside the window should dismiss the popup

    // first create a parent surface
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &KWayland::Client::Pointer::entered);
    QSignalSpy leftSpy(pointer, &KWayland::Client::Pointer::left);
    QSignalSpy buttonStateChangedSpy(pointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy motionSpy(pointer, &KWayland::Client::Pointer::motion);

    Cursors::self()->mouse()->setPos(800, 800);

    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface.get(), surface.get());
    QVERIFY(shellSurface);
    render(surface.get());
    QVERIFY(windowAddedSpy.wait());
    Window *window = workspace()->activeWindow();
    QVERIFY(window);
    QCOMPARE(window->hasPopupGrab(), false);
    // move pointer into window
    QVERIFY(!window->frameGeometry().contains(QPoint(800, 800)));
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());
    QVERIFY(enteredSpy.wait());
    // click inside window to create serial
    quint32 timestamp = 0;
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(buttonStateChangedSpy.wait());

    // now create the popup surface
    std::unique_ptr<Test::XdgPositioner> positioner(Test::createXdgPositioner());
    positioner->set_size(100, 50);
    positioner->set_anchor_rect(0, 0, 80, 20);
    positioner->set_anchor(Test::XdgPositioner::anchor_bottom_right);
    positioner->set_gravity(Test::XdgPositioner::gravity_bottom_right);
    std::unique_ptr<KWayland::Client::Surface> popupSurface = Test::createSurface();
    QVERIFY(popupSurface);
    Test::XdgPopup *popupShellSurface = Test::createXdgPopupSurface(popupSurface.get(), shellSurface->xdgSurface(), positioner.get());
    QVERIFY(popupShellSurface);
    QSignalSpy doneReceivedSpy(popupShellSurface, &Test::XdgPopup::doneReceived);
    popupShellSurface->grab(*Test::waylandSeat(), 0); // FIXME: Serial.
    render(popupSurface.get(), QSize(100, 50));
    QVERIFY(windowAddedSpy.wait());
    auto popupWindow = windowAddedSpy.last().first().value<Window *>();
    QVERIFY(popupWindow);
    QVERIFY(popupWindow != window);
    QCOMPARE(window, workspace()->activeWindow());
    QCOMPARE(popupWindow->transientFor(), window);
    QCOMPARE(popupWindow->pos(), window->pos() + QPoint(80, 20));
    QCOMPARE(popupWindow->hasPopupGrab(), true);

    // let's move the pointer into the center of the window
    Cursors::self()->mouse()->setPos(popupWindow->frameGeometry().center());
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(pointer->enteredSurface(), popupSurface.get());

    // let's move the pointer outside of the popup window
    // this should not really change anything, it gets a leave event
    Cursors::self()->mouse()->setPos(popupWindow->frameGeometry().bottomRight() + QPoint(2, 2));
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.count(), 2);
    QVERIFY(doneReceivedSpy.isEmpty());
    // now click, should trigger popupDone
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(doneReceivedSpy.wait());
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
}

void PointerInputTest::testDecoCancelsPopup()
{
    // this test verifies that clicking the window decoration of parent window
    // cancels the popup

    // first create a parent surface
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &KWayland::Client::Pointer::entered);
    QSignalSpy leftSpy(pointer, &KWayland::Client::Pointer::left);
    QSignalSpy buttonStateChangedSpy(pointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy motionSpy(pointer, &KWayland::Client::Pointer::motion);

    Cursors::self()->mouse()->setPos(800, 800);

    // create a decorated window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly));
    std::unique_ptr<Test::XdgToplevelDecorationV1> decoration(Test::createXdgToplevelDecorationV1(shellSurface.get()));
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);
    decoration->set_mode(Test::XdgToplevelDecorationV1::mode_server_side);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(surfaceConfigureRequestedSpy.wait());
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);
    QCOMPARE(window->hasPopupGrab(), false);
    QVERIFY(window->isDecorated());

    // move pointer into window
    QVERIFY(!window->frameGeometry().contains(QPoint(800, 800)));
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());
    QVERIFY(enteredSpy.wait());
    // click inside window to create serial
    quint32 timestamp = 0;
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(buttonStateChangedSpy.wait());

    // now create the popup surface
    std::unique_ptr<Test::XdgPositioner> positioner(Test::createXdgPositioner());
    positioner->set_size(100, 50);
    positioner->set_anchor_rect(0, 0, 80, 20);
    positioner->set_anchor(Test::XdgPositioner::anchor_bottom_right);
    positioner->set_gravity(Test::XdgPositioner::gravity_bottom_right);
    std::unique_ptr<KWayland::Client::Surface> popupSurface = Test::createSurface();
    QVERIFY(popupSurface);
    Test::XdgPopup *popupShellSurface = Test::createXdgPopupSurface(popupSurface.get(), shellSurface->xdgSurface(), positioner.get());
    QVERIFY(popupShellSurface);
    QSignalSpy doneReceivedSpy(popupShellSurface, &Test::XdgPopup::doneReceived);
    popupShellSurface->grab(*Test::waylandSeat(), 0); // FIXME: Serial.
    auto popupWindow = Test::renderAndWaitForShown(popupSurface.get(), QSize(100, 50), Qt::red);
    QVERIFY(popupWindow);
    QVERIFY(popupWindow != window);
    QCOMPARE(window, workspace()->activeWindow());
    QCOMPARE(popupWindow->transientFor(), window);
    QCOMPARE(popupWindow->pos(), window->mapFromLocal(QPoint(80, 20)));
    QCOMPARE(popupWindow->hasPopupGrab(), true);

    // let's move the pointer into the center of the deco
    Cursors::self()->mouse()->setPos(window->frameGeometry().center().x(), window->y() + (window->height() - window->clientSize().height()) / 2);

    Test::pointerButtonPressed(BTN_RIGHT, timestamp++);
    QVERIFY(doneReceivedSpy.wait());
    Test::pointerButtonReleased(BTN_RIGHT, timestamp++);
}

void PointerInputTest::testWindowUnderCursorWhileButtonPressed()
{
    // this test verifies that opening a window underneath the mouse cursor does not
    // trigger a leave event if a button is pressed
    // see BUG: 372876

    // first create a parent surface
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &KWayland::Client::Pointer::entered);
    QSignalSpy leftSpy(pointer, &KWayland::Client::Pointer::left);

    Cursors::self()->mouse()->setPos(800, 800);
    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    std::unique_ptr<KWayland::Client::Surface> surface = Test::createSurface();
    QVERIFY(surface);
    Test::XdgToplevel *shellSurface = Test::createXdgToplevelSurface(surface.get(), surface.get());
    QVERIFY(shellSurface);
    render(surface.get());
    QVERIFY(windowAddedSpy.wait());
    Window *window = workspace()->activeWindow();
    QVERIFY(window);

    // move cursor over window
    QVERIFY(!window->frameGeometry().contains(QPoint(800, 800)));
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());
    QVERIFY(enteredSpy.wait());
    // click inside window
    quint32 timestamp = 0;
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);

    // now create a second window as transient
    std::unique_ptr<Test::XdgPositioner> positioner(Test::createXdgPositioner());
    positioner->set_size(99, 49);
    positioner->set_anchor_rect(0, 0, 1, 1);
    positioner->set_anchor(Test::XdgPositioner::anchor_bottom_right);
    positioner->set_gravity(Test::XdgPositioner::gravity_bottom_right);
    std::unique_ptr<KWayland::Client::Surface> popupSurface = Test::createSurface();
    QVERIFY(popupSurface);
    Test::XdgPopup *popupShellSurface = Test::createXdgPopupSurface(popupSurface.get(), shellSurface->xdgSurface(), positioner.get());
    QVERIFY(popupShellSurface);
    render(popupSurface.get(), QSize(99, 49));
    QVERIFY(windowAddedSpy.wait());
    auto popupWindow = windowAddedSpy.last().first().value<Window *>();
    QVERIFY(popupWindow);
    QVERIFY(popupWindow != window);
    QVERIFY(window->frameGeometry().contains(Cursors::self()->mouse()->pos()));
    QVERIFY(popupWindow->frameGeometry().contains(Cursors::self()->mouse()->pos()));
    QVERIFY(!leftSpy.wait());

    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    // now that the button is no longer pressed we should get the leave event
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(enteredSpy.count(), 2);
}

void PointerInputTest::testConfineToScreenGeometry_data()
{
    QTest::addColumn<QPoint>("startPos");
    QTest::addColumn<QPoint>("targetPos");
    QTest::addColumn<QPoint>("expectedPos");

    // screen layout:
    //
    // +----------+----------+---------+
    // |   left   |    top   |  right  |
    // +----------+----------+---------+
    //            |  bottom  |
    //            +----------+
    //

    QTest::newRow("move top-left - left screen") << QPoint(640, 512) << QPoint(-100, -100) << QPoint(0, 0);
    QTest::newRow("move top - left screen") << QPoint(640, 512) << QPoint(640, -100) << QPoint(640, 0);
    QTest::newRow("move top-right - left screen") << QPoint(640, 512) << QPoint(1380, -100) << QPoint(1380, 0);
    QTest::newRow("move right - left screen") << QPoint(640, 512) << QPoint(1380, 512) << QPoint(1380, 512);
    QTest::newRow("move bottom-right - left screen") << QPoint(640, 512) << QPoint(1380, 1124) << QPoint(1380, 1124);
    QTest::newRow("move bottom - left screen") << QPoint(640, 512) << QPoint(640, 1124) << QPoint(640, 1023);
    QTest::newRow("move bottom-left - left screen") << QPoint(640, 512) << QPoint(-100, 1124) << QPoint(0, 1023);
    QTest::newRow("move left - left screen") << QPoint(640, 512) << QPoint(-100, 512) << QPoint(0, 512);

    QTest::newRow("move top-left - top screen") << QPoint(1920, 512) << QPoint(1180, -100) << QPoint(1180, 0);
    QTest::newRow("move top - top screen") << QPoint(1920, 512) << QPoint(1920, -100) << QPoint(1920, 0);
    QTest::newRow("move top-right - top screen") << QPoint(1920, 512) << QPoint(2660, -100) << QPoint(2660, 0);
    QTest::newRow("move right - top screen") << QPoint(1920, 512) << QPoint(2660, 512) << QPoint(2660, 512);
    QTest::newRow("move bottom-right - top screen") << QPoint(1920, 512) << QPoint(2660, 1124) << QPoint(2660, 1023);
    QTest::newRow("move bottom - top screen") << QPoint(1920, 512) << QPoint(1920, 1124) << QPoint(1920, 1124);
    QTest::newRow("move bottom-left - top screen") << QPoint(1920, 512) << QPoint(1180, 1124) << QPoint(1280, 1124);
    QTest::newRow("move left - top screen") << QPoint(1920, 512) << QPoint(1180, 512) << QPoint(1180, 512);

    QTest::newRow("move top-left - right screen") << QPoint(3200, 512) << QPoint(2460, -100) << QPoint(2460, 0);
    QTest::newRow("move top - right screen") << QPoint(3200, 512) << QPoint(3200, -100) << QPoint(3200, 0);
    QTest::newRow("move top-right - right screen") << QPoint(3200, 512) << QPoint(3940, -100) << QPoint(3839, 0);
    QTest::newRow("move right - right screen") << QPoint(3200, 512) << QPoint(3940, 512) << QPoint(3839, 512);
    QTest::newRow("move bottom-right - right screen") << QPoint(3200, 512) << QPoint(3940, 1124) << QPoint(3839, 1023);
    QTest::newRow("move bottom - right screen") << QPoint(3200, 512) << QPoint(3200, 1124) << QPoint(3200, 1023);
    QTest::newRow("move bottom-left - right screen") << QPoint(3200, 512) << QPoint(2460, 1124) << QPoint(2460, 1124);
    QTest::newRow("move left - right screen") << QPoint(3200, 512) << QPoint(2460, 512) << QPoint(2460, 512);

    QTest::newRow("move top-left - bottom screen") << QPoint(1920, 1536) << QPoint(1180, 924) << QPoint(1180, 924);
    QTest::newRow("move top - bottom screen") << QPoint(1920, 1536) << QPoint(1920, 924) << QPoint(1920, 924);
    QTest::newRow("move top-right - bottom screen") << QPoint(1920, 1536) << QPoint(2660, 924) << QPoint(2660, 924);
    QTest::newRow("move right - bottom screen") << QPoint(1920, 1536) << QPoint(2660, 1536) << QPoint(2559, 1536);
    QTest::newRow("move bottom-right - bottom screen") << QPoint(1920, 1536) << QPoint(2660, 2148) << QPoint(2559, 2047);
    QTest::newRow("move bottom - bottom screen") << QPoint(1920, 1536) << QPoint(1920, 2148) << QPoint(1920, 2047);
    QTest::newRow("move bottom-left - bottom screen") << QPoint(1920, 1536) << QPoint(1180, 2148) << QPoint(1280, 2047);
    QTest::newRow("move left - bottom screen") << QPoint(1920, 1536) << QPoint(1180, 1536) << QPoint(1280, 1536);
}

void PointerInputTest::testConfineToScreenGeometry()
{
    // this test verifies that pointer belongs to at least one screen
    // after moving it to off-screen area

    // unload the Window View effect because it pushes back
    // pointer if it's at (0, 0)
    static_cast<EffectsHandlerImpl *>(effects)->unloadEffect(QStringLiteral("windowview"));

    // setup screen layout
    const QVector<QRect> geometries{
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
        QRect(2560, 0, 1280, 1024),
        QRect(1280, 1024, 1280, 1024)};
    QMetaObject::invokeMethod(kwinApp()->outputBackend(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(QVector<QRect>, geometries));

    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), geometries.count());
    QCOMPARE(outputs[0]->geometry(), geometries.at(0));
    QCOMPARE(outputs[1]->geometry(), geometries.at(1));
    QCOMPARE(outputs[2]->geometry(), geometries.at(2));
    QCOMPARE(outputs[3]->geometry(), geometries.at(3));

    // move pointer to initial position
    QFETCH(QPoint, startPos);
    Cursors::self()->mouse()->setPos(startPos);
    QCOMPARE(Cursors::self()->mouse()->pos(), startPos);

    // perform movement
    QFETCH(QPoint, targetPos);
    Test::pointerMotion(targetPos, 1);

    QFETCH(QPoint, expectedPos);
    QCOMPARE(Cursors::self()->mouse()->pos(), expectedPos);
}

void PointerInputTest::testResizeCursor_data()
{
    QTest::addColumn<Qt::Edges>("edges");
    QTest::addColumn<KWin::CursorShape>("cursorShape");

    QTest::newRow("top-left") << Qt::Edges(Qt::TopEdge | Qt::LeftEdge) << CursorShape(ExtendedCursor::SizeNorthWest);
    QTest::newRow("top") << Qt::Edges(Qt::TopEdge) << CursorShape(ExtendedCursor::SizeNorth);
    QTest::newRow("top-right") << Qt::Edges(Qt::TopEdge | Qt::RightEdge) << CursorShape(ExtendedCursor::SizeNorthEast);
    QTest::newRow("right") << Qt::Edges(Qt::RightEdge) << CursorShape(ExtendedCursor::SizeEast);
    QTest::newRow("bottom-right") << Qt::Edges(Qt::BottomEdge | Qt::RightEdge) << CursorShape(ExtendedCursor::SizeSouthEast);
    QTest::newRow("bottom") << Qt::Edges(Qt::BottomEdge) << CursorShape(ExtendedCursor::SizeSouth);
    QTest::newRow("bottom-left") << Qt::Edges(Qt::BottomEdge | Qt::LeftEdge) << CursorShape(ExtendedCursor::SizeSouthWest);
    QTest::newRow("left") << Qt::Edges(Qt::LeftEdge) << CursorShape(ExtendedCursor::SizeWest);
}

void PointerInputTest::testResizeCursor()
{
    // this test verifies that the cursor has correct shape during resize operation

    // first modify the config for this run
    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandAllKey", "Meta");
    group.writeEntry("CommandAll3", "Resize");
    group.sync();
    workspace()->slotReconfigure();
    QCOMPARE(options->commandAllModifier(), Qt::MetaModifier);
    QCOMPARE(options->commandAll3(), Options::MouseUnrestrictedResize);

    // load the fallback cursor (arrow cursor)
    const PlatformCursorImage arrowCursor = loadReferenceThemeCursor(Qt::ArrowCursor);
    QVERIFY(!arrowCursor.isNull());
    QCOMPARE(kwinApp()->cursorImage().image(), arrowCursor.image());
    QCOMPARE(kwinApp()->cursorImage().hotSpot(), arrowCursor.hotSpot());

    // we need a pointer to get the enter event
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &KWayland::Client::Pointer::entered);

    // create a test window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // move the cursor to the test position
    QPoint cursorPos;
    QFETCH(Qt::Edges, edges);

    if (edges & Qt::LeftEdge) {
        cursorPos.setX(window->frameGeometry().left());
    } else if (edges & Qt::RightEdge) {
        cursorPos.setX(window->frameGeometry().right() - 1);
    } else {
        cursorPos.setX(window->frameGeometry().center().x());
    }

    if (edges & Qt::TopEdge) {
        cursorPos.setY(window->frameGeometry().top());
    } else if (edges & Qt::BottomEdge) {
        cursorPos.setY(window->frameGeometry().bottom() - 1);
    } else {
        cursorPos.setY(window->frameGeometry().center().y());
    }

    Cursors::self()->mouse()->setPos(cursorPos);

    // wait for the enter event and set the cursor
    QVERIFY(enteredSpy.wait());
    std::unique_ptr<KWayland::Client::Surface> cursorSurface(Test::createSurface());
    QVERIFY(cursorSurface);
    QSignalSpy cursorRenderedSpy(cursorSurface.get(), &KWayland::Client::Surface::frameRendered);
    cursorSurface->attachBuffer(Test::waylandShmPool()->createBuffer(arrowCursor.image()));
    cursorSurface->damage(arrowCursor.image().rect());
    cursorSurface->commit();
    pointer->setCursor(cursorSurface.get(), arrowCursor.hotSpot());
    QVERIFY(cursorRenderedSpy.wait());

    // start resizing the window
    int timestamp = 1;
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    Test::pointerButtonPressed(BTN_RIGHT, timestamp++);
    QVERIFY(window->isInteractiveResize());

    QFETCH(KWin::CursorShape, cursorShape);
    const PlatformCursorImage resizeCursor = loadReferenceThemeCursor(cursorShape);
    QVERIFY(!resizeCursor.isNull());
    QCOMPARE(kwinApp()->cursorImage().image(), resizeCursor.image());
    QCOMPARE(kwinApp()->cursorImage().hotSpot(), resizeCursor.hotSpot());

    // finish resizing the window
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    Test::pointerButtonReleased(BTN_RIGHT, timestamp++);
    QVERIFY(!window->isInteractiveResize());

    QCOMPARE(kwinApp()->cursorImage().image(), arrowCursor.image());
    QCOMPARE(kwinApp()->cursorImage().hotSpot(), arrowCursor.hotSpot());
}

void PointerInputTest::testMoveCursor()
{
    // this test verifies that the cursor has correct shape during move operation

    // first modify the config for this run
    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandAllKey", "Meta");
    group.writeEntry("CommandAll1", "Move");
    group.sync();
    workspace()->slotReconfigure();
    QCOMPARE(options->commandAllModifier(), Qt::MetaModifier);
    QCOMPARE(options->commandAll1(), Options::MouseUnrestrictedMove);

    // load the fallback cursor (arrow cursor)
    const PlatformCursorImage arrowCursor = loadReferenceThemeCursor(Qt::ArrowCursor);
    QVERIFY(!arrowCursor.isNull());
    QCOMPARE(kwinApp()->cursorImage().image(), arrowCursor.image());
    QCOMPARE(kwinApp()->cursorImage().hotSpot(), arrowCursor.hotSpot());

    // we need a pointer to get the enter event
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &KWayland::Client::Pointer::entered);

    // create a test window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // move cursor to the test position
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());

    // wait for the enter event and set the cursor
    QVERIFY(enteredSpy.wait());
    std::unique_ptr<KWayland::Client::Surface> cursorSurface = Test::createSurface();
    QVERIFY(cursorSurface);
    QSignalSpy cursorRenderedSpy(cursorSurface.get(), &KWayland::Client::Surface::frameRendered);
    cursorSurface->attachBuffer(Test::waylandShmPool()->createBuffer(arrowCursor.image()));
    cursorSurface->damage(arrowCursor.image().rect());
    cursorSurface->commit();
    pointer->setCursor(cursorSurface.get(), arrowCursor.hotSpot());
    QVERIFY(cursorRenderedSpy.wait());

    // start moving the window
    int timestamp = 1;
    Test::keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(window->isInteractiveMove());

    const PlatformCursorImage sizeAllCursor = loadReferenceThemeCursor(Qt::SizeAllCursor);
    QVERIFY(!sizeAllCursor.isNull());
    QCOMPARE(kwinApp()->cursorImage().image(), sizeAllCursor.image());
    QCOMPARE(kwinApp()->cursorImage().hotSpot(), sizeAllCursor.hotSpot());

    // finish moving the window
    Test::keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(!window->isInteractiveMove());

    QCOMPARE(kwinApp()->cursorImage().image(), arrowCursor.image());
    QCOMPARE(kwinApp()->cursorImage().hotSpot(), arrowCursor.hotSpot());
}

void PointerInputTest::testHideShowCursor()
{
    QCOMPARE(Cursors::self()->isCursorHidden(), false);
    Cursors::self()->hideCursor();
    QCOMPARE(Cursors::self()->isCursorHidden(), true);
    Cursors::self()->showCursor();
    QCOMPARE(Cursors::self()->isCursorHidden(), false);

    Cursors::self()->hideCursor();
    QCOMPARE(Cursors::self()->isCursorHidden(), true);
    Cursors::self()->hideCursor();
    Cursors::self()->hideCursor();
    Cursors::self()->hideCursor();
    QCOMPARE(Cursors::self()->isCursorHidden(), true);

    Cursors::self()->showCursor();
    QCOMPARE(Cursors::self()->isCursorHidden(), true);
    Cursors::self()->showCursor();
    QCOMPARE(Cursors::self()->isCursorHidden(), true);
    Cursors::self()->showCursor();
    QCOMPARE(Cursors::self()->isCursorHidden(), true);
    Cursors::self()->showCursor();
    QCOMPARE(Cursors::self()->isCursorHidden(), false);
}

void PointerInputTest::testDefaultInputRegion()
{
    // This test verifies that a surface that hasn't specified the input region can be focused.

    // Create a test window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // Move the point to the center of the surface.
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());
    QCOMPARE(waylandServer()->seat()->focusedPointerSurface(), window->surface());

    // Destroy the test window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

void PointerInputTest::testEmptyInputRegion()
{
    // This test verifies that a surface that has specified an empty input region can't be focused.

    // Create a test window.
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    QVERIFY(surface != nullptr);
    std::unique_ptr<KWayland::Client::Region> inputRegion(m_compositor->createRegion(QRegion()));
    surface->setInputRegion(inputRegion.get());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    QVERIFY(shellSurface != nullptr);
    Window *window = Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);
    QVERIFY(window);

    // Move the point to the center of the surface.
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());
    QVERIFY(!waylandServer()->seat()->focusedPointerSurface());

    // Destroy the test window.
    shellSurface.reset();
    QVERIFY(Test::waitForWindowDestroyed(window));
}

}

WAYLANDTEST_MAIN(KWin::PointerInputTest)
#include "pointer_input.moc"
