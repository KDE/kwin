/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "keyboard_input.h"
#include "keyboard_layout.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KGlobalAccel>

#include <linux/input.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_keymap_creation_failure-0");

class KeymapCreationFailureTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testPointerButton();
};

void KeymapCreationFailureTest::initTestCase()
{
    // situation for for BUG 381210
    // this will fail to create keymap
    qputenv("XKB_DEFAULT_RULES", "no");
    qputenv("XKB_DEFAULT_MODEL", "no");
    qputenv("XKB_DEFAULT_LAYOUT", "no");
    qputenv("XKB_DEFAULT_VARIANT", "no");
    qputenv("XKB_DEFAULT_OPTIONS", "no");

    qRegisterMetaType<KWin::Window *>();
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));
    kwinApp()->setKxkbConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));
    KConfigGroup layoutGroup = kwinApp()->kxkbConfig()->group(QStringLiteral("Layout"));
    layoutGroup.writeEntry("LayoutList", QStringLiteral("no"));
    layoutGroup.writeEntry("Model", "no");
    layoutGroup.writeEntry("Options", "no");
    layoutGroup.sync();

    kwinApp()->start();
}

void KeymapCreationFailureTest::init()
{
    QVERIFY(Test::setupWaylandConnection());
}

void KeymapCreationFailureTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void KeymapCreationFailureTest::testPointerButton()
{
    // test case for BUG 381210
    // pressing a pointer button results in crash

    // now create the crashing condition
    // which is sending in a pointer event
    Test::pointerButtonPressed(BTN_LEFT, 0);
    Test::pointerButtonReleased(BTN_LEFT, 1);
}

WAYLANDTEST_MAIN(KeymapCreationFailureTest)
#include "keymap_creation_failure_test.moc"
