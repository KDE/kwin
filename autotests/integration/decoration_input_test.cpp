/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "core/output.h"
#include "cursor.h"
#include "internalwindow.h"
#include "pointer_input.h"
#include "touch_input.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include "decorations/decoratedwindow.h"
#include "decorations/decorationbridge.h"
#include "decorations/settings.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationSettings>

#include <QSignalSpy>

#include <linux/input.h>

Q_DECLARE_METATYPE(Qt::WindowFrameSection)

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_decoration_input-0");

class DecorationInputTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testAxis_data();
    void testAxis();
    void testDoubleClickOnAllDesktops();
    void testDoubleClickClose();
    void testDoubleTap();
    void testHover();
    void testPressToMove_data();
    void testPressToMove();
    void testTapToMove_data();
    void testTapToMove();
    void testResizeOutsideWindow_data();
    void testResizeOutsideWindow();
    void testModifierClickUnrestrictedMove_data();
    void testModifierClickUnrestrictedMove();
    void testModifierScrollOpacity_data();
    void testModifierScrollOpacity();
    void testTouchEvents();
    void testTooltipDoesntEatKeyEvents();

private:
    std::tuple<Window *, std::unique_ptr<KWayland::Client::Surface>, std::unique_ptr<Test::XdgToplevel>, std::unique_ptr<Test::XdgToplevelDecorationV1>> showWindow();
};

#define MOTION(target) Test::pointerMotion(target, timestamp++)

#define PRESS Test::pointerButtonPressed(BTN_LEFT, timestamp++)

#define RELEASE Test::pointerButtonReleased(BTN_LEFT, timestamp++)

std::tuple<Window *, std::unique_ptr<KWayland::Client::Surface>, std::unique_ptr<Test::XdgToplevel>, std::unique_ptr<Test::XdgToplevelDecorationV1>> DecorationInputTest::showWindow()
{
#define VERIFY(statement)                                                 \
    if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__)) \
        return {nullptr, nullptr, nullptr, nullptr};
#define COMPARE(actual, expected)                                                   \
    if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__)) \
        return {nullptr, nullptr, nullptr, nullptr};

    std::unique_ptr<KWayland::Client::Surface> surface{Test::createSurface()};
    VERIFY(surface.get());
    std::unique_ptr<Test::XdgToplevel> shellSurface = Test::createXdgToplevelSurface(surface.get(), Test::CreationSetup::CreateOnly);
    VERIFY(shellSurface.get());
    std::unique_ptr<Test::XdgToplevelDecorationV1> decoration = Test::createXdgToplevelDecorationV1(shellSurface.get());
    VERIFY(decoration.get());

    QSignalSpy decorationConfigureRequestedSpy(decoration.get(), &Test::XdgToplevelDecorationV1::configureRequested);
    QSignalSpy surfaceConfigureRequestedSpy(shellSurface->xdgSurface(), &Test::XdgSurface::configureRequested);

    decoration->set_mode(Test::XdgToplevelDecorationV1::mode_server_side);
    surface->commit(KWayland::Client::Surface::CommitFlag::None);
    VERIFY(surfaceConfigureRequestedSpy.wait());
    COMPARE(decorationConfigureRequestedSpy.last().at(0).value<Test::XdgToplevelDecorationV1::mode>(), Test::XdgToplevelDecorationV1::mode_server_side);

    // let's render
    shellSurface->xdgSurface()->ack_configure(surfaceConfigureRequestedSpy.last().at(0).value<quint32>());
    auto window = Test::renderAndWaitForShown(surface.get(), QSize(500, 50), Qt::blue);
    VERIFY(window);
    COMPARE(workspace()->activeWindow(), window);

#undef VERIFY
#undef COMPARE

    return {window, std::move(surface), std::move(shellSurface), std::move(decoration)};
}

void DecorationInputTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    qRegisterMetaType<KWin::InternalWindow *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    // change some options
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group(QStringLiteral("MouseBindings")).writeEntry("CommandTitlebarWheel", QStringLiteral("above/below"));
    config->group(QStringLiteral("Windows")).writeEntry("TitlebarDoubleClickCommand", QStringLiteral("OnAllDesktops"));
    config->group(QStringLiteral("Desktops")).writeEntry("Number", 2);
    config->sync();

    kwinApp()->setConfig(config);

    kwinApp()->start();
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
}

void DecorationInputTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::XdgDecorationV1));
    QVERIFY(Test::waitForWaylandPointer());

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
}

void DecorationInputTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void DecorationInputTest::testAxis_data()
{
    QTest::addColumn<QPoint>("decoPoint");
    QTest::addColumn<Qt::WindowFrameSection>("expectedSection");

    QTest::newRow("topLeft") << QPoint(0, 0) << Qt::TopLeftSection;
    QTest::newRow("top") << QPoint(250, 0) << Qt::TopSection;
    QTest::newRow("topRight") << QPoint(499, 0) << Qt::TopRightSection;
}

void DecorationInputTest::testAxis()
{
    static constexpr double oneTick = 15;

    const auto [window, surface, shellSurface, decoration] = showWindow();
    QVERIFY(window);
    QVERIFY(window->isDecorated());
    QVERIFY(!window->noBorder());
    QCOMPARE(window->titlebarPosition(), Qt::TopEdge);
    QVERIFY(!window->keepAbove());
    QVERIFY(!window->keepBelow());

    quint32 timestamp = 1;
    MOTION(QPoint(window->frameGeometry().center().x(), window->frameMargins().top() / 2.0));
    QVERIFY(input()->pointer()->decoration());
    QCOMPARE(input()->pointer()->decoration()->decoration()->sectionUnderMouse(), Qt::TitleBarArea);

    // TODO: mouse wheel direction looks wrong to me
    // simulate wheel
    Test::pointerAxisVertical(oneTick, timestamp++);
    QVERIFY(window->keepBelow());
    QVERIFY(!window->keepAbove());
    Test::pointerAxisVertical(-oneTick, timestamp++);
    QVERIFY(!window->keepBelow());
    QVERIFY(!window->keepAbove());
    Test::pointerAxisVertical(-oneTick, timestamp++);
    QVERIFY(!window->keepBelow());
    QVERIFY(window->keepAbove());

    // test top most deco pixel, BUG: 362860
    window->move(QPoint(0, 0));
    QFETCH(QPoint, decoPoint);
    MOTION(decoPoint);
    QVERIFY(input()->pointer()->decoration());
    QCOMPARE(input()->pointer()->decoration()->window(), window);
    QTEST(input()->pointer()->decoration()->decoration()->sectionUnderMouse(), "expectedSection");
    Test::pointerAxisVertical(oneTick, timestamp++);
    QVERIFY(!window->keepBelow());
    QVERIFY(!window->keepAbove());
}

void KWin::DecorationInputTest::testDoubleClickOnAllDesktops()
{
    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("Windows"));
    group.writeEntry("TitlebarDoubleClickCommand", QStringLiteral("OnAllDesktops"));
    group.sync();
    workspace()->slotReconfigure();

    const auto [window, surface, shellSurface, decoration] = showWindow();
    QVERIFY(window);
    QVERIFY(window->isDecorated());
    QVERIFY(!window->noBorder());
    QVERIFY(!window->isOnAllDesktops());
    quint32 timestamp = 1;
    MOTION(QPoint(window->frameGeometry().center().x(), window->frameMargins().top() / 2.0));

    // double click
    PRESS;
    RELEASE;
    PRESS;
    RELEASE;
    QVERIFY(window->isOnAllDesktops());
    // double click again
    PRESS;
    RELEASE;
    QVERIFY(window->isOnAllDesktops());
    PRESS;
    RELEASE;
    QVERIFY(!window->isOnAllDesktops());
}

void DecorationInputTest::testDoubleClickClose()
{
    // this test verifies that no crash occurs when double click is configured to close action
    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("Windows"));
    group.writeEntry("TitlebarDoubleClickCommand", QStringLiteral("Close"));
    group.sync();
    workspace()->slotReconfigure();

    auto [window, surface, shellSurface, decoration] = showWindow();
    QVERIFY(window);
    QVERIFY(window->isDecorated());
    quint32 timestamp = 1;
    MOTION(QPoint(window->frameGeometry().center().x(), window->frameMargins().top() / 2.0));

    connect(shellSurface.get(), &Test::XdgToplevel::closeRequested, this, [&surface = surface]() {
        surface.reset();
    });

    // double click
    QSignalSpy closedSpy(window, &Window::closed);
    window->ref();
    PRESS;
    RELEASE;
    PRESS;
    QVERIFY(closedSpy.wait());
    RELEASE;

    QVERIFY(window->isDeleted());
    window->unref();
}

void KWin::DecorationInputTest::testDoubleTap()
{
    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("Windows"));
    group.writeEntry("TitlebarDoubleClickCommand", QStringLiteral("OnAllDesktops"));
    group.sync();
    workspace()->slotReconfigure();

    const auto [window, surface, shellSurface, decoration] = showWindow();
    QVERIFY(window);
    QVERIFY(window->isDecorated());
    QVERIFY(!window->noBorder());
    QVERIFY(!window->isOnAllDesktops());
    quint32 timestamp = 1;
    const QPoint tapPoint(window->frameGeometry().center().x(), window->frameMargins().top() / 2.0);

    // double tap
    Test::touchDown(0, tapPoint, timestamp++);
    Test::touchUp(0, timestamp++);
    Test::touchDown(0, tapPoint, timestamp++);
    Test::touchUp(0, timestamp++);
    QVERIFY(window->isOnAllDesktops());
    // double tap again
    Test::touchDown(0, tapPoint, timestamp++);
    Test::touchUp(0, timestamp++);
    QVERIFY(window->isOnAllDesktops());
    Test::touchDown(0, tapPoint, timestamp++);
    Test::touchUp(0, timestamp++);
    QVERIFY(!window->isOnAllDesktops());
}

void DecorationInputTest::testHover()
{
    const auto [window, surface, shellSurface, decoration] = showWindow();
    QVERIFY(window);
    QVERIFY(window->isDecorated());
    QVERIFY(!window->noBorder());

    // our left border is moved out of the visible area, so move the window to a better place
    window->move(QPoint(20, 0));

    quint32 timestamp = 1;
    MOTION(QPoint(window->frameGeometry().center().x(), window->frameMargins().top() / 2.0));
    QCOMPARE(window->cursor(), CursorShape(Qt::ArrowCursor));

    // There is a mismatch of the cursor key positions between windows
    // with and without borders (with borders one can move inside a bit and still
    // be on an edge, without not). We should make this consistent in KWin's core.
    //
    // TODO: Test input position with different border sizes.
    // TODO: We should test with the fake decoration to have a fixed test environment.
    const bool hasBorders = Workspace::self()->decorationBridge()->settings()->borderSize() != KDecoration3::BorderSize::None;
    auto deviation = [hasBorders] {
        return hasBorders ? -1 : 0;
    };

    MOTION(QPoint(window->frameGeometry().x(), 0));
    QCOMPARE(window->cursor(), CursorShape(KWin::ExtendedCursor::SizeNorthWest));
    MOTION(QPoint(window->frameGeometry().x() + window->frameGeometry().width() / 2, 0));
    QCOMPARE(window->cursor(), CursorShape(KWin::ExtendedCursor::SizeNorth));
    MOTION(QPoint(window->frameGeometry().x() + window->frameGeometry().width() - 1, 0));
    QCOMPARE(window->cursor(), CursorShape(KWin::ExtendedCursor::SizeNorthEast));
    MOTION(QPoint(window->frameGeometry().x() + window->frameGeometry().width() + deviation(), window->height() / 2));
    QCOMPARE(window->cursor(), CursorShape(KWin::ExtendedCursor::SizeEast));
    MOTION(QPoint(window->frameGeometry().x() + window->frameGeometry().width() + deviation(), window->height() - 1));
    QCOMPARE(window->cursor(), CursorShape(KWin::ExtendedCursor::SizeSouthEast));
    MOTION(QPoint(window->frameGeometry().x() + window->frameGeometry().width() / 2, window->height() + deviation()));
    QCOMPARE(window->cursor(), CursorShape(KWin::ExtendedCursor::SizeSouth));
    MOTION(QPoint(window->frameGeometry().x(), window->height() + deviation()));
    QCOMPARE(window->cursor(), CursorShape(KWin::ExtendedCursor::SizeSouthWest));
    MOTION(QPoint(window->frameGeometry().x() - 1, window->height() / 2));
    QCOMPARE(window->cursor(), CursorShape(KWin::ExtendedCursor::SizeWest));

    MOTION(window->frameGeometry().center());
    QEXPECT_FAIL("", "Cursor not set back on leave", Continue);
    QCOMPARE(window->cursor(), CursorShape(Qt::ArrowCursor));
}

void DecorationInputTest::testPressToMove_data()
{
    QTest::addColumn<QPoint>("offset");
    QTest::addColumn<QPoint>("offset2");
    QTest::addColumn<QPoint>("offset3");

    QTest::newRow("To right") << QPoint(10, 0) << QPoint(20, 0) << QPoint(30, 0);
    QTest::newRow("To left") << QPoint(-10, 0) << QPoint(-20, 0) << QPoint(-30, 0);
    QTest::newRow("To bottom") << QPoint(0, 10) << QPoint(0, 20) << QPoint(0, 30);
    QTest::newRow("To top") << QPoint(0, -10) << QPoint(0, -20) << QPoint(0, -30);
}

void DecorationInputTest::testPressToMove()
{
    const auto [window, surface, shellSurface, decoration] = showWindow();
    QVERIFY(window);
    QVERIFY(window->isDecorated());
    QVERIFY(!window->noBorder());
    window->move(workspace()->activeOutput()->geometry().center() - QPoint(window->width() / 2, window->height() / 2));
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);

    quint32 timestamp = 1;
    MOTION(QPoint(window->frameGeometry().center().x(), window->y() + window->frameMargins().top() / 2.0));
    QCOMPARE(window->cursor(), CursorShape(Qt::ArrowCursor));

    PRESS;
    QVERIFY(!window->isInteractiveMove());
    QFETCH(QPoint, offset);
    MOTION(QPoint(window->frameGeometry().center().x(), window->y() + window->frameMargins().top() / 2.0) + offset);
    const QPointF oldPos = window->pos();
    QVERIFY(window->isInteractiveMove());
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);

    RELEASE;
    QTRY_VERIFY(!window->isInteractiveMove());
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
    QEXPECT_FAIL("", "Just trigger move doesn't move the window", Continue);
    QCOMPARE(window->pos(), oldPos + offset);

    // again
    PRESS;
    QVERIFY(!window->isInteractiveMove());
    QFETCH(QPoint, offset2);
    MOTION(QPoint(window->frameGeometry().center().x(), window->y() + window->frameMargins().top() / 2.0) + offset2);
    QVERIFY(window->isInteractiveMove());
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 2);
    QFETCH(QPoint, offset3);
    MOTION(QPoint(window->frameGeometry().center().x(), window->y() + window->frameMargins().top() / 2.0) + offset3);

    RELEASE;
    QTRY_VERIFY(!window->isInteractiveMove());
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 2);
    // TODO: the offset should also be included
    QCOMPARE(window->pos(), oldPos + offset2 + offset3);
}

void DecorationInputTest::testTapToMove_data()
{
    QTest::addColumn<QPoint>("offset");
    QTest::addColumn<QPoint>("offset2");
    QTest::addColumn<QPoint>("offset3");

    QTest::newRow("To right") << QPoint(10, 0) << QPoint(20, 0) << QPoint(30, 0);
    QTest::newRow("To left") << QPoint(-10, 0) << QPoint(-20, 0) << QPoint(-30, 0);
    QTest::newRow("To bottom") << QPoint(0, 10) << QPoint(0, 20) << QPoint(0, 30);
    QTest::newRow("To top") << QPoint(0, -10) << QPoint(0, -20) << QPoint(0, -30);
}

void DecorationInputTest::testTapToMove()
{
    const auto [window, surface, shellSurface, decoration] = showWindow();
    QVERIFY(window);
    QVERIFY(window->isDecorated());
    QVERIFY(!window->noBorder());
    window->move(workspace()->activeOutput()->geometry().center() - QPoint(window->width() / 2, window->height() / 2));
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);
    QSignalSpy interactiveMoveResizeFinishedSpy(window, &Window::interactiveMoveResizeFinished);

    quint32 timestamp = 1;
    QPoint p = QPoint(window->frameGeometry().center().x(), window->y() + window->frameMargins().top() / 2.0);

    Test::touchDown(0, p, timestamp++);
    QVERIFY(!window->isInteractiveMove());
    QFETCH(QPoint, offset);
    QCOMPARE(input()->touch()->decorationPressId(), 0);
    Test::touchMotion(0, p + offset, timestamp++);
    const QPointF oldPos = window->pos();
    QVERIFY(window->isInteractiveMove());
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 1);

    Test::touchUp(0, timestamp++);
    QTRY_VERIFY(!window->isInteractiveMove());
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 1);
    QEXPECT_FAIL("", "Just trigger move doesn't move the window", Continue);
    QCOMPARE(window->pos(), oldPos + offset);

    // again
    Test::touchDown(1, p + offset, timestamp++);
    QCOMPARE(input()->touch()->decorationPressId(), 1);
    QVERIFY(!window->isInteractiveMove());
    QFETCH(QPoint, offset2);
    Test::touchMotion(1, QPoint(window->frameGeometry().center().x(), window->y() + window->frameMargins().top() / 2.0) + offset2, timestamp++);
    QVERIFY(window->isInteractiveMove());
    QCOMPARE(interactiveMoveResizeStartedSpy.count(), 2);
    QFETCH(QPoint, offset3);
    Test::touchMotion(1, QPoint(window->frameGeometry().center().x(), window->y() + window->frameMargins().top() / 2.0) + offset3, timestamp++);

    Test::touchUp(1, timestamp++);
    QTRY_VERIFY(!window->isInteractiveMove());
    QCOMPARE(interactiveMoveResizeFinishedSpy.count(), 2);
    // TODO: the offset should also be included
    QCOMPARE(window->pos(), oldPos + offset2 + offset3);
}

void DecorationInputTest::testResizeOutsideWindow_data()
{
    QTest::addColumn<Qt::Edge>("edge");
    QTest::addColumn<Qt::CursorShape>("expectedCursor");

    QTest::newRow("left") << Qt::LeftEdge << Qt::SizeHorCursor;
    QTest::newRow("right") << Qt::RightEdge << Qt::SizeHorCursor;
    QTest::newRow("bottom") << Qt::BottomEdge << Qt::SizeVerCursor;
}

void DecorationInputTest::testResizeOutsideWindow()
{
    // this test verifies that one can resize the window outside the decoration with NoSideBorder

    // first adjust config
    kwinApp()->config()->group(QStringLiteral("org.kde.kdecoration2")).writeEntry("BorderSize", QStringLiteral("None"));
    kwinApp()->config()->sync();
    workspace()->slotReconfigure();

    // now create window
    const auto [window, surface, shellSurface, decoration] = showWindow();
    QVERIFY(window);
    QVERIFY(window->isDecorated());
    QVERIFY(!window->noBorder());
    window->move(workspace()->activeOutput()->geometry().center() - QPoint(window->width() / 2, window->height() / 2));
    QSignalSpy interactiveMoveResizeStartedSpy(window, &Window::interactiveMoveResizeStarted);

    // go to border
    quint32 timestamp = 1;
    QFETCH(Qt::Edge, edge);
    switch (edge) {
    case Qt::LeftEdge:
        MOTION(QPoint(window->frameGeometry().x() - 1, window->frameGeometry().center().y()));
        break;
    case Qt::RightEdge:
        MOTION(QPoint(window->frameGeometry().x() + window->frameGeometry().width() + 1, window->frameGeometry().center().y()));
        break;
    case Qt::BottomEdge:
        MOTION(QPoint(window->frameGeometry().center().x(), window->frameGeometry().y() + window->frameGeometry().height() + 1));
        break;
    default:
        break;
    }
    QVERIFY(!exclusiveContains(window->frameGeometry(), KWin::Cursors::self()->mouse()->pos()));

    // pressing should trigger resize
    PRESS;
    QVERIFY(!window->isInteractiveResize());
    QVERIFY(interactiveMoveResizeStartedSpy.wait());
    QVERIFY(window->isInteractiveResize());

    RELEASE;
    QVERIFY(!window->isInteractiveResize());
}

void DecorationInputTest::testModifierClickUnrestrictedMove_data()
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

void DecorationInputTest::testModifierClickUnrestrictedMove()
{
    // this test ensures that Alt+mouse button press triggers unrestricted move

    // first modify the config for this run
    QFETCH(QString, modKey);
    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("MouseBindings"));
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
    const auto [window, surface, shellSurface, decoration] = showWindow();
    QVERIFY(window);
    QVERIFY(window->isDecorated());
    QVERIFY(!window->noBorder());
    window->move(workspace()->activeOutput()->geometry().center() - QPoint(window->width() / 2, window->height() / 2));
    // move cursor on window
    input()->pointer()->warp(QPoint(window->frameGeometry().center().x(), window->y() + window->frameMargins().top() / 2.0));

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
}

void DecorationInputTest::testModifierScrollOpacity_data()
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

void DecorationInputTest::testModifierScrollOpacity()
{
    // this test verifies that mod+wheel performs a window operation

    // first modify the config for this run
    QFETCH(QString, modKey);
    KConfigGroup group = kwinApp()->config()->group(QStringLiteral("MouseBindings"));
    group.writeEntry("CommandAllKey", modKey);
    group.writeEntry("CommandAllWheel", "change opacity");
    group.sync();
    workspace()->slotReconfigure();

    const auto [window, surface, shellSurface, decoration] = showWindow();
    QVERIFY(window);
    QVERIFY(window->isDecorated());
    QVERIFY(!window->noBorder());
    window->move(workspace()->activeOutput()->geometry().center() - QPoint(window->width() / 2, window->height() / 2));
    // move cursor on window
    input()->pointer()->warp(QPoint(window->frameGeometry().center().x(), window->y() + window->frameMargins().top() / 2.0));
    // set the opacity to 0.5
    window->setOpacity(0.5);
    QCOMPARE(window->opacity(), 0.5);

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
}

class EventHelper : public QObject
{
    Q_OBJECT
public:
    EventHelper()
        : QObject()
    {
    }
    ~EventHelper() override = default;

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::HoverMove) {
            Q_EMIT hoverMove();
        } else if (event->type() == QEvent::HoverLeave) {
            Q_EMIT hoverLeave();
        }
        return false;
    }

Q_SIGNALS:
    void hoverMove();
    void hoverLeave();
};

void DecorationInputTest::testTouchEvents()
{
    // this test verifies that the decoration gets a hover leave event on touch release
    // see BUG 386231
    const auto [window, surface, shellSurface, decoration] = showWindow();
    QVERIFY(window);
    QVERIFY(window->isDecorated());
    QVERIFY(!window->noBorder());

    EventHelper helper;
    window->decoration()->installEventFilter(&helper);
    QSignalSpy hoverMoveSpy(&helper, &EventHelper::hoverMove);
    QSignalSpy hoverLeaveSpy(&helper, &EventHelper::hoverLeave);

    quint32 timestamp = 1;
    const QPoint tapPoint(window->frameGeometry().center().x(), window->frameMargins().top() / 2.0);

    QVERIFY(!input()->touch()->decoration());
    Test::touchDown(0, tapPoint, timestamp++);
    QVERIFY(input()->touch()->decoration());
    QCOMPARE(input()->touch()->decoration()->decoration(), window->decoration());
    QCOMPARE(hoverMoveSpy.count(), 1);
    QCOMPARE(hoverLeaveSpy.count(), 0);
    Test::touchUp(0, timestamp++);
    QCOMPARE(hoverMoveSpy.count(), 1);
    QCOMPARE(hoverLeaveSpy.count(), 1);

    QCOMPARE(window->isInteractiveMove(), false);

    // let's check that a hover motion is sent if the pointer is on deco, when touch release
    input()->pointer()->warp(tapPoint);
    QCOMPARE(hoverMoveSpy.count(), 2);
    Test::touchDown(0, tapPoint, timestamp++);
    QCOMPARE(hoverMoveSpy.count(), 3);
    QCOMPARE(hoverLeaveSpy.count(), 1);
    Test::touchUp(0, timestamp++);
    QCOMPARE(hoverMoveSpy.count(), 3);
    QCOMPARE(hoverLeaveSpy.count(), 2);
}

void DecorationInputTest::testTooltipDoesntEatKeyEvents()
{
    // this test verifies that a tooltip on the decoration does not steal key events
    // BUG: 393253

    // first create a keyboard
    auto keyboard = Test::waylandSeat()->createKeyboard(Test::waylandSeat());
    QVERIFY(keyboard);
    QSignalSpy enteredSpy(keyboard, &KWayland::Client::Keyboard::entered);

    const auto [window, surface, shellSurface, decoration] = showWindow();
    QVERIFY(window);
    QVERIFY(window->isDecorated());
    QVERIFY(!window->noBorder());
    QVERIFY(enteredSpy.wait());

    QSignalSpy keyEvent(keyboard, &KWayland::Client::Keyboard::keyChanged);
    QVERIFY(keyEvent.isValid());

    QSignalSpy windowAddedSpy(workspace(), &Workspace::windowAdded);
    window->decoratedWindow()->requestShowToolTip(QStringLiteral("test"));
    // now we should get an internal window
    QVERIFY(windowAddedSpy.wait());
    InternalWindow *internal = windowAddedSpy.first().first().value<InternalWindow *>();
    QVERIFY(internal->isInternal());
    QVERIFY(internal->handle()->flags().testFlag(Qt::ToolTip));

    // now send a key
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_A, timestamp++);
    QVERIFY(keyEvent.wait());
    Test::keyboardKeyReleased(KEY_A, timestamp++);
    QVERIFY(keyEvent.wait());

    window->decoratedWindow()->requestHideToolTip();
    Test::waitForWindowClosed(internal);
}

}

WAYLANDTEST_MAIN(KWin::DecorationInputTest)
#include "decoration_input_test.moc"
