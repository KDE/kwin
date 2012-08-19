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
#include "test_tabbox_clientmodel.h"
#include "mock_tabboxhandler.h"
#include "clientmodel.h"

#include <QtTest/QtTest>
using namespace KWin;

void TestTabBoxClientModel::testLongestCaptionWithNullClient()
{
    MockTabBoxHandler tabboxhandler;
    TabBox::ClientModel *clientModel = new TabBox::ClientModel(&tabboxhandler);
    clientModel->createClientList();
    QCOMPARE(clientModel->longestCaption(), QString());
    // add a window to the mock
    tabboxhandler.createMockWindow(QString("test"), 1);
    clientModel->createClientList();
    QCOMPARE(clientModel->longestCaption(), QString("test"));
    // delete the one client in the list
    QModelIndex index = clientModel->index(0, 0);
    QVERIFY(index.isValid());
    TabBox::TabBoxClient *client = static_cast<TabBox::TabBoxClient *>(clientModel->data(index, TabBox::ClientModel::ClientRole).value<void*>());
    client->close();
    // internal model of ClientModel now contains a deleted pointer
    // longestCaption should behave just as if the window were not in the list
    QCOMPARE(clientModel->longestCaption(), QString());
}

void TestTabBoxClientModel::testCreateClientListNoActiveClient()
{
    MockTabBoxHandler tabboxhandler;
    tabboxhandler.setConfig(TabBox::TabBoxConfig());
    TabBox::ClientModel *clientModel = new TabBox::ClientModel(&tabboxhandler);
    clientModel->createClientList();
    QCOMPARE(clientModel->rowCount(), 0);
    // create two windows, rowCount() should go to two
    QWeakPointer<TabBox::TabBoxClient> client = tabboxhandler.createMockWindow(QString("test"), 1);
    tabboxhandler.createMockWindow(QString("test2"), 2);
    clientModel->createClientList();
    QCOMPARE(clientModel->rowCount(), 2);
    // let's ensure there is no active client
    tabboxhandler.setActiveClient(QWeakPointer<TabBox::TabBoxClient>());
    // now it should still have two members in the list
    clientModel->createClientList();
    QCOMPARE(clientModel->rowCount(), 2);
}

QTEST_MAIN(TestTabBoxClientModel)
