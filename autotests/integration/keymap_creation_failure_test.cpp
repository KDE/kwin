/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2017 Martin Flöser <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "kwin_wayland_test.h"
#include "keyboard_input.h"
#include "keyboard_layout.h"
#include "platform.h"
#include "shell_client.h"
#include "virtualdesktops.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KGlobalAccel>

#include <linux/input.h>

using namespace KWin;
using namespace KWayland::Client;

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

    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    kwinApp()->setConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));
    kwinApp()->setKxkbConfig(KSharedConfig::openConfig(QString(), KConfig::SimpleConfig));
    KConfigGroup layoutGroup = kwinApp()->kxkbConfig()->group("Layout");
    layoutGroup.writeEntry("LayoutList", QStringLiteral("no"));
    layoutGroup.writeEntry("Model", "no");
    layoutGroup.writeEntry("Options", "no");
    layoutGroup.sync();

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    waylandServer()->initWorkspace();
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
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, 0);
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, 1);
}

WAYLANDTEST_MAIN(KeymapCreationFailureTest)
#include "keymap_creation_failure_test.moc"
