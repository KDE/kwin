/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "atoms.h"
#include "core/output.h"
#include "utils/pipe.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

#include <KWayland/Client/datadevice.h>
#include <KWayland/Client/datadevicemanager.h>
#include <KWayland/Client/datasource.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>

#include <QFutureWatcher>
#include <QMimeDatabase>
#include <QMimeType>
#include <QSocketNotifier>
#include <QtConcurrent>

#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_icccm.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_xwayland_selection-0");

class X11Display;

template<typename Offer>
static QFuture<QByteArray> readMimeTypeData(const Offer &offer, const QMimeType &mimeType)
{
    auto pipe = Pipe::create(O_CLOEXEC);
    if (!pipe) {
        return QFuture<QByteArray>();
    }

    offer->receive(mimeType.name(), pipe->fds[1].get());

    return QtConcurrent::run([fd = std::move(pipe->fds[0])] {
        QFile file;
        if (!file.open(fd.get(), QFile::ReadOnly | QFile::Text)) {
            return QByteArray();
        }

        return file.readAll();
    });
}

template<typename T>
static bool waitFuture(const QFuture<T> &future)
{
    QFutureWatcher<T> watcher;
    QSignalSpy finishedSpy(&watcher, &QFutureWatcher<T>::finished);
    watcher.setFuture(future);
    return finishedSpy.wait();
}

static QByteArray generateText(size_t size)
{
    auto generator = QRandomGenerator::global();
    QByteArray result;
    result.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        result.append(char(generator->bounded(32, 126)));
    }
    return result;
}

class X11Object
{
public:
    explicit X11Object(X11Display *display);
    virtual ~X11Object();

    virtual void handle(xcb_generic_event_t *event) = 0;

    X11Display *m_display;
};

class X11Display : public QObject
{
    Q_OBJECT

public:
    static std::unique_ptr<X11Display> create()
    {
        auto connection = Test::createX11Connection();
        if (!connection) {
            return nullptr;
        }
        return std::unique_ptr<X11Display>(new X11Display(std::move(connection)));
    }

    xcb_connection_t *connection() const
    {
        return m_connection.get();
    }

    xcb_window_t root() const
    {
        return m_root;
    }

    xcb_timestamp_t time() const
    {
        return 0;
    }

    xcb_atom_t mimeTypeToAtom(const QMimeType &mimeType) const
    {
        QByteArray effectiveName;
        if (mimeType.name() == QLatin1String("text/plain")) {
            effectiveName = QByteArrayLiteral("TEXT");
        } else {
            effectiveName = mimeType.name().toUtf8();
        }

        xcb_intern_atom_cookie_t cookie = xcb_intern_atom(m_connection.get(), 0, effectiveName.size(), effectiveName.data());
        xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(m_connection.get(), cookie, nullptr);
        if (!reply) {
            return XCB_ATOM_NONE;
        }

        const xcb_atom_t atom = reply->atom;
        free(reply);
        return atom;
    }

    QMimeType atomToMimeType(xcb_atom_t atom) const
    {
        xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name(m_connection.get(), atom);
        xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply(m_connection.get(), cookie, nullptr);
        if (!reply) {
            return QMimeType();
        }

        const size_t size = xcb_get_atom_name_name_length(reply);
        const char *buffer = xcb_get_atom_name_name(reply);
        const QString name = QString::fromLatin1(buffer, size);

        free(reply);

        QString effectiveName;
        if (name == QLatin1String("TEXT")) {
            effectiveName = QStringLiteral("text/plain");
        } else {
            effectiveName = name;
        }

        return QMimeDatabase().mimeTypeForName(effectiveName);
    }

    void addObject(X11Object *object)
    {
        m_objects.append(object);
    }

    void removeObject(X11Object *object)
    {
        m_objects.removeOne(object);
    }

private:
    X11Display(Test::XcbConnectionPtr connection)
        : m_connection(std::move(connection))
        , m_notifier(new QSocketNotifier(xcb_get_file_descriptor(m_connection.get()), QSocketNotifier::Read))
    {
        xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(m_connection.get())).data;
        m_root = screen->root;

        connect(m_notifier.get(), &QSocketNotifier::activated, this, &X11Display::dispatchEvents);
        connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, &X11Display::dispatchEvents);
        connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::awake, this, &X11Display::dispatchEvents);
    }

    void dispatchEvents()
    {
        xcb_generic_event_t *event;
        while ((event = xcb_poll_for_event(m_connection.get()))) {
            const auto objects = m_objects; // The object list can change while handling events.
            for (X11Object *object : objects) {
                object->handle(event);
            }
            std::free(event);
        }

        xcb_flush(m_connection.get());
    }

    Test::XcbConnectionPtr m_connection;
    xcb_window_t m_root = XCB_WINDOW_NONE;
    std::unique_ptr<QSocketNotifier> m_notifier;
    QList<X11Object *> m_objects;
};

struct X11SelectionData
{
    QByteArray data;
    xcb_atom_t type;
    uint8_t format;
};

class X11IncrSelectionTransaction : public QObject, public X11Object
{
    Q_OBJECT

public:
    explicit X11IncrSelectionTransaction(X11Display *display, xcb_window_t requestor, xcb_atom_t property, size_t chunkSize, const X11SelectionData &data)
        : X11Object(display)
        , m_data(data)
        , m_requestor(requestor)
        , m_property(property)
        , m_chunkSize(chunkSize)
    {
        const uint32_t valueMask = XCB_CW_EVENT_MASK;
        const uint32_t values[] = {XCB_EVENT_MASK_PROPERTY_CHANGE};
        xcb_change_window_attributes(display->connection(), m_requestor, valueMask, values);
    }

    ~X11IncrSelectionTransaction() override
    {
        const uint32_t valueMask = XCB_CW_EVENT_MASK;
        const uint32_t values[] = {XCB_EVENT_MASK_NO_EVENT};
        xcb_change_window_attributes(m_display->connection(), m_requestor, valueMask, values);
    }

    void handle(xcb_generic_event_t *event) override
    {
        if (XCB_EVENT_RESPONSE_TYPE(event) == XCB_PROPERTY_NOTIFY) {
            const auto propertyNotifyEvent = reinterpret_cast<xcb_property_notify_event_t *>(event);
            if (propertyNotifyEvent->window == m_requestor && propertyNotifyEvent->atom == m_property && propertyNotifyEvent->state == XCB_PROPERTY_DELETE) {
                if (const size_t remainingBytes = m_data.data.size() - m_offset; remainingBytes > 0) {
                    const size_t packetSize = std::min(remainingBytes, m_chunkSize);
                    const size_t elementSize = m_data.format / 8;
                    Q_ASSERT(packetSize % elementSize == 0);

                    const uint32_t elementCount = packetSize / elementSize;
                    const char *elements = m_data.data.constData() + m_offset;

                    xcb_change_property(m_display->connection(), XCB_PROP_MODE_REPLACE, m_requestor, m_property, m_data.type, m_data.format, elementCount, elements);

                    m_offset += packetSize;
                } else {
                    xcb_change_property(m_display->connection(), XCB_PROP_MODE_REPLACE, m_requestor, m_property, m_data.type, m_data.format, 0, nullptr);
                    delete this;
                }
            }
        }
    }

    static void start(X11Display *display, xcb_window_t requestor, xcb_atom_t property, size_t chunkSize, const X11SelectionData &data)
    {
        const size_t dataSize = data.data.size();
        xcb_change_property(display->connection(), XCB_PROP_MODE_REPLACE, requestor, property, atoms->incr, 32, 1, &dataSize);
        new X11IncrSelectionTransaction(display, requestor, property, chunkSize, data);
    }

private:
    X11SelectionData m_data;
    xcb_window_t m_requestor;
    xcb_atom_t m_property;
    size_t m_offset = 0;
    size_t m_chunkSize = 0;
};

class X11SelectionOwner : public QObject, public X11Object
{
    Q_OBJECT

public:
    explicit X11SelectionOwner(X11Display *display, xcb_atom_t selection, const QList<QMimeType> &mimeTypes, std::function<X11SelectionData(const QMimeType &mimeType)> renderFunction)
        : X11Object(display)
        , m_mimeTypes(mimeTypes)
        , m_renderFunction(renderFunction)
        , m_selection(selection)
    {
        m_window = xcb_generate_id(m_display->connection());
        xcb_create_window(m_display->connection(), XCB_COPY_FROM_PARENT, m_window, m_display->root(), 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, 0, nullptr);
    }

    ~X11SelectionOwner()
    {
        xcb_destroy_window(m_display->connection(), m_window);
    }

    QList<QMimeType> mimeTypes() const
    {
        return m_mimeTypes;
    }

    void setOwner(bool owner)
    {
        if (owner) {
            xcb_set_selection_owner(m_display->connection(), m_window, m_selection, XCB_CURRENT_TIME);
        } else {
            xcb_set_selection_owner(m_display->connection(), XCB_WINDOW_NONE, m_selection, XCB_CURRENT_TIME);
        }
    }

    void handle(xcb_generic_event_t *event) override
    {
        switch (XCB_EVENT_RESPONSE_TYPE(event)) {
        case XCB_SELECTION_REQUEST:
            onSelectionRequest(reinterpret_cast<xcb_selection_request_event_t *>(event));
            break;
        }
    }

    void setOnTargets(std::function<void()> callback)
    {
        m_onTargets = callback;
    }

private:
    void onSelectionRequest(xcb_selection_request_event_t *event)
    {
        if (event->selection != m_selection) {
            return;
        }

        if (event->property == XCB_NONE) {
            event->property = event->target;
        }

        if (event->target == atoms->timestamp) {
            sendTimestamp(event->requestor, event->property);
        } else if (event->target == atoms->targets) {
            sendTargets(event->requestor, event->property);
        } else {
            sendData(event->target, event->requestor, event->property);
        }

        xcb_selection_notify_event_t notifyEvent = {};
        notifyEvent.response_type = XCB_SELECTION_NOTIFY;
        notifyEvent.selection = event->selection;
        notifyEvent.requestor = event->requestor;
        notifyEvent.target = event->target;
        notifyEvent.property = event->property;
        xcb_send_event(m_display->connection(), false, event->requestor, 0, reinterpret_cast<char *>(&notifyEvent));
    }

    void sendTargets(xcb_window_t requestor, xcb_atom_t property)
    {
        if (m_onTargets) {
            m_onTargets();
        }

        QList<xcb_atom_t> targets{
            atoms->targets,
            atoms->timestamp,
        };

        for (const QMimeType &mimeType : std::as_const(m_mimeTypes)) {
            targets.append(m_display->mimeTypeToAtom(mimeType));
        }

        xcb_change_property(m_display->connection(), XCB_PROP_MODE_REPLACE, requestor, property, XCB_ATOM_ATOM, 32, targets.size(), targets.data());
    }

    void sendTimestamp(xcb_window_t requestor, xcb_atom_t property)
    {
        xcb_change_property(m_display->connection(), XCB_PROP_MODE_REPLACE, requestor, property, XCB_ATOM_INTEGER, 32, 1, &m_timestamp);
    }

    void sendData(xcb_atom_t target, xcb_window_t requestor, xcb_atom_t property)
    {
        const QMimeType mimeType = m_display->atomToMimeType(target);
        const X11SelectionData mimeData = m_renderFunction(mimeType);

        if (const size_t dataSize = mimeData.data.size(); dataSize <= m_chunkSize) {
            xcb_change_property(m_display->connection(), XCB_PROP_MODE_REPLACE, requestor, property, mimeData.type, mimeData.format, mimeData.data.size(), mimeData.data.data());
        } else {
            X11IncrSelectionTransaction::start(m_display, requestor, property, m_chunkSize, mimeData);
        }
    }

    QList<QMimeType> m_mimeTypes;
    std::function<X11SelectionData(const QMimeType &mimeType)> m_renderFunction;
    std::function<void()> m_onTargets = nullptr;
    xcb_window_t m_window = XCB_WINDOW_NONE;
    xcb_atom_t m_selection;
    xcb_timestamp_t m_timestamp = 0;
    const size_t m_chunkSize = 256;
};

class X11SelectionReader : public QObject, public X11Object
{
    Q_OBJECT

public:
    X11SelectionReader(X11Display *display, xcb_window_t requestor, xcb_atom_t selection, xcb_atom_t target, xcb_atom_t property, xcb_timestamp_t timestamp)
        : X11Object(display)
        , m_requestor(requestor)
        , m_selection(selection)
    {
        xcb_convert_selection(display->connection(), requestor, selection, target, property, timestamp);
    }

    void handle(xcb_generic_event_t *event) override
    {
        if (XCB_EVENT_RESPONSE_TYPE(event) == XCB_SELECTION_NOTIFY) {
            const auto selectionNotifyEvent = reinterpret_cast<xcb_selection_notify_event_t *>(event);
            if (selectionNotifyEvent->requestor == m_requestor && selectionNotifyEvent->selection == m_selection) {
                const auto [data, type] = readBlock(selectionNotifyEvent->requestor, selectionNotifyEvent->property);
                if (type == atoms->incr) {
                    m_incremental = true;
                } else {
                    if (m_incremental) {
                        if (data.isEmpty()) {
                            Q_EMIT done(m_buffer);
                        } else {
                            m_buffer += data;
                        }
                    } else {
                        Q_EMIT done(data);
                    }
                }
            }
        }
    }

    static QByteArray read(X11Display *display, xcb_window_t requestor, xcb_atom_t selection, xcb_atom_t target, xcb_atom_t property, xcb_timestamp_t timestamp)
    {
        X11SelectionReader reader(display, requestor, selection, target, property, timestamp);
        QSignalSpy doneSpy(&reader, &X11SelectionReader::done);
        doneSpy.wait();
        return doneSpy.last().at(0).value<QByteArray>();
    }

Q_SIGNALS:
    void done(const QByteArray &data);

private:
    std::pair<QByteArray, xcb_atom_t> readBlock(xcb_window_t window, xcb_atom_t property) const
    {
        xcb_get_property_cookie_t cookie = xcb_get_property(m_display->connection(), true, window, property, XCB_GET_PROPERTY_TYPE_ANY, 0, 0x1fffffff);
        xcb_get_property_reply_t *reply = xcb_get_property_reply(m_display->connection(), cookie, nullptr);
        if (!reply) {
            return std::make_pair(QByteArray(), XCB_ATOM_NONE);
        }

        const char *rawData = reinterpret_cast<char *>(xcb_get_property_value(reply));
        const int rawDataSizeInBytes = xcb_get_property_value_length(reply);

        const QByteArray data(rawData, rawDataSizeInBytes);
        const xcb_atom_t type = reply->type;
        free(reply);

        return std::make_pair(data, type);
    }

    xcb_window_t m_requestor;
    xcb_atom_t m_selection;
    QByteArray m_buffer;
    bool m_incremental = false;
};

X11Object::X11Object(X11Display *display)
    : m_display(display)
{
    m_display->addObject(this);
}

X11Object::~X11Object()
{
    m_display->removeObject(this);
}

static X11Window *createX11Window(xcb_connection_t *connection, const QRect &geometry, std::function<void(xcb_window_t)> setup = {})
{
    const uint32_t events =
        XCB_EVENT_MASK_ENTER_WINDOW
        | XCB_EVENT_MASK_LEAVE_WINDOW
        | XCB_EVENT_MASK_BUTTON_PRESS
        | XCB_EVENT_MASK_BUTTON_RELEASE
        | XCB_EVENT_MASK_BUTTON_MOTION;

    xcb_window_t windowId = xcb_generate_id(connection);
    xcb_create_window(connection, XCB_COPY_FROM_PARENT, windowId, rootWindow(),
                      geometry.x(),
                      geometry.y(),
                      geometry.width(),
                      geometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, XCB_CW_EVENT_MASK, &events);

    xcb_size_hints_t hints{};
    xcb_icccm_size_hints_set_position(&hints, 1, geometry.x(), geometry.y());
    xcb_icccm_size_hints_set_size(&hints, 1, geometry.width(), geometry.height());
    xcb_icccm_set_wm_normal_hints(connection, windowId, &hints);

    if (setup) {
        setup(windowId);
    }

    xcb_map_window(connection, windowId);
    xcb_flush(connection);

    QSignalSpy windowCreatedSpy(workspace(), &Workspace::windowAdded);
    if (!windowCreatedSpy.wait()) {
        return nullptr;
    }

    X11Window *window = windowCreatedSpy.last().first().value<X11Window *>();
    QMetaObject::invokeMethod(window, "setReadyForPainting");

    if (!Test::waitForWaylandSurface(window)) {
        xcb_destroy_window(connection, windowId);
        window->destroyWindow();
        return nullptr;
    }

    return window;
}

class XwaylandSelectionTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void clipboardX11ToWayland_data();
    void clipboardX11ToWayland();
    void emptyClipboardX11ToWayland();
    void clipboardX11ToWaylandSwitchFocusBeforeTargets();
    void clipboardWaylandToX11_data();
    void clipboardWaylandToX11();
    void emptyClipboardWaylandToX11();
    void snoopClipboard();

    void primarySelectionX11ToWayland_data();
    void primarySelectionX11ToWayland();
    void emptyPrimarySelectionX11ToWayland();
    void primarySelectionX11ToWaylandSwitchFocusBeforeTargets();
    void primarySelectionWaylandToX11_data();
    void primarySelectionWaylandToX11();
    void emptyPrimarySelectionWaylandToX11();
    void snoopPrimarySelection();

private:
    QMimeDatabase m_mimeDatabase;
};

void XwaylandSelectionTest::initTestCase()
{
    qRegisterMetaType<Window *>();

    QVERIFY(waylandServer()->init(s_socketName));
    kwinApp()->start();
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });
    const auto outputs = workspace()->outputs();
    QCOMPARE(outputs.count(), 2);
    QCOMPARE(outputs[0]->geometry(), QRect(0, 0, 1280, 1024));
    QCOMPARE(outputs[1]->geometry(), QRect(1280, 0, 1280, 1024));
}

void XwaylandSelectionTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::DataDeviceManager | Test::AdditionalWaylandInterface::WpPrimarySelectionV1));
}

void XwaylandSelectionTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void XwaylandSelectionTest::clipboardX11ToWayland_data()
{
    QTest::addColumn<QList<QMimeType>>("offeredMimeTypes");
    QTest::addColumn<QMimeType>("acceptedMimeType");
    QTest::addColumn<QByteArray>("acceptedMimeData");

    const QMimeType plainText = m_mimeDatabase.mimeTypeForName(QStringLiteral("text/plain"));
    const QMimeType htmlText = m_mimeDatabase.mimeTypeForName(QStringLiteral("text/html"));

    // This checks a case where the data is transferred with a single XChangeProperty() call.
    QTest::addRow("short text") << QList<QMimeType>{htmlText, plainText} << plainText << generateText(16);

    // This checks a case where the data is transferred using multiple XChangeProperty() calls.
    QTest::addRow("long text") << QList<QMimeType>{htmlText, plainText} << plainText << generateText(1024);
}

void XwaylandSelectionTest::clipboardX11ToWayland()
{
    // This test verifies that copy pasting from an X11 client to a Wayland client works as expected.

    QFETCH(QList<QMimeType>, offeredMimeTypes);
    QFETCH(QMimeType, acceptedMimeType);
    QFETCH(QByteArray, acceptedMimeData);

    // Show a Wayland window.
    KWayland::Client::DataDevice *waylandDataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    std::unique_ptr<KWayland::Client::Surface> waylandSurface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> waylandShellSurface = Test::createXdgToplevelSurface(waylandSurface.get());
    Window *waylandWindow = Test::renderAndWaitForShown(waylandSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);

    // Show an X11 window.
    std::unique_ptr<X11Display> x11Display = X11Display::create();
    QVERIFY(x11Display);
    X11Window *x11Window = createX11Window(x11Display->connection(), QRect(0, 0, 100, 100));
    QVERIFY(x11Window);

    // Copy.
    auto x11Selection = std::make_unique<X11SelectionOwner>(x11Display.get(), atoms->clipboard, offeredMimeTypes, [acceptedMimeData, acceptedMimeType](const QMimeType &mimeType) {
        if (mimeType == acceptedMimeType) {
            return X11SelectionData{
                .data = acceptedMimeData,
                .type = atoms->text,
                .format = 8,
            };
        }
        return X11SelectionData();
    });

    QSignalSpy seatSelectionChangedSpy(waylandServer()->seat(), &SeatInterface::selectionChanged);
    x11Selection->setOwner(true);
    QVERIFY(seatSelectionChangedSpy.wait());

    // Paste.
    QSignalSpy waylandDataDeviceSelectionOfferedSpy(waylandDataDevice, &KWayland::Client::DataDevice::selectionOffered);
    workspace()->activateWindow(waylandWindow);
    QVERIFY(waylandDataDeviceSelectionOfferedSpy.wait());
    KWayland::Client::DataOffer *offer = waylandDataDevice->offeredSelection();
    QCOMPARE(offer->offeredMimeTypes(), offeredMimeTypes);

    const QFuture<QByteArray> data = readMimeTypeData(offer, acceptedMimeType);
    QVERIFY(waitFuture(data));
    QCOMPARE(data.result(), acceptedMimeData);

    // Clear selection.
    QSignalSpy waylandDataDeviceSelectionClearedSpy(waylandDataDevice, &KWayland::Client::DataDevice::selectionCleared);
    x11Selection->setOwner(false);
    QVERIFY(waylandDataDeviceSelectionClearedSpy.wait());
}

void XwaylandSelectionTest::emptyClipboardX11ToWayland()
{
    // This test verifies that an empty selection owned by an X11 client is properly announced to Wayland clients.

    // Show a Wayland window.
    KWayland::Client::DataDevice *waylandDataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    std::unique_ptr<KWayland::Client::Surface> waylandSurface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> waylandShellSurface = Test::createXdgToplevelSurface(waylandSurface.get());
    Window *waylandWindow = Test::renderAndWaitForShown(waylandSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);

    // Show an X11 window.
    std::unique_ptr<X11Display> x11Display = X11Display::create();
    QVERIFY(x11Display);
    X11Window *x11Window = createX11Window(x11Display->connection(), QRect(0, 0, 100, 100));
    QVERIFY(x11Window);

    // Copy.
    auto x11Selection = std::make_unique<X11SelectionOwner>(x11Display.get(), atoms->clipboard, QList<QMimeType>{}, [](const QMimeType &mimeType) {
        return X11SelectionData{
            .data = QByteArrayLiteral("foobar"),
            .type = atoms->text,
            .format = 8,
        };
    });

    QSignalSpy seatSelectionChangedSpy(waylandServer()->seat(), &SeatInterface::selectionChanged);
    x11Selection->setOwner(true);
    QVERIFY(seatSelectionChangedSpy.wait());

    // Paste.
    QSignalSpy waylandDataDeviceSelectionOfferedSpy(waylandDataDevice, &KWayland::Client::DataDevice::selectionOffered);
    workspace()->activateWindow(waylandWindow);
    QVERIFY(waylandDataDeviceSelectionOfferedSpy.wait());
    KWayland::Client::DataOffer *offer = waylandDataDevice->offeredSelection();
    QCOMPARE(offer->offeredMimeTypes(), QList<QMimeType>{});

    const QMimeType plainText = m_mimeDatabase.mimeTypeForName(QStringLiteral("text/plain"));
    const QFuture<QByteArray> data = readMimeTypeData(offer, plainText);
    QVERIFY(waitFuture(data));
    QCOMPARE(data.result(), QByteArray());

    // Clear selection.
    QSignalSpy waylandDataDeviceSelectionClearedSpy(waylandDataDevice, &KWayland::Client::DataDevice::selectionCleared);
    x11Selection->setOwner(false);
    QVERIFY(waylandDataDeviceSelectionClearedSpy.wait());
}

void XwaylandSelectionTest::clipboardX11ToWaylandSwitchFocusBeforeTargets()
{
    // If an X11 client claims a selection, the data bridge code will not set a data source immediately.
    // Instead, it'll ask the X11 client for TARGETS. The reason for that is that wayland clients
    // expect mime types to be announced with the wl_data_offer. This test verifies that if the focus
    // changes before a TARGETS reply arrives, the wayland clients will still be able to get clipboard
    // data set by the X client.

    // Show a Wayland window.
    KWayland::Client::DataDevice *waylandDataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    std::unique_ptr<KWayland::Client::Surface> waylandSurface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> waylandShellSurface = Test::createXdgToplevelSurface(waylandSurface.get());
    Window *waylandWindow = Test::renderAndWaitForShown(waylandSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);

    // Show an X11 window.
    std::unique_ptr<X11Display> x11Display = X11Display::create();
    QVERIFY(x11Display);
    X11Window *x11Window = createX11Window(x11Display->connection(), QRect(0, 0, 100, 100));
    QVERIFY(x11Window);

    // Copy.
    const QMimeType plainText = m_mimeDatabase.mimeTypeForName(QStringLiteral("text/plain"));
    const QByteArray plainData = generateText(42);
    auto x11Selection = std::make_unique<X11SelectionOwner>(x11Display.get(), atoms->clipboard, QList<QMimeType>{plainText}, [plainText, plainData](const QMimeType &mimeType) {
        if (mimeType == plainText) {
            return X11SelectionData{
                .data = plainData,
                .type = atoms->text,
                .format = 8,
            };
        }
        return X11SelectionData();
    });

    x11Selection->setOnTargets([waylandWindow]() {
        workspace()->activateWindow(waylandWindow);
    });
    x11Selection->setOwner(true);

    // Paste, the wayland window will be activated when kwin reads the TARGETS property.
    QSignalSpy waylandDataDeviceSelectionOfferedSpy(waylandDataDevice, &KWayland::Client::DataDevice::selectionOffered);
    QVERIFY(waylandDataDeviceSelectionOfferedSpy.wait());
    KWayland::Client::DataOffer *offer = waylandDataDevice->offeredSelection();
    QCOMPARE(offer->offeredMimeTypes(), {plainText});

    const QFuture<QByteArray> data = readMimeTypeData(offer, plainText);
    QVERIFY(waitFuture(data));
    QCOMPARE(data.result(), plainData);
}

void XwaylandSelectionTest::clipboardWaylandToX11_data()
{
    QTest::addColumn<QList<QMimeType>>("offeredMimeTypes");
    QTest::addColumn<QMimeType>("acceptedMimeType");
    QTest::addColumn<QByteArray>("acceptedMimeData");

    const QMimeType plainText = m_mimeDatabase.mimeTypeForName(QStringLiteral("text/plain"));
    const QMimeType htmlText = m_mimeDatabase.mimeTypeForName(QStringLiteral("text/html"));

    // This checks a case where the data is transferred with a single XChangeProperty() call.
    QTest::addRow("short text") << QList<QMimeType>{htmlText, plainText} << plainText << generateText(16);

    // This checks a case where the data is transferred using multiple XChangeProperty() calls.
    QTest::addRow("long text") << QList<QMimeType>{htmlText, plainText} << plainText << generateText(1024);
}

void XwaylandSelectionTest::clipboardWaylandToX11()
{
    // This test verifies that copy pasting from a Wayland client to an X11 client works as expected.

    QVERIFY(Test::waitForWaylandKeyboard());

    QFETCH(QList<QMimeType>, offeredMimeTypes);
    QFETCH(QMimeType, acceptedMimeType);
    QFETCH(QByteArray, acceptedMimeData);

    // Show an X11 window.
    std::unique_ptr<X11Display> x11Display = X11Display::create();
    QVERIFY(x11Display);
    X11Window *x11Window = createX11Window(x11Display->connection(), QRect(0, 0, 100, 100));
    QVERIFY(x11Window);

    // Show a Wayland window.
    KWayland::Client::DataDevice *waylandDataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    KWayland::Client::Pointer *waylandPointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    std::unique_ptr<KWayland::Client::Surface> waylandSurface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> waylandShellSurface = Test::createXdgToplevelSurface(waylandSurface.get());
    Window *waylandWindow = Test::renderAndWaitForShown(waylandSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);

    // Copy.
    QSignalSpy waylandPointerButtonSpy(waylandPointer, &KWayland::Client::Pointer::buttonStateChanged);
    quint32 timestamp = 0;
    Test::pointerMotion(waylandWindow->frameGeometry().center(), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(waylandPointerButtonSpy.wait());

    KWayland::Client::DataSource *waylandDataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    for (const QMimeType &mimeType : offeredMimeTypes) {
        waylandDataSource->offer(mimeType);
    }
    connect(waylandDataSource, &KWayland::Client::DataSource::sendDataRequested, this, [&acceptedMimeType, &acceptedMimeData](const QString &requestedMimeType, int fd) {
        if (requestedMimeType == acceptedMimeType.name()) {
            write(fd, acceptedMimeData.data(), acceptedMimeData.size());
        }
        close(fd);
    });

    QSignalSpy seatSelectionChangedSpy(waylandServer()->seat(), &SeatInterface::selectionChanged);
    waylandDataDevice->setSelection(waylandPointerButtonSpy.constFirst().at(0).value<quint32>(), waylandDataSource);
    QVERIFY(seatSelectionChangedSpy.wait());

    // Paste.
    workspace()->activateWindow(x11Window);

    const QByteArray actualData = X11SelectionReader::read(x11Display.get(), x11Window->window(), atoms->clipboard, x11Display->mimeTypeToAtom(acceptedMimeType), atoms->wl_selection, XCB_CURRENT_TIME);
    QCOMPARE(actualData, acceptedMimeData);

    // Clear selection.
    delete waylandDataSource;
    waylandDataSource = nullptr;
    QVERIFY(seatSelectionChangedSpy.wait());

    const QByteArray clearedData = X11SelectionReader::read(x11Display.get(), x11Window->window(), atoms->clipboard, x11Display->mimeTypeToAtom(acceptedMimeType), atoms->wl_selection, XCB_CURRENT_TIME);
    QCOMPARE(clearedData, QByteArray());
}

void XwaylandSelectionTest::emptyClipboardWaylandToX11()
{
    // This test verifies that an empty selection owned by a Wayland client is properly announced to X11 clients.

    QVERIFY(Test::waitForWaylandKeyboard());

    // Show an X11 window.
    std::unique_ptr<X11Display> x11Display = X11Display::create();
    QVERIFY(x11Display);
    X11Window *x11Window = createX11Window(x11Display->connection(), QRect(0, 0, 100, 100));
    QVERIFY(x11Window);

    // Show a Wayland window.
    KWayland::Client::DataDevice *waylandDataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    KWayland::Client::Pointer *waylandPointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    std::unique_ptr<KWayland::Client::Surface> waylandSurface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> waylandShellSurface = Test::createXdgToplevelSurface(waylandSurface.get());
    Window *waylandWindow = Test::renderAndWaitForShown(waylandSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);

    // Copy.
    QSignalSpy waylandPointerButtonSpy(waylandPointer, &KWayland::Client::Pointer::buttonStateChanged);
    quint32 timestamp = 0;
    Test::pointerMotion(waylandWindow->frameGeometry().center(), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(waylandPointerButtonSpy.wait());

    KWayland::Client::DataSource *waylandDataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    connect(waylandDataSource, &KWayland::Client::DataSource::sendDataRequested, this, [](const QString &requestedMimeType, int fd) {
        const auto data = QByteArrayLiteral("foobar");
        write(fd, data.data(), data.size());
        close(fd);
    });

    QSignalSpy seatSelectionChangedSpy(waylandServer()->seat(), &SeatInterface::selectionChanged);
    waylandDataDevice->setSelection(waylandPointerButtonSpy.constFirst().at(0).value<quint32>(), waylandDataSource);
    QVERIFY(seatSelectionChangedSpy.wait());

    // Paste.
    workspace()->activateWindow(x11Window);

    const QMimeType plainText = m_mimeDatabase.mimeTypeForName(QStringLiteral("text/plain"));
    const QByteArray actualData = X11SelectionReader::read(x11Display.get(), x11Window->window(), atoms->clipboard, x11Display->mimeTypeToAtom(plainText), atoms->wl_selection, XCB_CURRENT_TIME);
    QCOMPARE(actualData, QByteArray());

    // Clear selection.
    delete waylandDataSource;
    waylandDataSource = nullptr;
    QVERIFY(seatSelectionChangedSpy.wait());

    const QByteArray clearedData = X11SelectionReader::read(x11Display.get(), x11Window->window(), atoms->clipboard, x11Display->mimeTypeToAtom(plainText), atoms->wl_selection, XCB_CURRENT_TIME);
    QCOMPARE(clearedData, QByteArray());
}

void XwaylandSelectionTest::snoopClipboard()
{
    // This test verifies that an inactive X11 window can't read clipboard contents.

    QVERIFY(Test::waitForWaylandKeyboard());

    const QMimeType mimeType = m_mimeDatabase.mimeTypeForName(QStringLiteral("text/plain"));
    const QByteArray mimeData = QByteArrayLiteral("foobar");

    // Show an X11 window.
    std::unique_ptr<X11Display> x11Display = X11Display::create();
    QVERIFY(x11Display);
    X11Window *x11Window = createX11Window(x11Display->connection(), QRect(0, 0, 100, 100));
    QVERIFY(x11Window);

    // Show a Wayland window.
    KWayland::Client::DataDevice *waylandDataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    KWayland::Client::Pointer *waylandPointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    std::unique_ptr<KWayland::Client::Surface> waylandSurface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> waylandShellSurface = Test::createXdgToplevelSurface(waylandSurface.get());
    Window *waylandWindow = Test::renderAndWaitForShown(waylandSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);

    // Copy.
    QSignalSpy waylandPointerButtonSpy(waylandPointer, &KWayland::Client::Pointer::buttonStateChanged);
    quint32 timestamp = 0;
    Test::pointerMotion(waylandWindow->frameGeometry().center(), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(waylandPointerButtonSpy.wait());

    KWayland::Client::DataSource *waylandDataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    waylandDataSource->offer(mimeType);
    connect(waylandDataSource, &KWayland::Client::DataSource::sendDataRequested, this, [&mimeType, &mimeData](const QString &requestedMimeType, int fd) {
        if (requestedMimeType == mimeType.name()) {
            write(fd, mimeData.data(), mimeData.size());
        }
        close(fd);
    });

    QSignalSpy seatSelectionChangedSpy(waylandServer()->seat(), &SeatInterface::selectionChanged);
    waylandDataDevice->setSelection(waylandPointerButtonSpy.constFirst().at(0).value<quint32>(), waylandDataSource);
    QVERIFY(seatSelectionChangedSpy.wait());

    // Paste while the X window is inactive.
    QByteArray actualData = X11SelectionReader::read(x11Display.get(), x11Window->window(), atoms->clipboard, x11Display->mimeTypeToAtom(mimeType), atoms->wl_selection, XCB_CURRENT_TIME);
    QCOMPARE(actualData, QByteArray());

    // Try again but when the X window is active.
    workspace()->activateWindow(x11Window);
    actualData = X11SelectionReader::read(x11Display.get(), x11Window->window(), atoms->clipboard, x11Display->mimeTypeToAtom(mimeType), atoms->wl_selection, XCB_CURRENT_TIME);
    QCOMPARE(actualData, mimeData);

    // One more time but when the X window is inactive again.
    workspace()->activateWindow(waylandWindow);
    actualData = X11SelectionReader::read(x11Display.get(), x11Window->window(), atoms->clipboard, x11Display->mimeTypeToAtom(mimeType), atoms->wl_selection, XCB_CURRENT_TIME);
    QCOMPARE(actualData, QByteArray());
}

void XwaylandSelectionTest::primarySelectionX11ToWayland_data()
{
    QTest::addColumn<QList<QMimeType>>("offeredMimeTypes");
    QTest::addColumn<QMimeType>("acceptedMimeType");
    QTest::addColumn<QByteArray>("acceptedMimeData");

    const QMimeType plainText = m_mimeDatabase.mimeTypeForName(QStringLiteral("text/plain"));
    const QMimeType htmlText = m_mimeDatabase.mimeTypeForName(QStringLiteral("text/html"));

    // This checks a case where the data is transferred with a single XChangeProperty() call.
    QTest::addRow("short text") << QList<QMimeType>{htmlText, plainText} << plainText << generateText(16);

    // This checks a case where the data is transferred using multiple XChangeProperty() calls.
    QTest::addRow("long text") << QList<QMimeType>{htmlText, plainText} << plainText << generateText(1024);
}

void XwaylandSelectionTest::primarySelectionX11ToWayland()
{
    // This test verifies that copy pasting from an X11 client to a Wayland client works as expected.

    QFETCH(QList<QMimeType>, offeredMimeTypes);
    QFETCH(QMimeType, acceptedMimeType);
    QFETCH(QByteArray, acceptedMimeData);

    // Show a Wayland window.
    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> waylandDataDevice = Test::primarySelectionManager()->getDevice(Test::waylandSeat());
    std::unique_ptr<KWayland::Client::Surface> waylandSurface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> waylandShellSurface = Test::createXdgToplevelSurface(waylandSurface.get());
    Window *waylandWindow = Test::renderAndWaitForShown(waylandSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);

    // Show an X11 window.
    std::unique_ptr<X11Display> x11Display = X11Display::create();
    QVERIFY(x11Display);
    X11Window *x11Window = createX11Window(x11Display->connection(), QRect(0, 0, 100, 100));
    QVERIFY(x11Window);

    // Copy.
    auto x11Selection = std::make_unique<X11SelectionOwner>(x11Display.get(), atoms->primary, offeredMimeTypes, [acceptedMimeData, acceptedMimeType](const QMimeType &mimeType) {
        if (mimeType == acceptedMimeType) {
            return X11SelectionData{
                .data = acceptedMimeData,
                .type = atoms->text,
                .format = 8,
            };
        }
        return X11SelectionData();
    });

    QSignalSpy seatPrimarySelectionChangedSpy(waylandServer()->seat(), &SeatInterface::primarySelectionChanged);
    x11Selection->setOwner(true);
    QVERIFY(seatPrimarySelectionChangedSpy.wait());

    // Paste.
    QSignalSpy waylandDataDeviceSelectionOfferedSpy(waylandDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionOffered);
    workspace()->activateWindow(waylandWindow);
    QVERIFY(waylandDataDeviceSelectionOfferedSpy.wait());
    Test::WpPrimarySelectionOfferV1 *offer = waylandDataDevice->offer();
    QCOMPARE(offer->mimeTypes(), offeredMimeTypes);

    const QFuture<QByteArray> data = readMimeTypeData(offer, acceptedMimeType);
    QVERIFY(waitFuture(data));
    QCOMPARE(data.result(), acceptedMimeData);

    // Clear selection.
    QSignalSpy waylandDataDeviceSelectionClearedSpy(waylandDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionCleared);
    x11Selection->setOwner(false);
    QVERIFY(waylandDataDeviceSelectionClearedSpy.wait());
}

void XwaylandSelectionTest::emptyPrimarySelectionX11ToWayland()
{
    // This test verifies that an empty primary selection owned by an X11 client is properly announced to Wayland clients.

    // Show a Wayland window.
    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> waylandDataDevice = Test::primarySelectionManager()->getDevice(Test::waylandSeat());
    std::unique_ptr<KWayland::Client::Surface> waylandSurface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> waylandShellSurface = Test::createXdgToplevelSurface(waylandSurface.get());
    Window *waylandWindow = Test::renderAndWaitForShown(waylandSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);

    // Show an X11 window.
    std::unique_ptr<X11Display> x11Display = X11Display::create();
    QVERIFY(x11Display);
    X11Window *x11Window = createX11Window(x11Display->connection(), QRect(0, 0, 100, 100));
    QVERIFY(x11Window);

    // Copy.
    auto x11Selection = std::make_unique<X11SelectionOwner>(x11Display.get(), atoms->primary, QList<QMimeType>{}, [](const QMimeType &mimeType) {
        return X11SelectionData{
            .data = QByteArrayLiteral("foobar"),
            .type = atoms->text,
            .format = 8,
        };
    });

    QSignalSpy seatPrimarySelectionChangedSpy(waylandServer()->seat(), &SeatInterface::primarySelectionChanged);
    x11Selection->setOwner(true);
    QVERIFY(seatPrimarySelectionChangedSpy.wait());

    // Paste.
    QSignalSpy waylandDataDeviceSelectionOfferedSpy(waylandDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionOffered);
    workspace()->activateWindow(waylandWindow);
    QVERIFY(waylandDataDeviceSelectionOfferedSpy.wait());
    Test::WpPrimarySelectionOfferV1 *offer = waylandDataDevice->offer();
    QCOMPARE(offer->mimeTypes(), QList<QMimeType>{});

    const QMimeType plainText = m_mimeDatabase.mimeTypeForName(QStringLiteral("text/plain"));
    const QFuture<QByteArray> data = readMimeTypeData(offer, plainText);
    QVERIFY(waitFuture(data));
    QCOMPARE(data.result(), QByteArray());

    // Clear selection.
    QSignalSpy waylandDataDeviceSelectionClearedSpy(waylandDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionCleared);
    x11Selection->setOwner(false);
    QVERIFY(waylandDataDeviceSelectionClearedSpy.wait());
}

void XwaylandSelectionTest::primarySelectionX11ToWaylandSwitchFocusBeforeTargets()
{
    // If an X11 client claims a selection, the data bridge code will not set a data source immediately.
    // Instead, it'll ask the X11 client for TARGETS. The reason for that is that wayland clients
    // expect mime types to be announced with the wl_data_offer. This test verifies that if the focus
    // changes before a TARGETS reply arrives, the wayland clients will still be able to get clipboard
    // data set by the X client.

    // Show a Wayland window.
    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> waylandDataDevice = Test::primarySelectionManager()->getDevice(Test::waylandSeat());
    std::unique_ptr<KWayland::Client::Surface> waylandSurface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> waylandShellSurface = Test::createXdgToplevelSurface(waylandSurface.get());
    Window *waylandWindow = Test::renderAndWaitForShown(waylandSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);

    // Show an X11 window.
    std::unique_ptr<X11Display> x11Display = X11Display::create();
    QVERIFY(x11Display);
    X11Window *x11Window = createX11Window(x11Display->connection(), QRect(0, 0, 100, 100));
    QVERIFY(x11Window);

    // Copy.
    const QMimeType plainText = m_mimeDatabase.mimeTypeForName(QStringLiteral("text/plain"));
    const QByteArray plainData = generateText(42);
    auto x11Selection = std::make_unique<X11SelectionOwner>(x11Display.get(), atoms->primary, QList<QMimeType>{plainText}, [plainText, plainData](const QMimeType &mimeType) {
        if (mimeType == plainText) {
            return X11SelectionData{
                .data = plainData,
                .type = atoms->text,
                .format = 8,
            };
        }
        return X11SelectionData();
    });

    x11Selection->setOnTargets([waylandWindow]() {
        workspace()->activateWindow(waylandWindow);
    });
    x11Selection->setOwner(true);

    // Paste, the wayland window will be activated when kwin reads the TARGETS property.
    QSignalSpy waylandDataDeviceSelectionOfferedSpy(waylandDataDevice.get(), &Test::WpPrimarySelectionDeviceV1::selectionOffered);
    QVERIFY(waylandDataDeviceSelectionOfferedSpy.wait());
    Test::WpPrimarySelectionOfferV1 *offer = waylandDataDevice->offer();
    QCOMPARE(offer->mimeTypes(), {plainText});

    const QFuture<QByteArray> data = readMimeTypeData(offer, plainText);
    QVERIFY(waitFuture(data));
    QCOMPARE(data.result(), plainData);
}

void XwaylandSelectionTest::primarySelectionWaylandToX11_data()
{
    QTest::addColumn<QList<QMimeType>>("offeredMimeTypes");
    QTest::addColumn<QMimeType>("acceptedMimeType");
    QTest::addColumn<QByteArray>("acceptedMimeData");

    const QMimeType plainText = m_mimeDatabase.mimeTypeForName(QStringLiteral("text/plain"));
    const QMimeType htmlText = m_mimeDatabase.mimeTypeForName(QStringLiteral("text/html"));

    // This checks a case where the data is transferred with a single XChangeProperty() call.
    QTest::addRow("short text") << QList<QMimeType>{htmlText, plainText} << plainText << generateText(16);

    // This checks a case where the data is transferred using multiple XChangeProperty() calls.
    QTest::addRow("long text") << QList<QMimeType>{htmlText, plainText} << plainText << generateText(1024);
}

void XwaylandSelectionTest::primarySelectionWaylandToX11()
{
    // This test verifies that copy pasting from a Wayland client to an X11 client works as expected.

    QVERIFY(Test::waitForWaylandKeyboard());

    QFETCH(QList<QMimeType>, offeredMimeTypes);
    QFETCH(QMimeType, acceptedMimeType);
    QFETCH(QByteArray, acceptedMimeData);

    // Show an X11 window.
    std::unique_ptr<X11Display> x11Display = X11Display::create();
    QVERIFY(x11Display);
    X11Window *x11Window = createX11Window(x11Display->connection(), QRect(0, 0, 100, 100));
    QVERIFY(x11Window);

    // Show a Wayland window.
    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> waylandDataDevice = Test::primarySelectionManager()->getDevice(Test::waylandSeat());
    KWayland::Client::Pointer *waylandPointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    std::unique_ptr<KWayland::Client::Surface> waylandSurface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> waylandShellSurface = Test::createXdgToplevelSurface(waylandSurface.get());
    Window *waylandWindow = Test::renderAndWaitForShown(waylandSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);

    // Copy.
    QSignalSpy waylandPointerButtonSpy(waylandPointer, &KWayland::Client::Pointer::buttonStateChanged);
    quint32 timestamp = 0;
    Test::pointerMotion(waylandWindow->frameGeometry().center(), timestamp++);
    Test::pointerButtonPressed(BTN_MIDDLE, timestamp++);
    Test::pointerButtonReleased(BTN_MIDDLE, timestamp++);
    QVERIFY(waylandPointerButtonSpy.wait());

    std::unique_ptr<Test::WpPrimarySelectionSourceV1> waylandDataSource = Test::primarySelectionManager()->createSource();
    for (const QMimeType &mimeType : offeredMimeTypes) {
        waylandDataSource->offer(mimeType.name());
    }
    connect(waylandDataSource.get(), &Test::WpPrimarySelectionSourceV1::sendDataRequested, this, [&acceptedMimeType, &acceptedMimeData](const QString &requestedMimeType, int32_t fd) {
        if (requestedMimeType == acceptedMimeType.name()) {
            write(fd, acceptedMimeData.data(), acceptedMimeData.size());
        }
        close(fd);
    });

    QSignalSpy seatPrimarySelectionChangedSpy(waylandServer()->seat(), &SeatInterface::primarySelectionChanged);
    waylandDataDevice->set_selection(waylandDataSource->object(), waylandPointerButtonSpy.constFirst().at(0).value<quint32>());
    QVERIFY(seatPrimarySelectionChangedSpy.wait());

    // Paste.
    workspace()->activateWindow(x11Window);

    const QByteArray actualData = X11SelectionReader::read(x11Display.get(), x11Window->window(), atoms->primary, x11Display->mimeTypeToAtom(acceptedMimeType), atoms->wl_selection, XCB_CURRENT_TIME);
    QCOMPARE(actualData, acceptedMimeData);

    // Clear selection.
    waylandDataSource.reset();
    QVERIFY(seatPrimarySelectionChangedSpy.wait());

    const QByteArray clearedData = X11SelectionReader::read(x11Display.get(), x11Window->window(), atoms->primary, x11Display->mimeTypeToAtom(acceptedMimeType), atoms->wl_selection, XCB_CURRENT_TIME);
    QCOMPARE(clearedData, QByteArray());
}

void XwaylandSelectionTest::emptyPrimarySelectionWaylandToX11()
{
    // This test verifies that an empty primary selection owned by a Wayland client is properly announced to X11 clients.

    QVERIFY(Test::waitForWaylandKeyboard());

    // Show an X11 window.
    std::unique_ptr<X11Display> x11Display = X11Display::create();
    QVERIFY(x11Display);
    X11Window *x11Window = createX11Window(x11Display->connection(), QRect(0, 0, 100, 100));
    QVERIFY(x11Window);

    // Show a Wayland window.
    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> waylandDataDevice = Test::primarySelectionManager()->getDevice(Test::waylandSeat());
    KWayland::Client::Pointer *waylandPointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    std::unique_ptr<KWayland::Client::Surface> waylandSurface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> waylandShellSurface = Test::createXdgToplevelSurface(waylandSurface.get());
    Window *waylandWindow = Test::renderAndWaitForShown(waylandSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);

    // Copy.
    QSignalSpy waylandPointerButtonSpy(waylandPointer, &KWayland::Client::Pointer::buttonStateChanged);
    quint32 timestamp = 0;
    Test::pointerMotion(waylandWindow->frameGeometry().center(), timestamp++);
    Test::pointerButtonPressed(BTN_MIDDLE, timestamp++);
    Test::pointerButtonReleased(BTN_MIDDLE, timestamp++);
    QVERIFY(waylandPointerButtonSpy.wait());

    std::unique_ptr<Test::WpPrimarySelectionSourceV1> waylandDataSource = Test::primarySelectionManager()->createSource();
    connect(waylandDataSource.get(), &Test::WpPrimarySelectionSourceV1::sendDataRequested, this, [](const QString &requestedMimeType, int32_t fd) {
        const auto data = QByteArrayLiteral("foobar");
        write(fd, data.data(), data.size());
        close(fd);
    });

    QSignalSpy seatPrimarySelectionChangedSpy(waylandServer()->seat(), &SeatInterface::primarySelectionChanged);
    waylandDataDevice->set_selection(waylandDataSource->object(), waylandPointerButtonSpy.constFirst().at(0).value<quint32>());
    QVERIFY(seatPrimarySelectionChangedSpy.wait());

    // Paste.
    workspace()->activateWindow(x11Window);

    const QMimeType plainText = m_mimeDatabase.mimeTypeForName(QStringLiteral("text/plain"));
    const QByteArray actualData = X11SelectionReader::read(x11Display.get(), x11Window->window(), atoms->primary, x11Display->mimeTypeToAtom(plainText), atoms->wl_selection, XCB_CURRENT_TIME);
    QCOMPARE(actualData, QByteArray());

    // Clear selection.
    waylandDataSource.reset();
    QVERIFY(seatPrimarySelectionChangedSpy.wait());

    const QByteArray clearedData = X11SelectionReader::read(x11Display.get(), x11Window->window(), atoms->primary, x11Display->mimeTypeToAtom(plainText), atoms->wl_selection, XCB_CURRENT_TIME);
    QCOMPARE(clearedData, QByteArray());
}

void XwaylandSelectionTest::snoopPrimarySelection()
{
    // This test verifies that an inactive X11 window can't read primary selection contents.

    QVERIFY(Test::waitForWaylandKeyboard());

    const QMimeType mimeType = m_mimeDatabase.mimeTypeForName(QStringLiteral("text/plain"));
    const QByteArray mimeData = QByteArrayLiteral("foobar");

    // Show an X11 window.
    std::unique_ptr<X11Display> x11Display = X11Display::create();
    QVERIFY(x11Display);
    X11Window *x11Window = createX11Window(x11Display->connection(), QRect(0, 0, 100, 100));
    QVERIFY(x11Window);

    // Show a Wayland window.
    std::unique_ptr<Test::WpPrimarySelectionDeviceV1> waylandDataDevice = Test::primarySelectionManager()->getDevice(Test::waylandSeat());
    KWayland::Client::Pointer *waylandPointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    std::unique_ptr<KWayland::Client::Surface> waylandSurface = Test::createSurface();
    std::unique_ptr<Test::XdgToplevel> waylandShellSurface = Test::createXdgToplevelSurface(waylandSurface.get());
    Window *waylandWindow = Test::renderAndWaitForShown(waylandSurface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);

    // Copy.
    QSignalSpy waylandPointerButtonSpy(waylandPointer, &KWayland::Client::Pointer::buttonStateChanged);
    quint32 timestamp = 0;
    Test::pointerMotion(waylandWindow->frameGeometry().center(), timestamp++);
    Test::pointerButtonPressed(BTN_MIDDLE, timestamp++);
    Test::pointerButtonReleased(BTN_MIDDLE, timestamp++);
    QVERIFY(waylandPointerButtonSpy.wait());

    std::unique_ptr<Test::WpPrimarySelectionSourceV1> waylandDataSource = Test::primarySelectionManager()->createSource();
    waylandDataSource->offer(mimeType.name());
    connect(waylandDataSource.get(), &Test::WpPrimarySelectionSourceV1::sendDataRequested, this, [&mimeType, &mimeData](const QString &requestedMimeType, int fd) {
        if (requestedMimeType == mimeType.name()) {
            write(fd, mimeData.data(), mimeData.size());
        }
        close(fd);
    });

    QSignalSpy seatPrimarySelectionChangedSpy(waylandServer()->seat(), &SeatInterface::primarySelectionChanged);
    waylandDataDevice->set_selection(waylandDataSource->object(), waylandPointerButtonSpy.constFirst().at(0).value<quint32>());
    QVERIFY(seatPrimarySelectionChangedSpy.wait());

    // Paste while the X window is inactive.
    QByteArray actualData = X11SelectionReader::read(x11Display.get(), x11Window->window(), atoms->primary, x11Display->mimeTypeToAtom(mimeType), atoms->wl_selection, XCB_CURRENT_TIME);
    QCOMPARE(actualData, QByteArray());

    // Try again but when the X window is active.
    workspace()->activateWindow(x11Window);
    actualData = X11SelectionReader::read(x11Display.get(), x11Window->window(), atoms->primary, x11Display->mimeTypeToAtom(mimeType), atoms->wl_selection, XCB_CURRENT_TIME);
    QCOMPARE(actualData, mimeData);

    // One more time but when the X window is inactive again.
    workspace()->activateWindow(waylandWindow);
    actualData = X11SelectionReader::read(x11Display.get(), x11Window->window(), atoms->primary, x11Display->mimeTypeToAtom(mimeType), atoms->wl_selection, XCB_CURRENT_TIME);
    QCOMPARE(actualData, QByteArray());
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::XwaylandSelectionTest)
#include "xwayland_selection_test.moc"
