/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"
#include "platform.h"
#include "abstract_client.h"
#include "cursor.h"
#include "deleted.h"
#include "effects.h"
#include "pointer_input.h"
#include "options.h"
#include "screenedge.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "xcursortheme.h"
#include <kwineffects.h>

#include <KWayland/Client/buffer.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

#include <KWaylandServer/buffer_interface.h>
#include <KWaylandServer/clientconnection.h>
#include <KWaylandServer/seat_interface.h>

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

    QImage cursorImage = sprites.first().data();
    cursorImage.setDevicePixelRatio(theme.devicePixelRatio());

    QPoint cursorHotspot = sprites.first().hotspot();

    return PlatformCursorImage(cursorImage, cursorHotspot);
}

static PlatformCursorImage loadReferenceThemeCursor(const QByteArray &name)
{
    const Cursor *pointerCursor = Cursors::self()->mouse();

    const KXcursorTheme theme = KXcursorTheme::fromTheme(pointerCursor->themeName(),
                                                         pointerCursor->themeSize(),
                                                         screens()->maxScale());
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
    void testUpdateFocusAfterScreenChange();
    void testModifierClickUnrestrictedMove_data();
    void testModifierClickUnrestrictedMove();
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

private:
    void render(KWayland::Client::Surface *surface, const QSize &size = QSize(100, 50));
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
};

void PointerInputTest::initTestCase()
{
    qRegisterMetaType<KWin::AbstractClient*>();
    qRegisterMetaType<KWin::Deleted*>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(applicationStartedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs", Qt::DirectConnection, Q_ARG(int, 2));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));

    if (!QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("icons/DMZ-White/index.theme")).isEmpty()) {
        qputenv("XCURSOR_THEME", QByteArrayLiteral("DMZ-White"));
    } else {
        // might be vanilla-dmz (e.g. Arch, FreeBSD)
        qputenv("XCURSOR_THEME", QByteArrayLiteral("Vanilla-DMZ"));
    }
    qputenv("XCURSOR_SIZE", QByteArrayLiteral("24"));
    qputenv("XKB_DEFAULT_RULES", "evdev");

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
    waylandServer()->initWorkspace();
}

void PointerInputTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::Decoration));
    QVERIFY(Test::waitForWaylandPointer());
    m_compositor = Test::waylandCompositor();
    m_seat = Test::waylandSeat();

    screens()->setCurrent(0);
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
    using namespace KWayland::Client;
    // create pointer and signal spy for enter and leave signals
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &Pointer::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(pointer, &Pointer::left);
    QVERIFY(leftSpy.isValid());

    // create a window
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = Test::createSurface(m_compositor);
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);
    render(surface);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
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
    QCOMPARE(pointer->enteredSurface(), surface);
    // also on the server
    QCOMPARE(waylandServer()->seat()->focusedPointerSurface(), window->surface());

    // and out again
    Cursors::self()->mouse()->setPos(QPoint(250, 250));;
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.count(), 1);
    // there should not be a focused pointer surface anymore
    QVERIFY(!waylandServer()->seat()->focusedPointerSurface());
    QVERIFY(!pointer->enteredSurface());
}

void PointerInputTest::testWarpingGeneratesPointerMotion()
{
    // this test verifies that warping the pointer creates pointer motion events
    using namespace KWayland::Client;
    // create pointer and signal spy for enter and motion
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &Pointer::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy movedSpy(pointer, &Pointer::motion);
    QVERIFY(movedSpy.isValid());

    // create a window
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = Test::createSurface(m_compositor);
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);
    render(surface);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
    QVERIFY(window);

    // enter
    kwinApp()->platform()->pointerMotion(QPointF(25, 25), 1);
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
    using namespace KWayland::Client;

    // create pointer
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy movedSpy(pointer, &Pointer::motion);
    QVERIFY(movedSpy.isValid());

    // warp cursor into expected geometry
    Cursors::self()->mouse()->setPos(10, 10);

    // create a window
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = Test::createSurface(m_compositor);
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);
    render(surface);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
    QVERIFY(window);

    QCOMPARE(window->pos(), QPoint(0, 0));
    QVERIFY(window->frameGeometry().contains(Cursors::self()->mouse()->pos()));

    // is PresentWindows effect for top left screen edge loaded
    QVERIFY(static_cast<EffectsHandlerImpl*>(effects)->isEffectLoaded("presentwindows"));
    QVERIFY(movedSpy.isEmpty());
    quint32 timestamp = 0;
    kwinApp()->platform()->pointerMotion(QPoint(0, 0), timestamp++);
    // screen edges push back
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(1, 1));
    QVERIFY(movedSpy.wait());
    QCOMPARE(movedSpy.count(), 2);
    QCOMPARE(movedSpy.at(0).first().toPoint(), QPoint(0, 0));
    QCOMPARE(movedSpy.at(1).first().toPoint(), QPoint(1, 1));
}

void PointerInputTest::testUpdateFocusAfterScreenChange()
{
    // this test verifies that a pointer enter event is generated when the cursor changes to another
    // screen due to removal of screen
    using namespace KWayland::Client;
    // ensure cursor is on second screen
    Cursors::self()->mouse()->setPos(1500, 300);

    // create pointer and signal spy for enter and motion
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &Pointer::entered);
    QVERIFY(enteredSpy.isValid());

    // create a window
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = Test::createSurface(m_compositor);
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);
    render(surface, QSize(1280, 1024));
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
    QVERIFY(window);
    QVERIFY(!window->frameGeometry().contains(Cursors::self()->mouse()->pos()));

    QSignalSpy screensChangedSpy(screens(), &Screens::changed);
    QVERIFY(screensChangedSpy.isValid());
    // now let's remove the screen containing the cursor
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs",
                              Qt::DirectConnection,
                              Q_ARG(int, 1),
                              Q_ARG(QVector<QRect>, QVector<QRect>{QRect(0, 0, 1280, 1024)}));
    QVERIFY(screensChangedSpy.wait());
    QCOMPARE(screens()->count(), 1);

    // this should have warped the cursor
    QCOMPARE(Cursors::self()->mouse()->pos(), QPoint(639, 511));
    QVERIFY(window->frameGeometry().contains(Cursors::self()->mouse()->pos()));

    // and we should get an enter event
    QTRY_COMPARE(enteredSpy.count(), 1);
}

void PointerInputTest::testModifierClickUnrestrictedMove_data()
{
    QTest::addColumn<int>("modifierKey");
    QTest::addColumn<int>("mouseButton");
    QTest::addColumn<QString>("modKey");
    QTest::addColumn<bool>("capsLock");

    const QString alt = QStringLiteral("Alt");
    const QString meta = QStringLiteral("Meta");

    QTest::newRow("Left Alt + Left Click")    << KEY_LEFTALT  << BTN_LEFT   << alt << false;
    QTest::newRow("Left Alt + Right Click")   << KEY_LEFTALT  << BTN_RIGHT  << alt << false;
    QTest::newRow("Left Alt + Middle Click")  << KEY_LEFTALT  << BTN_MIDDLE << alt << false;
    QTest::newRow("Right Alt + Left Click")   << KEY_RIGHTALT << BTN_LEFT   << alt << false;
    QTest::newRow("Right Alt + Right Click")  << KEY_RIGHTALT << BTN_RIGHT  << alt << false;
    QTest::newRow("Right Alt + Middle Click") << KEY_RIGHTALT << BTN_MIDDLE << alt << false;
    // now everything with meta
    QTest::newRow("Left Meta + Left Click")    << KEY_LEFTMETA  << BTN_LEFT   << meta << false;
    QTest::newRow("Left Meta + Right Click")   << KEY_LEFTMETA  << BTN_RIGHT  << meta << false;
    QTest::newRow("Left Meta + Middle Click")  << KEY_LEFTMETA  << BTN_MIDDLE << meta << false;
    QTest::newRow("Right Meta + Left Click")   << KEY_RIGHTMETA << BTN_LEFT   << meta << false;
    QTest::newRow("Right Meta + Right Click")  << KEY_RIGHTMETA << BTN_RIGHT  << meta << false;
    QTest::newRow("Right Meta + Middle Click") << KEY_RIGHTMETA << BTN_MIDDLE << meta << false;

    // and with capslock
    QTest::newRow("Left Alt + Left Click/CapsLock")    << KEY_LEFTALT  << BTN_LEFT   << alt << true;
    QTest::newRow("Left Alt + Right Click/CapsLock")   << KEY_LEFTALT  << BTN_RIGHT  << alt << true;
    QTest::newRow("Left Alt + Middle Click/CapsLock")  << KEY_LEFTALT  << BTN_MIDDLE << alt << true;
    QTest::newRow("Right Alt + Left Click/CapsLock")   << KEY_RIGHTALT << BTN_LEFT   << alt << true;
    QTest::newRow("Right Alt + Right Click/CapsLock")  << KEY_RIGHTALT << BTN_RIGHT  << alt << true;
    QTest::newRow("Right Alt + Middle Click/CapsLock") << KEY_RIGHTALT << BTN_MIDDLE << alt << true;
    // now everything with meta
    QTest::newRow("Left Meta + Left Click/CapsLock")    << KEY_LEFTMETA  << BTN_LEFT   << meta << true;
    QTest::newRow("Left Meta + Right Click/CapsLock")   << KEY_LEFTMETA  << BTN_RIGHT  << meta << true;
    QTest::newRow("Left Meta + Middle Click/CapsLock")  << KEY_LEFTMETA  << BTN_MIDDLE << meta << true;
    QTest::newRow("Right Meta + Left Click/CapsLock")   << KEY_RIGHTMETA << BTN_LEFT   << meta << true;
    QTest::newRow("Right Meta + Right Click/CapsLock")  << KEY_RIGHTMETA << BTN_RIGHT  << meta << true;
    QTest::newRow("Right Meta + Middle Click/CapsLock") << KEY_RIGHTMETA << BTN_MIDDLE << meta << true;
}

void PointerInputTest::testModifierClickUnrestrictedMove()
{
    // this test ensures that Alt+mouse button press triggers unrestricted move
    using namespace KWayland::Client;
    // create pointer and signal spy for button events
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy buttonSpy(pointer, &Pointer::buttonStateChanged);
    QVERIFY(buttonSpy.isValid());

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
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = Test::createSurface(m_compositor);
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);
    render(surface);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
    QVERIFY(window);

    // move cursor on window
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());

    // simulate modifier+click
    quint32 timestamp = 1;
    QFETCH(bool, capsLock);
    if (capsLock) {
        kwinApp()->platform()->keyboardKeyPressed(KEY_CAPSLOCK, timestamp++);
    }
    QFETCH(int, modifierKey);
    QFETCH(int, mouseButton);
    kwinApp()->platform()->keyboardKeyPressed(modifierKey, timestamp++);
    QVERIFY(!window->isMove());
    kwinApp()->platform()->pointerButtonPressed(mouseButton, timestamp++);
    QVERIFY(window->isMove());
    // release modifier should not change it
    kwinApp()->platform()->keyboardKeyReleased(modifierKey, timestamp++);
    QVERIFY(window->isMove());
    // but releasing the key should end move/resize
    kwinApp()->platform()->pointerButtonReleased(mouseButton, timestamp++);
    QVERIFY(!window->isMove());
    if (capsLock) {
        kwinApp()->platform()->keyboardKeyReleased(KEY_CAPSLOCK, timestamp++);
    }

    // all of that should not have triggered button events on the surface
    QCOMPARE(buttonSpy.count(), 0);
    // also waiting shouldn't give us the event
    QVERIFY(!buttonSpy.wait(100));
}

void PointerInputTest::testModifierClickUnrestrictedMoveGlobalShortcutsDisabled()
{
    // this test ensures that Alt+mouse button press triggers unrestricted move
    using namespace KWayland::Client;
    // create pointer and signal spy for button events
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy buttonSpy(pointer, &Pointer::buttonStateChanged);
    QVERIFY(buttonSpy.isValid());

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
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = Test::createSurface(m_compositor);
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);
    render(surface);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
    QVERIFY(window);

    // disable global shortcuts
    QVERIFY(!workspace()->globalShortcutsDisabled());
    workspace()->disableGlobalShortcutsForClient(true);
    QVERIFY(workspace()->globalShortcutsDisabled());

    // move cursor on window
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());

    // simulate modifier+click
    quint32 timestamp = 1;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    QVERIFY(!window->isMove());
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(!window->isMove());
    // release modifier should not change it
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    QVERIFY(!window->isMove());
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);

    workspace()->disableGlobalShortcutsForClient(false);
}

void PointerInputTest::testModifierScrollOpacity_data()
{
    QTest::addColumn<int>("modifierKey");
    QTest::addColumn<QString>("modKey");
    QTest::addColumn<bool>("capsLock");

    const QString alt = QStringLiteral("Alt");
    const QString meta = QStringLiteral("Meta");

    QTest::newRow("Left Alt")   << KEY_LEFTALT  << alt << false;
    QTest::newRow("Right Alt")  << KEY_RIGHTALT << alt << false;
    QTest::newRow("Left Meta")  << KEY_LEFTMETA  << meta << false;
    QTest::newRow("Right Meta") << KEY_RIGHTMETA << meta << false;
    QTest::newRow("Left Alt/CapsLock")   << KEY_LEFTALT  << alt << true;
    QTest::newRow("Right Alt/CapsLock")  << KEY_RIGHTALT << alt << true;
    QTest::newRow("Left Meta/CapsLock")  << KEY_LEFTMETA  << meta << true;
    QTest::newRow("Right Meta/CapsLock") << KEY_RIGHTMETA << meta << true;
}

void PointerInputTest::testModifierScrollOpacity()
{
    // this test verifies that mod+wheel performs a window operation and does not
    // pass the wheel to the window
    using namespace KWayland::Client;
    // create pointer and signal spy for button events
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy axisSpy(pointer, &Pointer::axisChanged);
    QVERIFY(axisSpy.isValid());

    // first modify the config for this run
    QFETCH(QString, modKey);
    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandAllKey", modKey);
    group.writeEntry("CommandAllWheel", "change opacity");
    group.sync();
    workspace()->slotReconfigure();

    // create a window
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = Test::createSurface(m_compositor);
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);
    render(surface);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
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
        kwinApp()->platform()->keyboardKeyPressed(KEY_CAPSLOCK, timestamp++);
    }
    QFETCH(int, modifierKey);
    kwinApp()->platform()->keyboardKeyPressed(modifierKey, timestamp++);
    kwinApp()->platform()->pointerAxisVertical(-5, timestamp++);
    QCOMPARE(window->opacity(), 0.6);
    kwinApp()->platform()->pointerAxisVertical(5, timestamp++);
    QCOMPARE(window->opacity(), 0.5);
    kwinApp()->platform()->keyboardKeyReleased(modifierKey, timestamp++);
    if (capsLock) {
        kwinApp()->platform()->keyboardKeyReleased(KEY_CAPSLOCK, timestamp++);
    }

    // axis should have been filtered out
    QCOMPARE(axisSpy.count(), 0);
    QVERIFY(!axisSpy.wait(100));
}

void PointerInputTest::testModifierScrollOpacityGlobalShortcutsDisabled()
{
    // this test verifies that mod+wheel performs a window operation and does not
    // pass the wheel to the window
    using namespace KWayland::Client;
    // create pointer and signal spy for button events
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy axisSpy(pointer, &Pointer::axisChanged);
    QVERIFY(axisSpy.isValid());

    // first modify the config for this run
    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandAllKey", "Meta");
    group.writeEntry("CommandAllWheel", "change opacity");
    group.sync();
    workspace()->slotReconfigure();

    // create a window
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = Test::createSurface(m_compositor);
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);
    render(surface);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
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
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    kwinApp()->platform()->pointerAxisVertical(-5, timestamp++);
    QCOMPARE(window->opacity(), 0.5);
    kwinApp()->platform()->pointerAxisVertical(5, timestamp++);
    QCOMPARE(window->opacity(), 0.5);
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTMETA, timestamp++);

    workspace()->disableGlobalShortcutsForClient(false);
}

void  PointerInputTest::testScrollAction()
{
    // this test verifies that scroll on inactive window performs a mouse action
    using namespace KWayland::Client;
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy axisSpy(pointer, &Pointer::axisChanged);
    QVERIFY(axisSpy.isValid());

    // first modify the config for this run
    KConfigGroup group = kwinApp()->config()->group("MouseBindings");
    group.writeEntry("CommandWindowWheel", "activate and scroll");
    group.sync();
    workspace()->slotReconfigure();
    // create two windows
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface1 = Test::createSurface(m_compositor);
    QVERIFY(surface1);
    XdgShellSurface *shellSurface1 = Test::createXdgShellStableSurface(surface1, surface1);
    QVERIFY(shellSurface1);
    render(surface1);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window1 = workspace()->activeClient();
    QVERIFY(window1);
    Surface *surface2 = Test::createSurface(m_compositor);
    QVERIFY(surface2);
    XdgShellSurface *shellSurface2 = Test::createXdgShellStableSurface(surface2, surface2);
    QVERIFY(shellSurface2);
    render(surface2);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window2 = workspace()->activeClient();
    QVERIFY(window2);
    QVERIFY(window1 != window2);

    // move cursor to the inactive window
    Cursors::self()->mouse()->setPos(window1->frameGeometry().center());

    quint32 timestamp = 1;
    QVERIFY(!window1->isActive());
    kwinApp()->platform()->pointerAxisVertical(5, timestamp++);
    QVERIFY(window1->isActive());

    // but also the wheel event should be passed to the window
    QVERIFY(axisSpy.wait());

    // we need to wait a little bit, otherwise the test crashes in effectshandler, needs fixing
    QTest::qWait(100);
}

void PointerInputTest::testFocusFollowsMouse()
{
    using namespace KWayland::Client;
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
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface1 = Test::createSurface(m_compositor);
    QVERIFY(surface1);
    XdgShellSurface *shellSurface1 = Test::createXdgShellStableSurface(surface1, surface1);
    QVERIFY(shellSurface1);
    render(surface1, QSize(800, 800));
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window1 = workspace()->activeClient();
    QVERIFY(window1);
    Surface *surface2 = Test::createSurface(m_compositor);
    QVERIFY(surface2);
    XdgShellSurface *shellSurface2 = Test::createXdgShellStableSurface(surface2, surface2);
    QVERIFY(shellSurface2);
    render(surface2, QSize(800, 800));
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window2 = workspace()->activeClient();
    QVERIFY(window2);
    QVERIFY(window1 != window2);
    QCOMPARE(workspace()->topClientOnDesktop(1, -1), window2);
    // geometry of the two windows should be overlapping
    QVERIFY(window1->frameGeometry().intersects(window2->frameGeometry()));

    // signal spies for active window changed and stacking order changed
    QSignalSpy activeWindowChangedSpy(workspace(), &Workspace::clientActivated);
    QVERIFY(activeWindowChangedSpy.isValid());
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(stackingOrderChangedSpy.isValid());

    QVERIFY(!window1->isActive());
    QVERIFY(window2->isActive());

    // move on top of first window
    QVERIFY(window1->frameGeometry().contains(10, 10));
    QVERIFY(!window2->frameGeometry().contains(10, 10));
    Cursors::self()->mouse()->setPos(10, 10);
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(stackingOrderChangedSpy.count(), 1);
    QCOMPARE(workspace()->topClientOnDesktop(1, -1), window1);
    QTRY_VERIFY(window1->isActive());

    // move on second window, but move away before active window change delay hits
    Cursors::self()->mouse()->setPos(810, 810);
    QVERIFY(stackingOrderChangedSpy.wait());
    QCOMPARE(stackingOrderChangedSpy.count(), 2);
    QCOMPARE(workspace()->topClientOnDesktop(1, -1), window2);
    Cursors::self()->mouse()->setPos(10, 10);
    QVERIFY(!activeWindowChangedSpy.wait(250));
    QVERIFY(window1->isActive());
    QCOMPARE(workspace()->topClientOnDesktop(1, -1), window1);
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

    QTest::newRow("Left")   << quint32(BTN_LEFT);
    QTest::newRow("Middle") << quint32(BTN_MIDDLE);
    QTest::newRow("Right")  << quint32(BTN_RIGHT);
}

void PointerInputTest::testMouseActionInactiveWindow()
{
    // this test performs the mouse button window action on an inactive window
    // it should activate the window and raise it
    using namespace KWayland::Client;

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
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface1 = Test::createSurface(m_compositor);
    QVERIFY(surface1);
    XdgShellSurface *shellSurface1 = Test::createXdgShellStableSurface(surface1, surface1);
    QVERIFY(shellSurface1);
    render(surface1, QSize(800, 800));
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window1 = workspace()->activeClient();
    QVERIFY(window1);
    Surface *surface2 = Test::createSurface(m_compositor);
    QVERIFY(surface2);
    XdgShellSurface *shellSurface2 = Test::createXdgShellStableSurface(surface2, surface2);
    QVERIFY(shellSurface2);
    render(surface2, QSize(800, 800));
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window2 = workspace()->activeClient();
    QVERIFY(window2);
    QVERIFY(window1 != window2);
    QCOMPARE(workspace()->topClientOnDesktop(1, -1), window2);
    // geometry of the two windows should be overlapping
    QVERIFY(window1->frameGeometry().intersects(window2->frameGeometry()));

    // signal spies for active window changed and stacking order changed
    QSignalSpy activeWindowChangedSpy(workspace(), &Workspace::clientActivated);
    QVERIFY(activeWindowChangedSpy.isValid());
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(stackingOrderChangedSpy.isValid());

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
    kwinApp()->platform()->pointerButtonPressed(button, timestamp++);
    // should raise window1 and activate it
    QCOMPARE(stackingOrderChangedSpy.count(), 1);
    QVERIFY(!activeWindowChangedSpy.isEmpty());
    QCOMPARE(workspace()->topClientOnDesktop(1, -1), window1);
    QVERIFY(window1->isActive());
    QVERIFY(!window2->isActive());

    // release again
    kwinApp()->platform()->pointerButtonReleased(button, timestamp++);
}

void PointerInputTest::testMouseActionActiveWindow_data()
{
    QTest::addColumn<bool>("clickRaise");
    QTest::addColumn<quint32>("button");

    for (quint32 i=BTN_LEFT; i < BTN_JOYSTICK; i++) {
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
    using namespace KWayland::Client;
    // create a button spy - all clicks should be passed through
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy buttonSpy(pointer, &Pointer::buttonStateChanged);
    QVERIFY(buttonSpy.isValid());

    // adjust config for this run
    QFETCH(bool, clickRaise);
    KConfigGroup group = kwinApp()->config()->group("Windows");
    group.writeEntry("ClickRaise", clickRaise);
    group.sync();
    workspace()->slotReconfigure();
    QCOMPARE(options->isClickRaise(), clickRaise);

    // create two windows
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface1 = Test::createSurface(m_compositor);
    QVERIFY(surface1);
    XdgShellSurface *shellSurface1 = Test::createXdgShellStableSurface(surface1, surface1);
    QVERIFY(shellSurface1);
    render(surface1, QSize(800, 800));
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window1 = workspace()->activeClient();
    QVERIFY(window1);
    QSignalSpy window1DestroyedSpy(window1, &QObject::destroyed);
    QVERIFY(window1DestroyedSpy.isValid());
    Surface *surface2 = Test::createSurface(m_compositor);
    QVERIFY(surface2);
    XdgShellSurface *shellSurface2 = Test::createXdgShellStableSurface(surface2, surface2);
    QVERIFY(shellSurface2);
    render(surface2, QSize(800, 800));
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window2 = workspace()->activeClient();
    QVERIFY(window2);
    QVERIFY(window1 != window2);
    QSignalSpy window2DestroyedSpy(window2, &QObject::destroyed);
    QVERIFY(window2DestroyedSpy.isValid());
    QCOMPARE(workspace()->topClientOnDesktop(1, -1), window2);
    // geometry of the two windows should be overlapping
    QVERIFY(window1->frameGeometry().intersects(window2->frameGeometry()));
    // lower the currently active window
    workspace()->lowerClient(window2);
    QCOMPARE(workspace()->topClientOnDesktop(1, -1), window1);

    // signal spy for stacking order spy
    QSignalSpy stackingOrderChangedSpy(workspace(), &Workspace::stackingOrderChanged);
    QVERIFY(stackingOrderChangedSpy.isValid());

    // move on top of second window
    QVERIFY(!window1->frameGeometry().contains(900, 900));
    QVERIFY(window2->frameGeometry().contains(900, 900));
    Cursors::self()->mouse()->setPos(900, 900);

    // and click
    quint32 timestamp = 1;
    QFETCH(quint32, button);
    kwinApp()->platform()->pointerButtonPressed(button, timestamp++);
    QVERIFY(buttonSpy.wait());
    if (clickRaise) {
        QCOMPARE(stackingOrderChangedSpy.count(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(workspace()->topClientOnDesktop(1, -1), window2, 200);
    } else {
        QCOMPARE(stackingOrderChangedSpy.count(), 0);
        QVERIFY(!stackingOrderChangedSpy.wait(100));
        QCOMPARE(workspace()->topClientOnDesktop(1, -1), window1);
    }

    // release again
    kwinApp()->platform()->pointerButtonReleased(button, timestamp++);

    delete surface1;
    QVERIFY(window1DestroyedSpy.wait());
    delete surface2;
    QVERIFY(window2DestroyedSpy.wait());
}

void PointerInputTest::testCursorImage()
{
    // this test verifies that the pointer image gets updated correctly from the client provided data
    using namespace KWayland::Client;
    // we need a pointer to get the enter event
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &Pointer::entered);
    QVERIFY(enteredSpy.isValid());

    // move cursor somewhere the new window won't open
    auto cursor = Cursors::self()->mouse();
    cursor->setPos(800, 800);
    auto p = input()->pointer();
    // at the moment it should be the fallback cursor
    const QImage fallbackCursor = cursor->image();
    QVERIFY(!fallbackCursor.isNull());

    // create a window
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = Test::createSurface(m_compositor);
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);
    render(surface);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
    QVERIFY(window);

    // move cursor to center of window, this should first set a null pointer, so we still show old cursor
    cursor->setPos(window->frameGeometry().center());
    QCOMPARE(p->focus(), window);
    QCOMPARE(cursor->image(), fallbackCursor);
    QVERIFY(enteredSpy.wait());

    // create a cursor on the pointer
    Surface *cursorSurface = Test::createSurface(m_compositor);
    QVERIFY(cursorSurface);
    QSignalSpy cursorRenderedSpy(cursorSurface, &Surface::frameRendered);
    QVERIFY(cursorRenderedSpy.isValid());
    QImage red = QImage(QSize(10, 10), QImage::Format_ARGB32_Premultiplied);
    red.fill(Qt::red);
    cursorSurface->attachBuffer(Test::waylandShmPool()->createBuffer(red));
    cursorSurface->damage(QRect(0, 0, 10, 10));
    cursorSurface->commit();
    pointer->setCursor(cursorSurface, QPoint(5, 5));
    QVERIFY(cursorRenderedSpy.wait());
    QCOMPARE(cursor->image(), red);
    QCOMPARE(cursor->hotspot(), QPoint(5, 5));
    // change hotspot
    pointer->setCursor(cursorSurface, QPoint(6, 6));
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
    QCOMPARE(cursor->hotspot(), QPoint(6, 6)); //surface-local (so not changed)

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
    HelperEffect() {}
    ~HelperEffect() override {}
};

void PointerInputTest::testEffectOverrideCursorImage()
{
    // this test verifies the effect cursor override handling
    using namespace KWayland::Client;
    // we need a pointer to get the enter event and set a cursor
    auto pointer = m_seat->createPointer(m_seat);
    auto cursor = Cursors::self()->mouse();
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &Pointer::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(pointer, &Pointer::left);
    QVERIFY(leftSpy.isValid());
    // move cursor somewhere the new window won't open
    cursor->setPos(800, 800);
    // here we should have the fallback cursor
    const QImage fallback = cursor->image();
    QVERIFY(!fallback.isNull());

    // now let's create a window
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = Test::createSurface(m_compositor);
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);
    render(surface);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
    QVERIFY(window);

    // and move cursor to the window
    QVERIFY(!window->frameGeometry().contains(QPoint(800, 800)));
    cursor->setPos(window->frameGeometry().center());
    QVERIFY(enteredSpy.wait());
    // cursor image should still be fallback
    QCOMPARE(cursor->image(), fallback);

    // now create an effect and set an override cursor
    QScopedPointer<HelperEffect> effect(new HelperEffect);
    effects->startMouseInterception(effect.data(), Qt::SizeAllCursor);
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
    effects->stopMouseInterception(effect.data());
    QCOMPARE(cursor->image(), fallback);

    // start mouse interception again
    effects->startMouseInterception(effect.data(), Qt::SizeAllCursor);
    QCOMPARE(cursor->image(), sizeAll);

    // move cursor to area of window
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());
    // this should not result in an enter event
    QVERIFY(!enteredSpy.wait(100));

    // after ending the interception we should get an enter event
    effects->stopMouseInterception(effect.data());
    QVERIFY(enteredSpy.wait());
    QVERIFY(cursor->image().isNull());
}

void PointerInputTest::testPopup()
{
    // this test validates the basic popup behavior
    // a button press outside the window should dismiss the popup

    // first create a parent surface
    using namespace KWayland::Client;
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &Pointer::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(pointer, &Pointer::left);
    QVERIFY(leftSpy.isValid());
    QSignalSpy buttonStateChangedSpy(pointer, &Pointer::buttonStateChanged);
    QVERIFY(buttonStateChangedSpy.isValid());
    QSignalSpy motionSpy(pointer, &Pointer::motion);
    QVERIFY(motionSpy.isValid());

    Cursors::self()->mouse()->setPos(800, 800);

    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = Test::createSurface(m_compositor);
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);
    render(surface);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
    QVERIFY(window);
    QCOMPARE(window->hasPopupGrab(), false);
    // move pointer into window
    QVERIFY(!window->frameGeometry().contains(QPoint(800, 800)));
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());
    QVERIFY(enteredSpy.wait());
    // click inside window to create serial
    quint32 timestamp = 0;
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(buttonStateChangedSpy.wait());

    // now create the popup surface
    XdgPositioner positioner(QSize(100, 50), QRect(0, 0, 80, 20));
    positioner.setAnchorEdge(Qt::BottomEdge | Qt::RightEdge);
    positioner.setGravity(Qt::BottomEdge | Qt::RightEdge);
    Surface *popupSurface = Test::createSurface(m_compositor);
    QVERIFY(popupSurface);
    XdgShellPopup *popupShellSurface = Test::createXdgShellStablePopup(popupSurface, shellSurface, positioner);
    QVERIFY(popupShellSurface);
    QSignalSpy popupDoneSpy(popupShellSurface, &XdgShellPopup::popupDone);
    QVERIFY(popupDoneSpy.isValid());
    popupShellSurface->requestGrab(Test::waylandSeat(), 0); // FIXME: Serial.
    render(popupSurface, positioner.initialSize());
    QVERIFY(clientAddedSpy.wait());
    auto popupClient = clientAddedSpy.last().first().value<AbstractClient *>();
    QVERIFY(popupClient);
    QVERIFY(popupClient != window);
    QCOMPARE(window, workspace()->activeClient());
    QCOMPARE(popupClient->transientFor(), window);
    QCOMPARE(popupClient->pos(), window->pos() + QPoint(80, 20));
    QCOMPARE(popupClient->hasPopupGrab(), true);

    // let's move the pointer into the center of the window
    Cursors::self()->mouse()->setPos(popupClient->frameGeometry().center());
    QVERIFY(enteredSpy.wait());
    QCOMPARE(enteredSpy.count(), 2);
    QCOMPARE(leftSpy.count(), 1);
    QCOMPARE(pointer->enteredSurface(), popupSurface);

    // let's move the pointer outside of the popup window
    // this should not really change anything, it gets a leave event
    Cursors::self()->mouse()->setPos(popupClient->frameGeometry().bottomRight() + QPoint(2, 2));
    QVERIFY(leftSpy.wait());
    QCOMPARE(leftSpy.count(), 2);
    QVERIFY(popupDoneSpy.isEmpty());
    // now click, should trigger popupDone
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(popupDoneSpy.wait());
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
}

void PointerInputTest::testDecoCancelsPopup()
{
    // this test verifies that clicking the window decoration of parent window
    // cancels the popup

    // first create a parent surface
    using namespace KWayland::Client;
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &Pointer::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(pointer, &Pointer::left);
    QVERIFY(leftSpy.isValid());
    QSignalSpy buttonStateChangedSpy(pointer, &Pointer::buttonStateChanged);
    QVERIFY(buttonStateChangedSpy.isValid());
    QSignalSpy motionSpy(pointer, &Pointer::motion);
    QVERIFY(motionSpy.isValid());

    Cursors::self()->mouse()->setPos(800, 800);
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = Test::createSurface(m_compositor);
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);

    auto deco = Test::waylandServerSideDecoration()->create(surface, surface);
    QSignalSpy decoSpy(deco, &ServerSideDecoration::modeChanged);
    QVERIFY(decoSpy.isValid());
    QVERIFY(decoSpy.wait());
    deco->requestMode(ServerSideDecoration::Mode::Server);
    QVERIFY(decoSpy.wait());
    QCOMPARE(deco->mode(), ServerSideDecoration::Mode::Server);
    render(surface);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
    QVERIFY(window);
    QCOMPARE(window->hasPopupGrab(), false);
    QVERIFY(window->isDecorated());

    // move pointer into window
    QVERIFY(!window->frameGeometry().contains(QPoint(800, 800)));
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());
    QVERIFY(enteredSpy.wait());
    // click inside window to create serial
    quint32 timestamp = 0;
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(buttonStateChangedSpy.wait());

    // now create the popup surface
    XdgPositioner positioner(QSize(100, 50), QRect(0, 0, 80, 20));
    positioner.setAnchorEdge(Qt::BottomEdge | Qt::RightEdge);
    positioner.setGravity(Qt::BottomEdge | Qt::RightEdge);
    Surface *popupSurface = Test::createSurface(m_compositor);
    QVERIFY(popupSurface);
    XdgShellPopup *popupShellSurface = Test::createXdgShellStablePopup(popupSurface, shellSurface, positioner);
    QVERIFY(popupShellSurface);
    QSignalSpy popupDoneSpy(popupShellSurface, &XdgShellPopup::popupDone);
    QVERIFY(popupDoneSpy.isValid());
    popupShellSurface->requestGrab(Test::waylandSeat(), 0); // FIXME: Serial.
    render(popupSurface, positioner.initialSize());
    QVERIFY(clientAddedSpy.wait());
    auto popupClient = clientAddedSpy.last().first().value<AbstractClient *>();
    QVERIFY(popupClient);
    QVERIFY(popupClient != window);
    QCOMPARE(window, workspace()->activeClient());
    QCOMPARE(popupClient->transientFor(), window);
    QCOMPARE(popupClient->pos(), window->pos() + window->clientPos() + QPoint(80, 20));
    QCOMPARE(popupClient->hasPopupGrab(), true);

    // let's move the pointer into the center of the deco
    Cursors::self()->mouse()->setPos(window->frameGeometry().center().x(), window->y() + (window->height() - window->clientSize().height()) / 2);

    kwinApp()->platform()->pointerButtonPressed(BTN_RIGHT, timestamp++);
    QVERIFY(popupDoneSpy.wait());
    kwinApp()->platform()->pointerButtonReleased(BTN_RIGHT, timestamp++);
}

void PointerInputTest::testWindowUnderCursorWhileButtonPressed()
{
    // this test verifies that opening a window underneath the mouse cursor does not
    // trigger a leave event if a button is pressed
    // see BUG: 372876

    // first create a parent surface
    using namespace KWayland::Client;
    auto pointer = m_seat->createPointer(m_seat);
    QVERIFY(pointer);
    QVERIFY(pointer->isValid());
    QSignalSpy enteredSpy(pointer, &Pointer::entered);
    QVERIFY(enteredSpy.isValid());
    QSignalSpy leftSpy(pointer, &Pointer::left);
    QVERIFY(leftSpy.isValid());

    Cursors::self()->mouse()->setPos(800, 800);
    QSignalSpy clientAddedSpy(workspace(), &Workspace::clientAdded);
    QVERIFY(clientAddedSpy.isValid());
    Surface *surface = Test::createSurface(m_compositor);
    QVERIFY(surface);
    XdgShellSurface *shellSurface = Test::createXdgShellStableSurface(surface, surface);
    QVERIFY(shellSurface);
    render(surface);
    QVERIFY(clientAddedSpy.wait());
    AbstractClient *window = workspace()->activeClient();
    QVERIFY(window);

    // move cursor over window
    QVERIFY(!window->frameGeometry().contains(QPoint(800, 800)));
    Cursors::self()->mouse()->setPos(window->frameGeometry().center());
    QVERIFY(enteredSpy.wait());
    // click inside window
    quint32 timestamp = 0;
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);

    // now create a second window as transient
    XdgPositioner positioner(QSize(99, 49), QRect(0, 0, 1, 1));
    positioner.setAnchorEdge(Qt::BottomEdge | Qt::RightEdge);
    positioner.setGravity(Qt::BottomEdge | Qt::RightEdge);
    Surface *popupSurface = Test::createSurface(m_compositor);
    QVERIFY(popupSurface);
    XdgShellPopup *popupShellSurface = Test::createXdgShellStablePopup(popupSurface, shellSurface, positioner);
    QVERIFY(popupShellSurface);
    render(popupSurface, positioner.initialSize());
    QVERIFY(clientAddedSpy.wait());
    auto popupClient = clientAddedSpy.last().first().value<AbstractClient *>();
    QVERIFY(popupClient);
    QVERIFY(popupClient != window);
    QVERIFY(window->frameGeometry().contains(Cursors::self()->mouse()->pos()));
    QVERIFY(popupClient->frameGeometry().contains(Cursors::self()->mouse()->pos()));
    QVERIFY(!leftSpy.wait());

    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
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

    QTest::newRow("move top-left - left screen")       << QPoint(640, 512)   << QPoint(-100, -100) << QPoint(0, 0);
    QTest::newRow("move top - left screen")            << QPoint(640, 512)   << QPoint(640, -100)  << QPoint(640, 0);
    QTest::newRow("move top-right - left screen")      << QPoint(640, 512)   << QPoint(1380, -100) << QPoint(1380, 0);
    QTest::newRow("move right - left screen")          << QPoint(640, 512)   << QPoint(1380, 512)  << QPoint(1380, 512);
    QTest::newRow("move bottom-right - left screen")   << QPoint(640, 512)   << QPoint(1380, 1124) << QPoint(1380, 1124);
    QTest::newRow("move bottom - left screen")         << QPoint(640, 512)   << QPoint(640, 1124)  << QPoint(640, 1023);
    QTest::newRow("move bottom-left - left screen")    << QPoint(640, 512)   << QPoint(-100, 1124) << QPoint(0, 1023);
    QTest::newRow("move left - left screen")           << QPoint(640, 512)   << QPoint(-100, 512)  << QPoint(0, 512);

    QTest::newRow("move top-left - top screen")        << QPoint(1920, 512)  << QPoint(1180, -100) << QPoint(1180, 0);
    QTest::newRow("move top - top screen")             << QPoint(1920, 512)  << QPoint(1920, -100) << QPoint(1920, 0);
    QTest::newRow("move top-right - top screen")       << QPoint(1920, 512)  << QPoint(2660, -100) << QPoint(2660, 0);
    QTest::newRow("move right - top screen")           << QPoint(1920, 512)  << QPoint(2660, 512)  << QPoint(2660, 512);
    QTest::newRow("move bottom-right - top screen")    << QPoint(1920, 512)  << QPoint(2660, 1124) << QPoint(2559, 1023);
    QTest::newRow("move bottom - top screen")          << QPoint(1920, 512)  << QPoint(1920, 1124) << QPoint(1920, 1124);
    QTest::newRow("move bottom-left - top screen")     << QPoint(1920, 512)  << QPoint(1180, 1124) << QPoint(1280, 1023);
    QTest::newRow("move left - top screen")            << QPoint(1920, 512)  << QPoint(1180, 512)  << QPoint(1180, 512);

    QTest::newRow("move top-left - right screen")      << QPoint(3200, 512)  << QPoint(2460, -100) << QPoint(2460, 0);
    QTest::newRow("move top - right screen")           << QPoint(3200, 512)  << QPoint(3200, -100) << QPoint(3200, 0);
    QTest::newRow("move top-right - right screen")     << QPoint(3200, 512)  << QPoint(3940, -100) << QPoint(3839, 0);
    QTest::newRow("move right - right screen")         << QPoint(3200, 512)  << QPoint(3940, 512)  << QPoint(3839, 512);
    QTest::newRow("move bottom-right - right screen")  << QPoint(3200, 512)  << QPoint(3940, 1124) << QPoint(3839, 1023);
    QTest::newRow("move bottom - right screen")        << QPoint(3200, 512)  << QPoint(3200, 1124) << QPoint(3200, 1023);
    QTest::newRow("move bottom-left - right screen")   << QPoint(3200, 512)  << QPoint(2460, 1124) << QPoint(2460, 1124);
    QTest::newRow("move left - right screen")          << QPoint(3200, 512)  << QPoint(2460, 512)  << QPoint(2460, 512);

    QTest::newRow("move top-left - bottom screen")     << QPoint(1920, 1536) << QPoint(1180, 924)  << QPoint(1180, 924);
    QTest::newRow("move top - bottom screen")          << QPoint(1920, 1536) << QPoint(1920, 924)  << QPoint(1920, 924);
    QTest::newRow("move top-right - bottom screen")    << QPoint(1920, 1536) << QPoint(2660, 924)  << QPoint(2660, 924);
    QTest::newRow("move right - bottom screen")        << QPoint(1920, 1536) << QPoint(2660, 1536) << QPoint(2559, 1536);
    QTest::newRow("move bottom-right - bottom screen") << QPoint(1920, 1536) << QPoint(2660, 2148) << QPoint(2559, 2047);
    QTest::newRow("move bottom - bottom screen")       << QPoint(1920, 1536) << QPoint(1920, 2148) << QPoint(1920, 2047);
    QTest::newRow("move bottom-left - bottom screen")  << QPoint(1920, 1536) << QPoint(1180, 2148) << QPoint(1280, 2047);
    QTest::newRow("move left - bottom screen")         << QPoint(1920, 1536) << QPoint(1180, 1536) << QPoint(1280, 1536);
}

void PointerInputTest::testConfineToScreenGeometry()
{
    // this test verifies that pointer belongs to at least one screen
    // after moving it to off-screen area

    // unload the Present Windows effect because it pushes back
    // pointer if it's at (0, 0)
    static_cast<EffectsHandlerImpl*>(effects)->unloadEffect(QStringLiteral("presentwindows"));

    // setup screen layout
    const QVector<QRect> geometries {
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
        QRect(2560, 0, 1280, 1024),
        QRect(1280, 1024, 1280, 1024)
    };
    QMetaObject::invokeMethod(kwinApp()->platform(), "setVirtualOutputs",
                              Qt::DirectConnection,
                              Q_ARG(int, geometries.count()),
                              Q_ARG(QVector<QRect>, geometries));
    QCOMPARE(screens()->count(), geometries.count());
    QCOMPARE(screens()->geometry(0), geometries.at(0));
    QCOMPARE(screens()->geometry(1), geometries.at(1));
    QCOMPARE(screens()->geometry(2), geometries.at(2));
    QCOMPARE(screens()->geometry(3), geometries.at(3));

    // move pointer to initial position
    QFETCH(QPoint, startPos);
    Cursors::self()->mouse()->setPos(startPos);
    QCOMPARE(Cursors::self()->mouse()->pos(), startPos);

    // perform movement
    QFETCH(QPoint, targetPos);
    kwinApp()->platform()->pointerMotion(targetPos, 1);

    QFETCH(QPoint, expectedPos);
    QCOMPARE(Cursors::self()->mouse()->pos(), expectedPos);
}

void PointerInputTest::testResizeCursor_data()
{
    QTest::addColumn<Qt::Edges>("edges");
    QTest::addColumn<KWin::CursorShape>("cursorShape");

    QTest::newRow("top-left")     << Qt::Edges(Qt::TopEdge | Qt::LeftEdge)     << CursorShape(ExtendedCursor::SizeNorthWest);
    QTest::newRow("top")          << Qt::Edges(Qt::TopEdge)                    << CursorShape(ExtendedCursor::SizeNorth);
    QTest::newRow("top-right")    << Qt::Edges(Qt::TopEdge | Qt::RightEdge)    << CursorShape(ExtendedCursor::SizeNorthEast);
    QTest::newRow("right")        << Qt::Edges(Qt::RightEdge)                  << CursorShape(ExtendedCursor::SizeEast);
    QTest::newRow("bottom-right") << Qt::Edges(Qt::BottomEdge | Qt::RightEdge) << CursorShape(ExtendedCursor::SizeSouthEast);
    QTest::newRow("bottom")       << Qt::Edges(Qt::BottomEdge)                 << CursorShape(ExtendedCursor::SizeSouth);
    QTest::newRow("bottom-left")  << Qt::Edges(Qt::BottomEdge | Qt::LeftEdge)  << CursorShape(ExtendedCursor::SizeSouthWest);
    QTest::newRow("left")         << Qt::Edges(Qt::LeftEdge)                   << CursorShape(ExtendedCursor::SizeWest);
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

    // create a test client
    using namespace KWayland::Client;
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    AbstractClient *c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);

    // move the cursor to the test position
    QPoint cursorPos;
    QFETCH(Qt::Edges, edges);

    if (edges & Qt::LeftEdge) {
        cursorPos.setX(c->frameGeometry().left());
    } else if (edges & Qt::RightEdge) {
        cursorPos.setX(c->frameGeometry().right());
    } else {
        cursorPos.setX(c->frameGeometry().center().x());
    }

    if (edges & Qt::TopEdge) {
        cursorPos.setY(c->frameGeometry().top());
    } else if (edges & Qt::BottomEdge) {
        cursorPos.setY(c->frameGeometry().bottom());
    } else {
        cursorPos.setY(c->frameGeometry().center().y());
    }

    Cursors::self()->mouse()->setPos(cursorPos);

    const PlatformCursorImage arrowCursor = loadReferenceThemeCursor(Qt::ArrowCursor);
    QVERIFY(!arrowCursor.isNull());
    QCOMPARE(kwinApp()->platform()->cursorImage().image(), arrowCursor.image());
    QCOMPARE(kwinApp()->platform()->cursorImage().hotSpot(), arrowCursor.hotSpot());

    // start resizing the client
    int timestamp = 1;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    kwinApp()->platform()->pointerButtonPressed(BTN_RIGHT, timestamp++);
    QVERIFY(c->isResize());

    QFETCH(KWin::CursorShape, cursorShape);
    const PlatformCursorImage resizeCursor = loadReferenceThemeCursor(cursorShape);
    QVERIFY(!resizeCursor.isNull());
    QCOMPARE(kwinApp()->platform()->cursorImage().image(), resizeCursor.image());
    QCOMPARE(kwinApp()->platform()->cursorImage().hotSpot(), resizeCursor.hotSpot());

    // finish resizing the client
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    kwinApp()->platform()->pointerButtonReleased(BTN_RIGHT, timestamp++);
    QVERIFY(!c->isResize());

    QCOMPARE(kwinApp()->platform()->cursorImage().image(), arrowCursor.image());
    QCOMPARE(kwinApp()->platform()->cursorImage().hotSpot(), arrowCursor.hotSpot());
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

    // create a test client
    using namespace KWayland::Client;
    QScopedPointer<Surface> surface(Test::createSurface());
    QVERIFY(!surface.isNull());
    QScopedPointer<XdgShellSurface> shellSurface(Test::createXdgShellStableSurface(surface.data()));
    QVERIFY(!shellSurface.isNull());
    AbstractClient *c = Test::renderAndWaitForShown(surface.data(), QSize(100, 50), Qt::blue);
    QVERIFY(c);

    // move cursor to the test position
    Cursors::self()->mouse()->setPos(c->frameGeometry().center());

    const PlatformCursorImage arrowCursor = loadReferenceThemeCursor(Qt::ArrowCursor);
    QVERIFY(!arrowCursor.isNull());
    QCOMPARE(kwinApp()->platform()->cursorImage().image(), arrowCursor.image());
    QCOMPARE(kwinApp()->platform()->cursorImage().hotSpot(), arrowCursor.hotSpot());

    // start moving the client
    int timestamp = 1;
    kwinApp()->platform()->keyboardKeyPressed(KEY_LEFTMETA, timestamp++);
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(c->isMove());

    const PlatformCursorImage sizeAllCursor = loadReferenceThemeCursor(Qt::SizeAllCursor);
    QVERIFY(!sizeAllCursor.isNull());
    QCOMPARE(kwinApp()->platform()->cursorImage().image(), sizeAllCursor.image());
    QCOMPARE(kwinApp()->platform()->cursorImage().hotSpot(), sizeAllCursor.hotSpot());

    // finish moving the client
    kwinApp()->platform()->keyboardKeyReleased(KEY_LEFTMETA, timestamp++);
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(!c->isMove());

    QCOMPARE(kwinApp()->platform()->cursorImage().image(), arrowCursor.image());
    QCOMPARE(kwinApp()->platform()->cursorImage().hotSpot(), arrowCursor.hotSpot());
}

void PointerInputTest::testHideShowCursor()
{
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), false);
    kwinApp()->platform()->hideCursor();
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), true);
    kwinApp()->platform()->showCursor();
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), false);

    kwinApp()->platform()->hideCursor();
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), true);
    kwinApp()->platform()->hideCursor();
    kwinApp()->platform()->hideCursor();
    kwinApp()->platform()->hideCursor();
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), true);

    kwinApp()->platform()->showCursor();
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), true);
    kwinApp()->platform()->showCursor();
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), true);
    kwinApp()->platform()->showCursor();
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), true);
    kwinApp()->platform()->showCursor();
    QCOMPARE(kwinApp()->platform()->isCursorHidden(), false);
}

}

WAYLANDTEST_MAIN(KWin::PointerInputTest)
#include "pointer_input.moc"
