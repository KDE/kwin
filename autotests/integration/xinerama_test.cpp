/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "core/output.h"
#include "wayland_server.h"
#include "workspace.h"

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_xinerama-0");

class XineramaTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void indexToOutput();
};

void XineramaTest::initTestCase()
{
    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->start();
}

void XineramaTest::indexToOutput()
{
    Test::setOutputConfig({
        Test::OutputInfo{
            .geometry = QRect(0, 0, 1280, 1024),
            .scale = 1.5,
        },
        Test::OutputInfo{
            .geometry = QRect(1280, 0, 1280, 1024),
            .scale = 1.5,
        },
    });
    kwinApp()->setXwaylandScale(1.5);

    // Start Xwayland
    Test::XcbConnectionPtr c = Test::createX11Connection();
    QVERIFY(!xcb_connection_has_error(c.get()));

    const auto outputs = workspace()->outputs();
    QCOMPARE(workspace()->xineramaIndexToOutput(0), outputs.at(0));
    QCOMPARE(workspace()->xineramaIndexToOutput(1), outputs.at(1));

    workspace()->setOutputOrder({outputs[1], outputs[0]});
    QCOMPARE(workspace()->xineramaIndexToOutput(0), outputs.at(1));
    QCOMPARE(workspace()->xineramaIndexToOutput(1), outputs.at(0));
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::XineramaTest)
#include "xinerama_test.moc"
