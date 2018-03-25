/********************************************************************
Copyright 2016  Oleg Chernovskiy <kanedias@xaker.ru>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
// Qt
#include <QtTest/QtTest>
// client
#include "../../src/client/connection_thread.h"
#include "../../src/client/event_queue.h"
#include "../../src/client/remote_access.h"
#include "../../src/client/registry.h"
#include "../../src/client/output.h"
// server
#include "../../src/server/display.h"
#include "../../src/server/output_interface.h"
#include "../../src/server/remote_access_interface.h"

#include <linux/input.h>

using namespace KWayland::Client;
using namespace KWayland::Server;

Q_DECLARE_METATYPE(const BufferHandle *)
Q_DECLARE_METATYPE(const RemoteBuffer *)
Q_DECLARE_METATYPE(const void *)

class RemoteAccessTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();

    void testSendReleaseSingle();
    void testSendReleaseMultiple();
    void testSendClientGone();
    void testSendReceiveClientGone();

private:
    Display *m_display = nullptr;
    OutputInterface *m_outputInterface = nullptr;
    RemoteAccessManagerInterface *m_remoteAccessInterface = nullptr;
    ConnectionThread *m_connection = nullptr;
    QThread *m_thread = nullptr;
    EventQueue *m_queue = nullptr;
    Registry *m_registry = nullptr;
    Output *m_output = nullptr;
};

static const QString s_socketName = QStringLiteral("kwayland-test-remote-access-0");

void RemoteAccessTest::init()
{
    qRegisterMetaType<const BufferHandle *>();
    qRegisterMetaType<const RemoteBuffer *>();
    qRegisterMetaType<const void *>();

    delete m_display;
    m_display = new Display(this);
    m_display->setSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());
    m_display->createShm();
    m_outputInterface = m_display->createOutput();
    m_outputInterface->create();
    m_remoteAccessInterface = m_display->createRemoteAccessManager();
    m_remoteAccessInterface->create();
    QSignalSpy bufferReleasedSpy(m_remoteAccessInterface, &RemoteAccessManagerInterface::bufferReleased);
    QVERIFY(bufferReleasedSpy.isValid());

    // setup connection
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new EventQueue(this);
    m_queue->setup(m_connection);

    m_registry = new Registry(this);
    QSignalSpy interfacesAnnouncedSpy(m_registry, &Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    m_registry->setEventQueue(m_queue);
    m_registry->create(m_connection);
    QVERIFY(m_registry->isValid());
    m_registry->setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    // client-bound output
    m_output = m_registry->createOutput(m_registry->interface(Registry::Interface::Output).name,
                             m_registry->interface(Registry::Interface::Output).version,
                             this);
}

void RemoteAccessTest::cleanup()
{
#define CLEANUP(variable) \
    if (variable) { \
        delete variable; \
        variable = nullptr; \
    }
    CLEANUP(m_output)
    CLEANUP(m_queue)
    CLEANUP(m_registry)
    if (m_thread) {
        if (m_connection) {
            m_connection->flush();
            m_connection->deleteLater();
            m_connection = nullptr;
        }

        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }

    CLEANUP(m_remoteAccessInterface)
    CLEANUP(m_display)
#undef CLEANUP
}

void RemoteAccessTest::testSendReleaseSingle()
{
    // this test verifies that a buffer is sent to client and returned back

    // setup
    QVERIFY(!m_remoteAccessInterface->isBound());
    auto client = m_registry->createRemoteAccessManager(
                                            m_registry->interface(Registry::Interface::RemoteAccessManager).name,
                                            m_registry->interface(Registry::Interface::RemoteAccessManager).version,
                                            this);
    QVERIFY(client->isValid());
    m_connection->flush();
    m_display->dispatchEvents();

    QVERIFY(m_remoteAccessInterface->isBound()); // we have one client now
    QSignalSpy bufferReadySpy(client, &RemoteAccessManager::bufferReady);
    QVERIFY(bufferReadySpy.isValid());

    BufferHandle *buf = new BufferHandle();
    QTemporaryFile *tmpFile = new QTemporaryFile(this);
    tmpFile->open();

    buf->setFd(tmpFile->handle());
    buf->setSize(50, 50);
    buf->setFormat(100500);
    buf->setStride(7800);
    m_remoteAccessInterface->sendBufferReady(m_outputInterface, buf);

    // receive buffer
    QVERIFY(bufferReadySpy.wait());
    auto rbuf = bufferReadySpy.takeFirst()[1].value<const RemoteBuffer *>();
    QSignalSpy paramsObtainedSpy(rbuf, &RemoteBuffer::parametersObtained);
    QVERIFY(paramsObtainedSpy.isValid());

    // wait for params
    QVERIFY(paramsObtainedSpy.wait());
    // client fd is different, not subject to check
    QCOMPARE(rbuf->width(), 50u);
    QCOMPARE(rbuf->height(), 50u);
    QCOMPARE(rbuf->format(), 100500u);
    QCOMPARE(rbuf->stride(), 7800u);

    // release
    QSignalSpy bufferReleasedSpy(m_remoteAccessInterface, &RemoteAccessManagerInterface::bufferReleased);
    QVERIFY(bufferReleasedSpy.isValid());
    delete rbuf;
    QVERIFY(bufferReleasedSpy.wait());

    // cleanup
    delete buf;
    delete client;
    m_connection->flush();
    m_display->dispatchEvents();
    QVERIFY(!m_remoteAccessInterface->isBound());
}

void RemoteAccessTest::testSendReleaseMultiple()
{
    // this test verifies that a buffer is sent to 2 clients and returned back

    // setup
    QVERIFY(!m_remoteAccessInterface->isBound());
    auto client1 = m_registry->createRemoteAccessManager(
                                            m_registry->interface(Registry::Interface::RemoteAccessManager).name,
                                            m_registry->interface(Registry::Interface::RemoteAccessManager).version,
                                            this);
    QVERIFY(client1->isValid());
    auto client2 = m_registry->createRemoteAccessManager(
                                            m_registry->interface(Registry::Interface::RemoteAccessManager).name,
                                            m_registry->interface(Registry::Interface::RemoteAccessManager).version,
                                            this);
    QVERIFY(client2->isValid());
    m_connection->flush();
    m_display->dispatchEvents();

    QVERIFY(m_remoteAccessInterface->isBound()); // now we have 2 clients
    QSignalSpy bufferReadySpy1(client1, &RemoteAccessManager::bufferReady);
    QVERIFY(bufferReadySpy1.isValid());
    QSignalSpy bufferReadySpy2(client2, &RemoteAccessManager::bufferReady);
    QVERIFY(bufferReadySpy2.isValid());

    BufferHandle *buf = new BufferHandle();
    QTemporaryFile *tmpFile = new QTemporaryFile(this);
    tmpFile->open();

    buf->setFd(tmpFile->handle());
    buf->setSize(50, 50);
    buf->setFormat(100500);
    buf->setStride(7800);
    m_remoteAccessInterface->sendBufferReady(m_outputInterface, buf);

    // wait for event loop
    QVERIFY(bufferReadySpy1.wait());

    // receive buffer at client 1
    QCOMPARE(bufferReadySpy1.size(), 1);
    auto rbuf1 = bufferReadySpy1.takeFirst()[1].value<const RemoteBuffer *>();
    QSignalSpy paramsObtainedSpy1(rbuf1, &RemoteBuffer::parametersObtained);
    QVERIFY(paramsObtainedSpy1.isValid());

    // receive buffer at client 2
    QCOMPARE(bufferReadySpy2.size(), 1);
    auto rbuf2 = bufferReadySpy2.takeFirst()[1].value<const RemoteBuffer *>();
    QSignalSpy paramsObtainedSpy2(rbuf2, &RemoteBuffer::parametersObtained);
    QVERIFY(paramsObtainedSpy2.isValid());

    // wait for event loop
    QVERIFY(paramsObtainedSpy1.wait());
    QCOMPARE(paramsObtainedSpy1.size(), 1);
    QCOMPARE(paramsObtainedSpy2.size(), 1);

    // release
    QSignalSpy bufferReleasedSpy(m_remoteAccessInterface, &RemoteAccessManagerInterface::bufferReleased);
    QVERIFY(bufferReleasedSpy.isValid());
    delete rbuf1;
    QVERIFY(!bufferReleasedSpy.wait(1000)); // one client released, second still holds buffer!

    delete rbuf2;
    QVERIFY(bufferReleasedSpy.wait()); // all clients released, buffer should be freed

    // cleanup
    delete buf;
    delete client1;
    delete client2;
    m_connection->flush();
    m_display->dispatchEvents();
    QVERIFY(!m_remoteAccessInterface->isBound());
}

void RemoteAccessTest::testSendClientGone()
{
    // this test verifies that when buffer is sent and client is gone, server will release buffer correctly
    QVERIFY(!m_remoteAccessInterface->isBound());
    auto client = m_registry->createRemoteAccessManager(
                                            m_registry->interface(Registry::Interface::RemoteAccessManager).name,
                                            m_registry->interface(Registry::Interface::RemoteAccessManager).version,
                                            this);
    QVERIFY(client->isValid());
    m_connection->flush();
    m_display->dispatchEvents();

    QVERIFY(m_remoteAccessInterface->isBound()); // we have one client now
    QSignalSpy bufferReadySpy(client, &RemoteAccessManager::bufferReady);
    QVERIFY(bufferReadySpy.isValid());

    BufferHandle *buf = new BufferHandle();
    QTemporaryFile *tmpFile = new QTemporaryFile(this);
    tmpFile->open();

    buf->setFd(tmpFile->handle());
    buf->setSize(50, 50);
    buf->setFormat(100500);
    buf->setStride(7800);
    m_remoteAccessInterface->sendBufferReady(m_outputInterface, buf);

    // release forcefully
    QSignalSpy bufferReleasedSpy(m_remoteAccessInterface, &RemoteAccessManagerInterface::bufferReleased);
    QVERIFY(bufferReleasedSpy.isValid());
    delete client;
    QVERIFY(bufferReleasedSpy.wait());

    // cleanup
    delete buf;
    m_connection->flush();
    m_display->dispatchEvents();
    QVERIFY(!m_remoteAccessInterface->isBound());
}

void RemoteAccessTest::testSendReceiveClientGone()
{
    // this test verifies that when buffer is sent, received and client is gone, 
    // both client and server will release buffer correctly
    QVERIFY(!m_remoteAccessInterface->isBound());
    auto client = m_registry->createRemoteAccessManager(
                                            m_registry->interface(Registry::Interface::RemoteAccessManager).name,
                                            m_registry->interface(Registry::Interface::RemoteAccessManager).version,
                                            this);
    QVERIFY(client->isValid());
    m_connection->flush();
    m_display->dispatchEvents();

    QVERIFY(m_remoteAccessInterface->isBound()); // we have one client now
    QSignalSpy bufferReadySpy(client, &RemoteAccessManager::bufferReady);
    QVERIFY(bufferReadySpy.isValid());

    BufferHandle *buf = new BufferHandle();
    QTemporaryFile *tmpFile = new QTemporaryFile(this);
    tmpFile->open();

    buf->setFd(tmpFile->handle());
    buf->setSize(50, 50);
    buf->setFormat(100500);
    buf->setStride(7800);
    m_remoteAccessInterface->sendBufferReady(m_outputInterface, buf);

    // receive buffer
    QVERIFY(bufferReadySpy.wait());
    auto rbuf = bufferReadySpy.takeFirst()[1].value<const RemoteBuffer *>();
    QSignalSpy paramsObtainedSpy(rbuf, &RemoteBuffer::parametersObtained);
    QVERIFY(paramsObtainedSpy.isValid());

    // wait for params
    QVERIFY(paramsObtainedSpy.wait());
    // client fd is different, not subject to check
    QCOMPARE(rbuf->width(), 50u);
    QCOMPARE(rbuf->height(), 50u);
    QCOMPARE(rbuf->format(), 100500u);
    QCOMPARE(rbuf->stride(), 7800u);

    // release forcefully
    QSignalSpy bufferReleasedSpy(m_remoteAccessInterface, &RemoteAccessManagerInterface::bufferReleased);
    QVERIFY(bufferReleasedSpy.isValid());
    delete client;
    QVERIFY(bufferReleasedSpy.wait());

    // cleanup
    delete buf;
    m_connection->flush();
    m_display->dispatchEvents();
    QVERIFY(!m_remoteAccessInterface->isBound());
}


QTEST_GUILESS_MAIN(RemoteAccessTest)
#include "test_remote_access.moc"
