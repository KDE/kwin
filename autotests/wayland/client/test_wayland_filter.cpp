/*
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QSignalSpy>
#include <QTest>
// KWin
#include "wayland/clientconnection.h"
#include "wayland/compositor.h"
#include "wayland/display.h"
#include "wayland/filtered_display.h"
#include "wayland/kde_blur.h"

#include "KWayland/Client/blur.h"
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/region.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/surface.h"

#include <wayland-server.h>

class TestDisplay;

class TestFilter : public QObject
{
    Q_OBJECT
public:
    explicit TestFilter(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();
    void testFilter_data();
    void testFilter();

private:
    TestDisplay *m_display;
    KWin::CompositorInterface *m_compositorInterface;
    KWin::BlurManagerInterface *m_blurManagerInterface;
};

static const QString s_socketName = QStringLiteral("kwayland-test-wayland-blur-0");

// The following non-realistic class allows only clients in the m_allowedClients list to access the blur interface
// all other interfaces are allowed
class TestDisplay : public KWin::FilteredDisplay
{
public:
    TestDisplay(QObject *parent);
    bool allowInterface(KWin::ClientConnection *client, const QByteArray &interfaceName) override;
    QList<wl_client *> m_allowedClients;
};

TestDisplay::TestDisplay(QObject *parent)
    : KWin::FilteredDisplay(parent)
{
}

bool TestDisplay::allowInterface(KWin::ClientConnection *client, const QByteArray &interfaceName)
{
    if (interfaceName == "org_kde_kwin_blur_manager") {
        return m_allowedClients.contains(*client);
    }
    return true;
}

TestFilter::TestFilter(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
{
}

void TestFilter::init()
{
    using namespace KWin;
    delete m_display;
    m_display = new TestDisplay(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    m_compositorInterface = new CompositorInterface(m_display, m_display);
    m_blurManagerInterface = new BlurManagerInterface(m_display, m_display);
}

void TestFilter::cleanup()
{
}

void TestFilter::testFilter_data()
{
    QTest::addColumn<bool>("accessAllowed");
    QTest::newRow("granted") << true;
    QTest::newRow("denied") << false;
}

void TestFilter::testFilter()
{
    QFETCH(bool, accessAllowed);

    // setup connection
    std::unique_ptr<KWayland::Client::ConnectionThread> connection(new KWayland::Client::ConnectionThread());
    QSignalSpy connectedSpy(connection.get(), &KWayland::Client::ConnectionThread::connected);
    connection->setSocketName(s_socketName);

    std::unique_ptr<QThread> thread(new QThread(this));
    connection->moveToThread(thread.get());
    thread->start();

    connection->initConnection();
    QVERIFY(connectedSpy.wait());

    // use low level API as Server::Display::connections only lists connections which have
    // been previous fetched via getConnection()
    if (accessAllowed) {
        wl_client *clientConnection;
        wl_client_for_each(clientConnection, wl_display_get_client_list(*m_display))
        {
            m_display->m_allowedClients << clientConnection;
        }
    }

    KWayland::Client::EventQueue queue;
    queue.setup(connection.get());

    KWayland::Client::Registry registry;
    QSignalSpy registryDoneSpy(&registry, &KWayland::Client::Registry::interfacesAnnounced);
    QSignalSpy compositorSpy(&registry, &KWayland::Client::Registry::compositorAnnounced);
    QSignalSpy blurSpy(&registry, &KWayland::Client::Registry::blurAnnounced);

    registry.setEventQueue(&queue);
    registry.create(connection->display());
    QVERIFY(registry.isValid());
    registry.setup();

    QVERIFY(registryDoneSpy.wait());
    QVERIFY(compositorSpy.count() == 1);
    QVERIFY(blurSpy.count() == accessAllowed ? 1 : 0);

    thread->quit();
    thread->wait();
}

QTEST_GUILESS_MAIN(TestFilter)
#include "test_wayland_filter.moc"
