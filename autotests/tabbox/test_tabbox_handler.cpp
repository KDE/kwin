/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "mock_tabboxhandler.h"
#include "clientmodel.h"
#include "../testutils.h"
#include <QtTest>
#include <QX11Info>

using namespace KWin;

class TestTabBoxHandler : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    /**
     * Test to verify that update outline does not crash
     * if the ModelIndex for which the outline should be
     * shown is not valid. That is accessing the Pointer
     * to the Client returns an invalid QVariant.
     * BUG: 304620
     */
    void testDontCrashUpdateOutlineNullClient();
};

void TestTabBoxHandler::initTestCase()
{
    qApp->setProperty("x11Connection", QVariant::fromValue<void*>(QX11Info::connection()));
}

void TestTabBoxHandler::testDontCrashUpdateOutlineNullClient()
{
    MockTabBoxHandler tabboxhandler;
    TabBox::TabBoxConfig config;
    config.setTabBoxMode(TabBox::TabBoxConfig::ClientTabBox);
    config.setShowTabBox(false);
    config.setHighlightWindows(false);
    tabboxhandler.setConfig(config);
    // now show the tabbox which will attempt to show the outline
    tabboxhandler.show();
}

Q_CONSTRUCTOR_FUNCTION(forceXcb)
QTEST_MAIN(TestTabBoxHandler)

#include "test_tabbox_handler.moc"
