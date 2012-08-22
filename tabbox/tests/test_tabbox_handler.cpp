/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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
#include "mock_tabboxhandler.h"
#include "clientmodel.h"
#include <QtTest/QtTest>

using namespace KWin;

class TestTabBoxHandler : public QObject
{
    Q_OBJECT
private slots:
    /**
     * Test to verify that update outline does not crash
     * if the ModelIndex for which the outline should be
     * shown is not valid. That is accessing the Pointer
     * to the Client returns an invalid QVariant.
     * BUG: 304620
     **/
    void testDontCrashUpdateOutlineNullClient();
};

void TestTabBoxHandler::testDontCrashUpdateOutlineNullClient()
{
    MockTabBoxHandler tabboxhandler;
    TabBox::TabBoxConfig config;
    config.setTabBoxMode(TabBox::TabBoxConfig::ClientTabBox);
    config.setShowOutline(true);
    config.setShowTabBox(false);
    config.setHighlightWindows(false);
    tabboxhandler.setConfig(config);
    // now show the tabbox which will attempt to show the outline
    tabboxhandler.show();
}

QTEST_MAIN(TestTabBoxHandler)

#include "test_tabbox_handler.moc"
