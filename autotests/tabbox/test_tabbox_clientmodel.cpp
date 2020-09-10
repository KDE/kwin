/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2012 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "test_tabbox_clientmodel.h"
#include "mock_tabboxhandler.h"
#include "clientmodel.h"
#include "../testutils.h"

#include <QtTest>
#include <QX11Info>
using namespace KWin;

void TestTabBoxClientModel::initTestCase()
{
    qApp->setProperty("x11Connection", QVariant::fromValue<void*>(QX11Info::connection()));
}

void TestTabBoxClientModel::testLongestCaptionWithNullClient()
{
    MockTabBoxHandler tabboxhandler;
    TabBox::ClientModel *clientModel = new TabBox::ClientModel(&tabboxhandler);
    clientModel->createClientList();
    QCOMPARE(clientModel->longestCaption(), QString());
    // add a window to the mock
    tabboxhandler.createMockWindow(QString("test"));
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
    QWeakPointer<TabBox::TabBoxClient> client = tabboxhandler.createMockWindow(QString("test"));
    tabboxhandler.createMockWindow(QString("test2"));
    clientModel->createClientList();
    QCOMPARE(clientModel->rowCount(), 2);
    // let's ensure there is no active client
    tabboxhandler.setActiveClient(QWeakPointer<TabBox::TabBoxClient>());
    // now it should still have two members in the list
    clientModel->createClientList();
    QCOMPARE(clientModel->rowCount(), 2);
}

void TestTabBoxClientModel::testCreateClientListActiveClientNotInFocusChain()
{
    MockTabBoxHandler tabboxhandler;
    tabboxhandler.setConfig(TabBox::TabBoxConfig());
    TabBox::ClientModel *clientModel = new TabBox::ClientModel(&tabboxhandler);
    // create two windows, rowCount() should go to two
    QWeakPointer<TabBox::TabBoxClient> client = tabboxhandler.createMockWindow(QString("test"));
    client = tabboxhandler.createMockWindow(QString("test2"));
    clientModel->createClientList();
    QCOMPARE(clientModel->rowCount(), 2);

    // simulate that the active client is not in the focus chain
    // for that we use the closeWindow of the MockTabBoxHandler which
    // removes the Client from the Focus Chain but leaves the active window as it is
    QSharedPointer<TabBox::TabBoxClient> clientOwner = client.toStrongRef();
    tabboxhandler.closeWindow(clientOwner.data());
    clientModel->createClientList();
    QCOMPARE(clientModel->rowCount(), 1);
}

Q_CONSTRUCTOR_FUNCTION(forceXcb)
QTEST_MAIN(TestTabBoxClientModel)
