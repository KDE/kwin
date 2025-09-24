/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wayland_test.h"

#include "atoms.h"
#include "input.h"
#include "pointer_input.h"
#include "utils/pipe.h"
#include "wayland/seat.h"
#include "wayland_server.h"
#include "workspace.h"
#include "x11window.h"

#include <KWayland/Client/datadevice.h>
#include <KWayland/Client/datadevicemanager.h>
#include <KWayland/Client/dataoffer.h>
#include <KWayland/Client/datasource.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/seat.h>

#include <QAbstractEventDispatcher>
#include <QFuture>
#include <QMimeDatabase>
#include <QSocketNotifier>
#include <QtConcurrent>

#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xfixes.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_xwayland_dnd-0");
static const int xdndVersion = 5;

class X11Display;

static bool waitTwo(QSignalSpy &firstSpy, const char *firstText, QSignalSpy &secondSpy, const char *secondText, int timeout = 5000)
{
    const std::array<QSignalSpy *, 2> spies{&firstSpy, &secondSpy};
    const std::array<const char *, 2> labels{firstText, secondText};
    const std::array<qsizetype, 2> initialCounts{firstSpy.count(), secondSpy.count()};

    const int step = std::max(1000, timeout / 5);
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeout) {
        for (size_t i = 0; i < spies.size(); ++i) {
            if (initialCounts[i] == spies[i]->size()) {
                if (!spies[i]->wait(step)) {
                    break;
                }
            }
        }

        bool ok = true;
        for (size_t i = 0; i < spies.size(); ++i) {
            if (initialCounts[i] == spies[i]->size()) {
                ok = false;
                break;
            }
        }

        if (ok) {
            return true;
        }
    }

    for (size_t i = 0; i < spies.size(); ++i) {
        if (initialCounts[i] == spies[i]->size()) {
            qDebug() << labels[i] << "has not been signalled";
        }
    }

    return false;
}

#define KWAIT_TWO(a, b) QVERIFY(waitTwo(a, #a, b, #b))

static QFuture<QByteArray> readMimeTypeData(KWayland::Client::DataOffer *offer, const QString &mimeType)
{
    auto pipe = Pipe::create(O_CLOEXEC);
    if (!pipe) {
        return QFuture<QByteArray>();
    }

    offer->receive(mimeType, pipe->fds[1].get());

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

static void sendXdndEvent(xcb_connection_t *connection, xcb_window_t target, xcb_atom_t type, const xcb_client_message_data_t *data)
{
    xcb_client_message_event_t event{
        .response_type = XCB_CLIENT_MESSAGE,
        .format = 32,
        .sequence = 0,
        .window = target,
        .type = type,
        .data = *data,
    };
    static_assert(sizeof(event) == 32, "Would leak stack data otherwise");

    xcb_send_event(connection, 0, target, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<const char *>(&event));
    xcb_flush(connection);
}

static xcb_window_t findXdndAwareWindow(xcb_connection_t *connection, xcb_window_t root, const QPoint &position)
{
    xcb_window_t window = root;
    QPoint localPosition = position;

    while (window != XCB_WINDOW_NONE) {
        xcb_translate_coordinates_cookie_t translateCookie = xcb_translate_coordinates(connection, window, window, localPosition.x(), localPosition.y());
        xcb_translate_coordinates_reply_t *translateReply = xcb_translate_coordinates_reply(connection, translateCookie, nullptr);
        auto translateReplyCleanup = qScopeGuard([&translateReply]() {
            free(translateReply);
        });

        if (!translateReply) {
            return XCB_WINDOW_NONE;
        }

        xcb_window_t target = translateReply->child;
        if (target == XCB_WINDOW_NONE) {
            break;
        }

        xcb_get_property_cookie_t propertyCookie = xcb_get_property(connection, false, target, atoms->xdnd_aware, XCB_GET_PROPERTY_TYPE_ANY, 0, 0);
        xcb_get_property_reply_t *propertyReply = xcb_get_property_reply(connection, propertyCookie, nullptr);
        auto propertyReplyCleanup = qScopeGuard([&propertyReply]() {
            free(propertyReply);
        });

        if (propertyReply && propertyReply->type != XCB_NONE) {
            return target;
        }

        window = target;
        localPosition = QPoint(translateReply->dst_x, translateReply->dst_y);
    }

    return XCB_WINDOW_NONE;
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

static X11Window *createXdndAwareTestWindow(xcb_connection_t *connection, const QRect &geometry)
{
    return createX11Window(connection, geometry, [connection](xcb_window_t window) {
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, atoms->xdnd_aware, XCB_ATOM_ATOM, 32, 1, &xdndVersion);
        Test::applyMotifHints(connection, window, Test::MotifHints{
                                                      .flags = Test::MWM_HINTS_DECORATIONS,
                                                      .decorations = 0,
                                                  });
    });
}

static X11Window *createXdndUnawareTestWindow(xcb_connection_t *connection, const QRect &geometry)
{
    return createX11Window(connection, geometry, [connection](xcb_window_t window) {
        Test::applyMotifHints(connection, window, Test::MotifHints{
                                                      .flags = Test::MWM_HINTS_DECORATIONS,
                                                      .decorations = 0,
                                                  });
    });
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

class X11Drag : public QObject, public X11Object
{
    Q_OBJECT

public:
    X11Drag(X11Display *display, X11SelectionOwner *selection, xcb_window_t source)
        : X11Object(display)
        , m_selection(selection)
        , m_source(source)
    {
    }

    ~X11Drag() override
    {
        m_selection->setOwner(false);
    }

    void handle(xcb_generic_event_t *event) override
    {
        switch (XCB_EVENT_RESPONSE_TYPE(event)) {
        case XCB_CLIENT_MESSAGE: {
            const auto clientMessageEvent = reinterpret_cast<xcb_client_message_event_t *>(event);
            if (clientMessageEvent->window == m_source) {
                if (clientMessageEvent->type == atoms->xdnd_status) {
                    onXdndStatus(clientMessageEvent);
                } else if (clientMessageEvent->type == atoms->xdnd_finished) {
                    onXdndFinished(clientMessageEvent);
                }
            }
            break;
        }
        case XCB_MOTION_NOTIFY: {
            const auto motionEvent = reinterpret_cast<xcb_motion_notify_event_t *>(event);
            move(QPoint(motionEvent->root_x, motionEvent->root_y));
            break;
        }

        case XCB_BUTTON_RELEASE: {
            const auto buttonEvent = reinterpret_cast<xcb_button_release_event_t *>(event);
            sendDrop(buttonEvent->time);
            break;
        }
        }
    }

    static std::unique_ptr<X11Drag> start(X11Display *display, X11SelectionOwner *selection, xcb_window_t window)
    {
        selection->setOwner(true);

        return std::unique_ptr<X11Drag>(new X11Drag(display, selection, window));
    }

Q_SIGNALS:
    void status(bool accepted, const QRect &rect, xcb_atom_t action);
    void finished(bool accepted, xcb_atom_t action);

public Q_SLOTS:
    void move(const QPoint &position)
    {
        const xcb_window_t target = findXdndAwareWindow(m_display->connection(), m_display->root(), position);

        if (m_target != target) {
            if (m_target != XCB_WINDOW_NONE) {
                sendLeave();
            }
            if (target != XCB_WINDOW_NONE) {
                sendEnter(target, m_selection->mimeTypes());
            }
        }

        if (m_target != XCB_WINDOW_NONE) {
            sendPosition(position, atoms->xdnd_action_copy, m_display->time());
        }
    }

    void sendEnter(xcb_window_t target, const QList<QMimeType> &mimeTypes)
    {
        xcb_client_message_data_t data = {};
        data.data32[0] = m_source;
        data.data32[1] = xdndVersion << 24;

        if (mimeTypes.size() <= 3) {
            for (int i = 0; i < mimeTypes.size(); ++i) {
                data.data32[2 + i] = m_display->mimeTypeToAtom(mimeTypes[i]);
            }
        } else {
            data.data32[1] |= 1;

            QList<xcb_atom_t> atomMimeTypes;
            for (const QMimeType &mimeType : mimeTypes) {
                atomMimeTypes.append(m_display->mimeTypeToAtom(mimeType));
            }

            xcb_change_property(m_display->connection(), XCB_PROP_MODE_REPLACE, m_source, atoms->xdnd_type_list, XCB_ATOM_ATOM, 32, atomMimeTypes.size(), atomMimeTypes.constData());
        }

        m_target = target;
        sendXdndEvent(m_display->connection(), m_target, atoms->xdnd_enter, &data);
    }

    void sendLeave()
    {
        xcb_client_message_data_t data = {};
        data.data32[0] = m_source;

        sendXdndEvent(m_display->connection(), m_target, atoms->xdnd_leave, &data);
        m_target = XCB_WINDOW_NONE;
    }

    void sendPosition(const QPoint &position, xcb_atom_t action, uint32_t timestamp)
    {
        xcb_client_message_data_t data = {};
        data.data32[0] = m_source;
        data.data32[1] = 0;
        data.data32[2] = (position.x() << 16) | position.y();
        data.data32[3] = timestamp;
        data.data32[4] = action;

        sendXdndEvent(m_display->connection(), m_target, atoms->xdnd_position, &data);
    }

    void sendDrop(uint32_t timestamp)
    {
        xcb_client_message_data_t data = {};
        data.data32[0] = m_source;
        data.data32[1] = timestamp;

        sendXdndEvent(m_display->connection(), m_target, atoms->xdnd_drop, &data);
    }

private:
    void onXdndStatus(const xcb_client_message_event_t *event)
    {
        Q_EMIT status(event->data.data32[1] & 1, QRect(event->data.data32[2] >> 16, event->data.data32[2] & 0xffff, event->data.data32[3] >> 16, event->data.data32[3] & 0xffff), event->data.data32[4]);
    }

    void onXdndFinished(const xcb_client_message_event_t *event)
    {
        Q_EMIT finished(event->data.data32[1], event->data.data32[2]);
    }

    X11SelectionOwner *m_selection;
    xcb_window_t m_source;
    xcb_window_t m_target = XCB_WINDOW_NONE;
};

class X11DropHandler : public QObject, public X11Object
{
    Q_OBJECT

public:
    explicit X11DropHandler(X11Display *display, xcb_window_t target)
        : X11Object(display)
        , m_target(target)
    {
    }

    void handle(xcb_generic_event_t *event) override
    {
        const uint8_t eventType = XCB_EVENT_RESPONSE_TYPE(event);
        if (eventType == XCB_CLIENT_MESSAGE) {
            const auto clientMessageEvent = reinterpret_cast<xcb_client_message_event_t *>(event);
            if (clientMessageEvent->window == m_target) {
                if (clientMessageEvent->type == atoms->xdnd_enter) {
                    onXdndEnter(clientMessageEvent);
                } else if (clientMessageEvent->type == atoms->xdnd_leave) {
                    onXdndLeave(clientMessageEvent);
                } else if (clientMessageEvent->type == atoms->xdnd_position) {
                    onXdndPosition(clientMessageEvent);
                } else if (clientMessageEvent->type == atoms->xdnd_drop) {
                    onXdndDrop(clientMessageEvent);
                }
            }
        }
    }

public Q_SLOTS:
    void sendFinish(bool accepted, xcb_atom_t action)
    {
        xcb_client_message_data_t data = {};
        data.data32[0] = m_target;
        data.data32[1] = accepted;
        if (accepted) {
            data.data32[2] = action;
        }

        sendXdndEvent(m_display->connection(), m_source, atoms->xdnd_finished, &data);
    }

    void sendStatus(bool accepted, const QRect &rect, xcb_atom_t action)
    {
        xcb_client_message_data_t data = {};
        data.data32[0] = m_target;
        data.data32[1] = accepted;
        data.data32[2] = (rect.x() << 16) | rect.y();
        data.data32[3] = (rect.width() << 16) | rect.height();
        data.data32[4] = action;

        sendXdndEvent(m_display->connection(), m_source, atoms->xdnd_status, &data);
    }

Q_SIGNALS:
    void entered(const QList<QMimeType> &mimeTypes);
    void left();
    void position(const QPoint &position, xcb_atom_t action, uint32_t timestamp);
    void dropped(uint32_t timestamp);

private:
    QList<xcb_atom_t> mimeTypeListFromWindow(xcb_window_t dragSource) const
    {
        const uint32_t length = 0x1fffffff;
        xcb_get_property_cookie_t cookie = xcb_get_property(m_display->connection(), 0, dragSource, atoms->xdnd_type_list, XCB_ATOM_ATOM, 0, length);
        xcb_get_property_reply_t *reply = xcb_get_property_reply(m_display->connection(), cookie, nullptr);
        if (!reply) {
            return QList<xcb_atom_t>();
        }

        const xcb_atom_t *mimeAtoms = static_cast<xcb_atom_t *>(xcb_get_property_value(reply));
        const QList<xcb_atom_t> atoms(mimeAtoms, mimeAtoms + reply->value_len);
        free(reply);

        return atoms;
    }

    void onXdndEnter(const xcb_client_message_event_t *event)
    {
        m_source = event->data.data32[0];

        QList<QMimeType> mimeTypes;
        if (event->data.data32[1] & 1) {
            const QList<xcb_atom_t> atoms = mimeTypeListFromWindow(m_source);
            for (const xcb_atom_t &atom : atoms) {
                mimeTypes.append(m_display->atomToMimeType(atom));
            }
        } else {
            for (int i = 0; i < 3; ++i) {
                const xcb_atom_t atom = event->data.data32[2 + i];
                if (atom == XCB_ATOM_NONE) {
                    break;
                }
                mimeTypes.append(m_display->atomToMimeType(atom));
            }
        }

        Q_EMIT entered(mimeTypes);
    }

    void onXdndLeave(const xcb_client_message_event_t *event)
    {
        m_source = XCB_WINDOW_NONE;
        Q_EMIT left();
    }

    void onXdndPosition(const xcb_client_message_event_t *event)
    {
        Q_EMIT position(QPoint(event->data.data32[2] >> 16, event->data.data32[2] & 0xffff), event->data.data32[3], event->data.data32[4]);
    }

    void onXdndDrop(const xcb_client_message_event_t *event)
    {
        Q_EMIT dropped(event->data.data32[1]);
    }

    xcb_window_t m_target;
    xcb_window_t m_source = XCB_WINDOW_NONE;
};

class X11Pointer : public QObject, public X11Object
{
    Q_OBJECT

public:
    explicit X11Pointer(X11Display *display)
        : X11Object(display)
    {
    }

    void handle(xcb_generic_event_t *event) override
    {
        switch (XCB_EVENT_RESPONSE_TYPE(event)) {
        case XCB_ENTER_NOTIFY: {
            const auto enterEvent = reinterpret_cast<xcb_enter_notify_event_t *>(event);
            Q_EMIT entered(enterEvent->event, QPoint(enterEvent->root_x, enterEvent->root_y));
            break;
        }
        case XCB_LEAVE_NOTIFY: {
            const auto leaveEvent = reinterpret_cast<xcb_leave_notify_event_t *>(event);

            const int buttonMask = XCB_BUTTON_MASK_1 | XCB_BUTTON_MASK_2 | XCB_BUTTON_MASK_3;
            if (!(leaveEvent->state & buttonMask)) {
                Q_EMIT left(leaveEvent->event);
            }
            break;
        }
        case XCB_MOTION_NOTIFY: {
            const auto motionEvent = reinterpret_cast<xcb_motion_notify_event_t *>(event);
            Q_EMIT motion(QPoint(motionEvent->root_x, motionEvent->root_y));
            break;
        }
        case XCB_BUTTON_PRESS: {
            const auto buttonEvent = reinterpret_cast<xcb_button_press_event_t *>(event);
            Q_EMIT pressed(buttonEvent->detail, QPoint(buttonEvent->root_x, buttonEvent->root_y));
            break;
        }
        case XCB_BUTTON_RELEASE: {
            const auto buttonEvent = reinterpret_cast<xcb_button_release_event_t *>(event);
            Q_EMIT released(buttonEvent->detail);
            break;
        }
        }
    }

Q_SIGNALS:
    void entered(xcb_window_t window, const QPoint &position);
    void left(xcb_window_t window);
    void motion(const QPointF &position);
    void pressed(xcb_button_t button, const QPoint &position);
    void released(xcb_button_t button);
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

class XwaylandDndTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void x11ToX11();
    void x11ToWayland_data();
    void x11ToWayland();
    void noAcceptedMimeTypeX11ToWayland();
    void destroyX11ToWaylandSource();
    void waylandToX11_data();
    void waylandToX11();
    void noAcceptedMimeTypeWaylandToX11();
    void destroyWaylandToX11Source();
    void cancelWaylandToX11();
    void waylandToXdndUnawareWindow();

private:
    QMimeDatabase m_mimeDb;
};

static void maybeResetXwaylandCursor()
{
    if (kwinApp()->x11Connection()) {
        xcb_warp_pointer(kwinApp()->x11Connection(), XCB_WINDOW_NONE, kwinApp()->x11RootWindow(), 0, 0, 0, 0, 640, 512);
        xcb_flush(kwinApp()->x11Connection());
    }
}

void XwaylandDndTest::initTestCase()
{
    qRegisterMetaType<Window *>();

    connect(kwinApp(), &Application::x11ConnectionChanged, this, []() {
        maybeResetXwaylandCursor();
    });

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

void XwaylandDndTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::DataDeviceManager));

    workspace()->setActiveOutput(QPoint(640, 512));
    input()->pointer()->warp(QPoint(640, 512));
    maybeResetXwaylandCursor();
}

void XwaylandDndTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void XwaylandDndTest::x11ToX11()
{
    // This test verifies that drag-and-drop between X11 clients works as expected.

    auto x11Display = X11Display::create();
    QVERIFY(x11Display);

    // Initialize the source window.
    auto sourceWindow = createXdndAwareTestWindow(x11Display->connection(), QRect(0, 0, 100, 100));
    QVERIFY(sourceWindow);

    X11DropHandler sourceDropHandler(x11Display.get(), sourceWindow->window());
    QSignalSpy sourceDropHandlerEnteredSpy(&sourceDropHandler, &X11DropHandler::entered);
    QSignalSpy sourceDropHandlerLeftSpy(&sourceDropHandler, &X11DropHandler::left);
    QSignalSpy sourceDropHandlerPositionSpy(&sourceDropHandler, &X11DropHandler::position);
    QSignalSpy sourceDropHandlerDroppedSpy(&sourceDropHandler, &X11DropHandler::dropped);

    // Initialize the target window.
    auto targetWindow = createXdndAwareTestWindow(x11Display->connection(), QRect(100, 0, 100, 100));
    QVERIFY(targetWindow);

    X11DropHandler targetDropHandler(x11Display.get(), targetWindow->window());
    QSignalSpy targetDropHandlerEnteredSpy(&targetDropHandler, &X11DropHandler::entered);
    QSignalSpy targetDropHandlerLeftSpy(&targetDropHandler, &X11DropHandler::left);
    QSignalSpy targetDropHandlerPositionSpy(&targetDropHandler, &X11DropHandler::position);
    QSignalSpy targetDropHandlerDroppedSpy(&targetDropHandler, &X11DropHandler::dropped);

    // Move the pointer to the center of the source window and initiate drag-and-drop.
    X11Pointer pointer(x11Display.get());
    QSignalSpy enteredSpy(&pointer, &X11Pointer::entered);
    QSignalSpy leftSpy(&pointer, &X11Pointer::left);
    QSignalSpy motionSpy(&pointer, &X11Pointer::motion);
    QSignalSpy pressedSpy(&pointer, &X11Pointer::pressed);
    QSignalSpy releasedSpy(&pointer, &X11Pointer::released);

    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    QVERIFY(enteredSpy.wait());
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(pressedSpy.wait());

    const QMimeType plainText = m_mimeDb.mimeTypeForName(QStringLiteral("text/plain"));
    X11SelectionOwner x11DragSelection(x11Display.get(), atoms->xdnd_selection, {plainText}, [plainText](const QMimeType &mimeType) {
        if (mimeType == plainText) {
            return X11SelectionData{
                .data = QByteArrayLiteral("x11 -> x11"),
                .type = atoms->text,
                .format = 8,
            };
        }
        return X11SelectionData();
    });
    auto x11Drag = X11Drag::start(x11Display.get(), &x11DragSelection, sourceWindow->window());
    x11Drag->move(pressedSpy.last().at(1).value<QPoint>());
    QVERIFY(sourceDropHandlerPositionSpy.wait());
    QCOMPARE(sourceDropHandlerEnteredSpy.count(), 1);
    QCOMPARE(sourceDropHandlerLeftSpy.count(), 0);
    QCOMPARE(sourceDropHandlerPositionSpy.count(), 1);
    QCOMPARE(sourceDropHandlerPositionSpy.last().at(0).value<QPoint>(), QPoint(50, 50));
    sourceDropHandler.sendStatus(true, QRect(), atoms->xdnd_action_copy);

    // Move the pointer a little bit in the source window.
    Test::pointerMotion(QPointF(75, 50), timestamp++);
    QVERIFY(sourceDropHandlerPositionSpy.wait());
    QCOMPARE(sourceDropHandlerEnteredSpy.count(), 1);
    QCOMPARE(sourceDropHandlerLeftSpy.count(), 0);
    QCOMPARE(sourceDropHandlerPositionSpy.count(), 2);
    QCOMPARE(sourceDropHandlerPositionSpy.last().at(0).value<QPoint>(), QPoint(75, 50));
    sourceDropHandler.sendStatus(true, QRect(), atoms->xdnd_action_copy);

    // Move the pointer to the target window.
    Test::pointerMotion(QPointF(125, 50), timestamp++);
    QVERIFY(targetDropHandlerPositionSpy.wait());
    QCOMPARE(targetDropHandlerEnteredSpy.count(), 1);
    QCOMPARE(targetDropHandlerLeftSpy.count(), 0);
    QCOMPARE(targetDropHandlerPositionSpy.count(), 1);
    QCOMPARE(targetDropHandlerPositionSpy.last().at(0).value<QPoint>(), QPoint(125, 50));
    QCOMPARE(sourceDropHandlerLeftSpy.count(), 1);
    targetDropHandler.sendStatus(true, QRect(), atoms->xdnd_action_copy);

    // Move the pointer a little bit in the target window.
    Test::pointerMotion(QPointF(150, 50), timestamp++);
    QVERIFY(targetDropHandlerPositionSpy.wait());
    QCOMPARE(targetDropHandlerEnteredSpy.count(), 1);
    QCOMPARE(targetDropHandlerLeftSpy.count(), 0);
    QCOMPARE(targetDropHandlerPositionSpy.count(), 2);
    QCOMPARE(targetDropHandlerPositionSpy.last().at(0).value<QPoint>(), QPoint(150, 50));
    targetDropHandler.sendStatus(true, QRect(), atoms->xdnd_action_copy);

    // Drop.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(targetDropHandlerDroppedSpy.wait());
    QCOMPARE(sourceDropHandlerDroppedSpy.count(), 0);

    // Ask for data.
    const xcb_timestamp_t dropTimestamp = targetDropHandlerDroppedSpy.last().at(0).value<xcb_timestamp_t>();
    const QByteArray data = X11SelectionReader::read(x11Display.get(), targetWindow->window(), atoms->xdnd_selection, x11Display->mimeTypeToAtom(plainText), atoms->wl_selection, dropTimestamp);
    QCOMPARE(data, QByteArrayLiteral("x11 -> x11"));

    // Finish drag-and-drop.
    QSignalSpy dragSourceFinishedSpy(x11Drag.get(), &X11Drag::finished);
    targetDropHandler.sendFinish(true, atoms->xdnd_action_copy);
    QVERIFY(dragSourceFinishedSpy.wait());
}

void XwaylandDndTest::x11ToWayland_data()
{
    QTest::addColumn<QList<QMimeType>>("offeredMimeTypes");
    QTest::addColumn<QMimeType>("acceptedMimeType");
    QTest::addColumn<QByteArray>("acceptedMimeData");

    const QMimeType imagePng = m_mimeDb.mimeTypeForName(QStringLiteral("image/png"));
    const QMimeType videoMp4 = m_mimeDb.mimeTypeForName(QStringLiteral("video/mp4"));
    const QMimeType fontOtf = m_mimeDb.mimeTypeForName(QStringLiteral("font/otf"));
    const QMimeType plainText = m_mimeDb.mimeTypeForName(QStringLiteral("text/plain"));

    // This checks a case where the data is transferred with a single XChangeProperty() call.
    QTest::addRow("short text") << QList<QMimeType>{plainText} << plainText << generateText(16);

    // This checks a case where the data is transferred using multiple XChangeProperty() calls.
    QTest::addRow("long text") << QList<QMimeType>{plainText} << plainText << generateText(1024);

    // This checks the XdndTypeList code path.
    QTest::addRow("xdnd type list") << QList<QMimeType>{imagePng, videoMp4, fontOtf, plainText} << plainText << generateText(24);
}

void XwaylandDndTest::x11ToWayland()
{
    // This test verifies that dnd from an x11 window to a wayland window works as expected.
    // Note that the X11 source side will continue receiving pointer events.

    QFETCH(QList<QMimeType>, offeredMimeTypes);
    QFETCH(QMimeType, acceptedMimeType);
    QFETCH(QByteArray, acceptedMimeData);

    QVERIFY(Test::waitForWaylandPointer());

    // Initialize the wayland window.
    auto waylandDataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    QSignalSpy waylandDataDeviceDragEnteredSpy(waylandDataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy waylandDataDeviceDragLeftSpy(waylandDataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy waylandDataDeviceDragMotionSpy(waylandDataDevice, &KWayland::Client::DataDevice::dragMotion);
    QSignalSpy waylandDataDeviceDroppedSpy(waylandDataDevice, &KWayland::Client::DataDevice::dropped);

    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);
    waylandWindow->move(QPointF(0, 0));

    // Initialize the X11 window.
    auto x11Display = X11Display::create();
    QVERIFY(x11Display);

    auto x11Window = createXdndAwareTestWindow(x11Display->connection(), QRect(100, 0, 100, 100));
    QVERIFY(x11Window);

    // Move the pointer to the center of the source X11 window and initiate a drag-and-drop session.
    X11Pointer x11Pointer(x11Display.get());
    QSignalSpy x11PointerEnteredSpy(&x11Pointer, &X11Pointer::entered);
    QSignalSpy x11PointerLeftSpy(&x11Pointer, &X11Pointer::left);
    QSignalSpy x11PointerMotionSpy(&x11Pointer, &X11Pointer::motion);
    QSignalSpy x11PointerPressedSpy(&x11Pointer, &X11Pointer::pressed);
    QSignalSpy x11PointerReleasedSpy(&x11Pointer, &X11Pointer::released);

    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(150, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    KWAIT_TWO(x11PointerEnteredSpy, x11PointerPressedSpy);
    QCOMPARE(x11PointerEnteredSpy.count(), 1);
    QCOMPARE(x11PointerLeftSpy.count(), 0);
    QCOMPARE(x11PointerMotionSpy.count(), 0);
    QCOMPARE(x11PointerPressedSpy.count(), 1);
    QCOMPARE(x11PointerReleasedSpy.count(), 0);

    QSignalSpy dragStartedSpy(waylandServer()->seat(), &SeatInterface::dragStarted);
    X11SelectionOwner x11DragSelection(x11Display.get(), atoms->xdnd_selection, offeredMimeTypes, [&acceptedMimeType, &acceptedMimeData](const QMimeType &mimeType) {
        if (mimeType == acceptedMimeType) {
            return X11SelectionData{
                .data = acceptedMimeData,
                .type = atoms->text,
                .format = 8,
            };
        }
        return X11SelectionData();
    });
    auto x11Drag = X11Drag::start(x11Display.get(), &x11DragSelection, x11Window->window());
    QVERIFY(dragStartedSpy.wait());

    // Move the pointer to the wayland surface.
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    KWAIT_TWO(x11PointerMotionSpy, waylandDataDeviceDragEnteredSpy);

    QCOMPARE(waylandDataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));

    QCOMPARE(x11PointerEnteredSpy.count(), 1);
    QCOMPARE(x11PointerLeftSpy.count(), 0);
    QCOMPARE(x11PointerMotionSpy.count(), 1);
    QCOMPARE(x11PointerMotionSpy.last().at(0).value<QPointF>(), QPointF(50, 50));
    QCOMPARE(x11PointerPressedSpy.count(), 1);
    QCOMPARE(x11PointerReleasedSpy.count(), 0);

    // Move the pointer in the wayland surface a little bit.
    Test::pointerMotion(QPointF(25, 50), timestamp++);
    KWAIT_TWO(x11PointerMotionSpy, waylandDataDeviceDragMotionSpy);

    QCOMPARE(waylandDataDeviceDragMotionSpy.last().at(0).value<QPointF>(), QPointF(25, 50));

    QCOMPARE(x11PointerEnteredSpy.count(), 1);
    QCOMPARE(x11PointerLeftSpy.count(), 0);
    QCOMPARE(x11PointerMotionSpy.count(), 2);
    QCOMPARE(x11PointerMotionSpy.last().at(0).value<QPointF>(), QPointF(25, 50));
    QCOMPARE(x11PointerPressedSpy.count(), 1);
    QCOMPARE(x11PointerReleasedSpy.count(), 0);

    // Accept the offer.
    auto offer = waylandDataDevice->takeDragOffer();
    QCOMPARE(offer->offeredMimeTypes(), offeredMimeTypes);
    QEXPECT_FAIL("", "source actions are not exposed properly to wayland clients", Continue);
    QCOMPARE(offer->sourceDragAndDropActions(), KWayland::Client::DataDeviceManager::DnDAction::Copy);
    offer->accept(acceptedMimeType, waylandDataDeviceDragEnteredSpy.last().at(0).value<quint32>());
    offer->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy, KWayland::Client::DataDeviceManager::DnDAction::Copy);
    QVERIFY(Test::waylandSync());

    // Drop.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    KWAIT_TWO(x11PointerLeftSpy, waylandDataDeviceDroppedSpy);

    QCOMPARE(x11PointerEnteredSpy.count(), 1);
    QCOMPARE(x11PointerLeftSpy.count(), 1);
    QCOMPARE(x11PointerMotionSpy.count(), 2);
    QCOMPARE(x11PointerPressedSpy.count(), 1);
    QCOMPARE(x11PointerReleasedSpy.count(), 1);

    // Ask for data.
    const QFuture<QByteArray> data = readMimeTypeData(offer.get(), acceptedMimeType.name());
    QVERIFY(waitFuture(data));
    QCOMPARE(data.result(), acceptedMimeData);

    // Finish drag and drop.
    QSignalSpy x11DragFinishedSpy(x11Drag.get(), &X11Drag::finished);
    offer->dragAndDropFinished();
    QVERIFY(x11DragFinishedSpy.wait());
}

void XwaylandDndTest::noAcceptedMimeTypeX11ToWayland()
{
    // This test verifies that dnd from an x11 window to a wayland window will be canceled if no mime type has been accepted.

    QVERIFY(Test::waitForWaylandPointer());

    // Initialize the wayland window.
    auto waylandDataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    QSignalSpy waylandDataDeviceDragEnteredSpy(waylandDataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy waylandDataDeviceDragLeftSpy(waylandDataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy waylandDataDeviceDroppedSpy(waylandDataDevice, &KWayland::Client::DataDevice::dropped);

    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);
    waylandWindow->move(QPointF(0, 0));

    // Initialize the X11 window.
    auto x11Display = X11Display::create();
    QVERIFY(x11Display);

    auto x11Window = createXdndAwareTestWindow(x11Display->connection(), QRect(100, 0, 100, 100));
    QVERIFY(x11Window);

    // Move the pointer to the center of the source X11 window and initiate a drag-and-drop session.
    X11Pointer x11Pointer(x11Display.get());
    QSignalSpy x11PointerEnteredSpy(&x11Pointer, &X11Pointer::entered);
    QSignalSpy x11PointerPressedSpy(&x11Pointer, &X11Pointer::pressed);

    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(150, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    KWAIT_TWO(x11PointerEnteredSpy, x11PointerPressedSpy);

    QSignalSpy dragStartedSpy(waylandServer()->seat(), &SeatInterface::dragStarted);
    const QMimeType plainText = m_mimeDb.mimeTypeForName(QStringLiteral("text/plain"));
    X11SelectionOwner x11DragSelection(x11Display.get(), atoms->xdnd_selection, {plainText}, [plainText](const QMimeType &mimeType) {
        if (mimeType == plainText) {
            return X11SelectionData{
                .data = QByteArrayLiteral("x11 -> wayland"),
                .type = atoms->text,
                .format = 8,
            };
        }
        return X11SelectionData();
    });
    auto x11Drag = X11Drag::start(x11Display.get(), &x11DragSelection, x11Window->window());
    QVERIFY(dragStartedSpy.wait());

    // Move the pointer to the wayland surface.
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    QVERIFY(waylandDataDeviceDragEnteredSpy.wait());
    QCOMPARE(waylandDataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));

    // Drop.
    QSignalSpy x11DragFinishedSpy(x11Drag.get(), &X11Drag::finished);
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    KWAIT_TWO(waylandDataDeviceDragLeftSpy, x11DragFinishedSpy);
    QCOMPARE(waylandDataDeviceDroppedSpy.count(), 0);
}

void XwaylandDndTest::destroyX11ToWaylandSource()
{
    // This test verifies that dnd from an x11 window to a wayland window will be canceled if the source side is destroyed.

    QVERIFY(Test::waitForWaylandPointer());

    // Initialize the wayland window.
    auto waylandDataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    QSignalSpy waylandDataDeviceDragEnteredSpy(waylandDataDevice, &KWayland::Client::DataDevice::dragEntered);
    QSignalSpy waylandDataDeviceDragLeftSpy(waylandDataDevice, &KWayland::Client::DataDevice::dragLeft);
    QSignalSpy waylandDataDeviceDroppedSpy(waylandDataDevice, &KWayland::Client::DataDevice::dropped);

    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);
    waylandWindow->move(QPointF(0, 0));

    // Initialize the X11 window.
    auto x11Display = X11Display::create();
    QVERIFY(x11Display);

    auto x11Window = createXdndAwareTestWindow(x11Display->connection(), QRect(100, 0, 100, 100));
    QVERIFY(x11Window);

    // Move the pointer to the center of the source X11 window and initiate a drag-and-drop session.
    auto x11Pointer = std::make_unique<X11Pointer>(x11Display.get());
    QSignalSpy x11PointerEnteredSpy(x11Pointer.get(), &X11Pointer::entered);
    QSignalSpy x11PointerPressedSpy(x11Pointer.get(), &X11Pointer::pressed);

    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(150, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    KWAIT_TWO(x11PointerEnteredSpy, x11PointerPressedSpy);

    QSignalSpy dragStartedSpy(waylandServer()->seat(), &SeatInterface::dragStarted);
    const QMimeType plainText = m_mimeDb.mimeTypeForName(QStringLiteral("text/plain"));
    auto x11DragSelection = std::make_unique<X11SelectionOwner>(x11Display.get(), atoms->xdnd_selection, QList<QMimeType>{plainText}, [plainText](const QMimeType &mimeType) {
        if (mimeType == plainText) {
            return X11SelectionData{
                .data = QByteArrayLiteral("x11 -> wayland"),
                .type = atoms->text,
                .format = 8,
            };
        }
        return X11SelectionData();
    });
    auto x11Drag = X11Drag::start(x11Display.get(), x11DragSelection.get(), x11Window->window());
    QVERIFY(dragStartedSpy.wait());

    // Move the pointer to the wayland surface.
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    QVERIFY(waylandDataDeviceDragEnteredSpy.wait());
    QCOMPARE(waylandDataDeviceDragEnteredSpy.last().at(1).value<QPointF>(), QPointF(50, 50));

    // Pretend that the source crashed.
    x11Drag.reset();
    QEXPECT_FAIL("", "the data bridge does not monitor the source side going down", Continue);
    QVERIFY(waylandDataDeviceDragLeftSpy.wait(100));
    QCOMPARE(waylandDataDeviceDroppedSpy.count(), 0);

    // Release the LMB.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
}

void XwaylandDndTest::waylandToX11_data()
{
    QTest::addColumn<QList<QMimeType>>("offeredMimeTypes");
    QTest::addColumn<QMimeType>("acceptedMimeType");
    QTest::addColumn<QByteArray>("acceptedMimeData");

    const QMimeType imagePng = m_mimeDb.mimeTypeForName(QStringLiteral("image/png"));
    const QMimeType videoMp4 = m_mimeDb.mimeTypeForName(QStringLiteral("video/mp4"));
    const QMimeType fontOtf = m_mimeDb.mimeTypeForName(QStringLiteral("font/otf"));
    const QMimeType plainText = m_mimeDb.mimeTypeForName(QStringLiteral("text/plain"));

    // This checks a case where the data is transferred with a single XChangeProperty() call.
    QTest::addRow("short text") << QList<QMimeType>{plainText} << plainText << generateText(16);

    // This checks a case where the data is transferred with multiple XChangeProperty() calls.
    QTest::addRow("long text") << QList<QMimeType>{plainText} << plainText << generateText(1024);

    // This checks the XdndTypeList code path.
    QTest::addRow("xdnd type list") << QList<QMimeType>{plainText, imagePng, videoMp4, fontOtf} << plainText << generateText(24);
}

void XwaylandDndTest::waylandToX11()
{
    // This test verifies that dnd from a wayland window to an x11 window works as expected.
    // Note that the X11 side will not receive pointer events until a drop is performed.

    QFETCH(QList<QMimeType>, offeredMimeTypes);
    QFETCH(QMimeType, acceptedMimeType);
    QFETCH(QByteArray, acceptedMimeData);

    QVERIFY(Test::waitForWaylandPointer());

    // Initialize the wayland window.
    auto waylandPointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    auto waylandDataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto waylandDataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    for (const QMimeType &mimeType : offeredMimeTypes) {
        waylandDataSource->offer(mimeType);
    }
    waylandDataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    connect(waylandDataSource, &KWayland::Client::DataSource::sendDataRequested, this, [&acceptedMimeType, &acceptedMimeData](const QString &requestedMimeType, int fd) {
        if (requestedMimeType == acceptedMimeType.name()) {
            write(fd, acceptedMimeData.data(), acceptedMimeData.size());
        }
        close(fd);
    });
    QSignalSpy waylandPointerButtonSpy(waylandPointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy waylandDataSourceDragAndDropFinishedSpy(waylandDataSource, &KWayland::Client::DataSource::dragAndDropFinished);

    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);
    waylandWindow->move(QPointF(0, 0));

    // Initialize the X11 window.
    auto x11Display = X11Display::create();
    QVERIFY(x11Display);

    auto x11Window = createXdndAwareTestWindow(x11Display->connection(), QRect(100, 0, 100, 100));
    QVERIFY(x11Window);

    X11Pointer x11Pointer(x11Display.get());
    QSignalSpy x11PointerEnteredSpy(&x11Pointer, &X11Pointer::entered);
    QSignalSpy x11PointerLeftSpy(&x11Pointer, &X11Pointer::left);
    QSignalSpy x11PointerMotionSpy(&x11Pointer, &X11Pointer::motion);
    QSignalSpy x11PointerPressedSpy(&x11Pointer, &X11Pointer::pressed);
    QSignalSpy x11PointerReleasedSpy(&x11Pointer, &X11Pointer::released);

    X11DropHandler x11DropHandler(x11Display.get(), x11Window->window());
    QSignalSpy x11DropHandlerEnteredSpy(&x11DropHandler, &X11DropHandler::entered);
    QSignalSpy x11DropHandlerLeftSpy(&x11DropHandler, &X11DropHandler::left);
    QSignalSpy x11DropHandlerPositionSpy(&x11DropHandler, &X11DropHandler::position);
    QSignalSpy x11DropHandlerDroppedSpy(&x11DropHandler, &X11DropHandler::dropped);

    // Move the pointer to the center of the wayland surface and start a drag-and-drop operation.
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(waylandPointerButtonSpy.wait());

    QSignalSpy dragStartedSpy(waylandServer()->seat(), &SeatInterface::dragStarted);
    waylandDataDevice->startDrag(waylandPointerButtonSpy.last().at(0).value<quint32>(), waylandDataSource, surface.get());
    QVERIFY(dragStartedSpy.wait());

    // Move the pointer to the X11 surface.
    Test::pointerMotion(QPointF(150, 50), timestamp++);
    KWAIT_TWO(x11DropHandlerEnteredSpy, x11DropHandlerPositionSpy);
    QCOMPARE(x11DropHandlerEnteredSpy.last().at(0).value<QList<QMimeType>>(), offeredMimeTypes);
    QCOMPARE(x11DropHandlerPositionSpy.last().at(0).value<QPoint>(), QPoint(150, 50));
    x11DropHandler.sendStatus(true, QRect(), atoms->xdnd_action_copy);

    QCOMPARE(x11PointerEnteredSpy.count(), 0);
    QCOMPARE(x11PointerLeftSpy.count(), 0);
    QCOMPARE(x11PointerMotionSpy.count(), 0);
    QCOMPARE(x11PointerPressedSpy.count(), 0);
    QCOMPARE(x11PointerReleasedSpy.count(), 0);

    // Move the pointer outside the X11 surface.
    Test::pointerMotion(QPointF(250, 50), timestamp++);
    QVERIFY(x11DropHandlerLeftSpy.wait());

    QCOMPARE(x11PointerEnteredSpy.count(), 0);
    QCOMPARE(x11PointerLeftSpy.count(), 0);
    QCOMPARE(x11PointerMotionSpy.count(), 0);
    QCOMPARE(x11PointerPressedSpy.count(), 0);
    QCOMPARE(x11PointerReleasedSpy.count(), 0);

    // Move the pointer to the X11 surface.
    Test::pointerMotion(QPointF(150, 50), timestamp++);
    KWAIT_TWO(x11DropHandlerEnteredSpy, x11DropHandlerPositionSpy);
    QCOMPARE(x11DropHandlerEnteredSpy.last().at(0).value<QList<QMimeType>>(), offeredMimeTypes);
    QCOMPARE(x11DropHandlerPositionSpy.last().at(0).value<QPoint>(), QPoint(150, 50));
    x11DropHandler.sendStatus(true, QRect(), atoms->xdnd_action_copy);

    QCOMPARE(x11PointerEnteredSpy.count(), 0);
    QCOMPARE(x11PointerLeftSpy.count(), 0);
    QCOMPARE(x11PointerMotionSpy.count(), 0);
    QCOMPARE(x11PointerPressedSpy.count(), 0);
    QCOMPARE(x11PointerReleasedSpy.count(), 0);

    // Move the pointer inside the X11 surface a little bit.
    Test::pointerMotion(QPointF(175, 50), timestamp++);
    QVERIFY(x11DropHandlerPositionSpy.wait());
    QCOMPARE(x11DropHandlerPositionSpy.last().at(0).value<QPoint>(), QPoint(175, 50));
    x11DropHandler.sendStatus(true, QRect(), atoms->xdnd_action_copy);

    QCOMPARE(x11PointerEnteredSpy.count(), 0);
    QCOMPARE(x11PointerLeftSpy.count(), 0);
    QCOMPARE(x11PointerMotionSpy.count(), 0);
    QCOMPARE(x11PointerPressedSpy.count(), 0);
    QCOMPARE(x11PointerReleasedSpy.count(), 0);

    // Drop.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    KWAIT_TWO(x11DropHandlerDroppedSpy, x11PointerEnteredSpy);

    QCOMPARE(x11PointerEnteredSpy.count(), 1);
    QCOMPARE(x11PointerLeftSpy.count(), 0);
    QCOMPARE(x11PointerMotionSpy.count(), 0);
    QCOMPARE(x11PointerPressedSpy.count(), 0);
    QCOMPARE(x11PointerReleasedSpy.count(), 0);

    // Ask for data.
    const xcb_timestamp_t dropTimestamp = x11DropHandlerDroppedSpy.last().at(0).value<xcb_timestamp_t>();
    const QByteArray actualData = X11SelectionReader::read(x11Display.get(), x11Window->window(), atoms->xdnd_selection, x11Display->mimeTypeToAtom(acceptedMimeType), atoms->wl_selection, dropTimestamp);
    QCOMPARE(actualData, acceptedMimeData);

    // Finish the drag-and-drop operation.
    x11DropHandler.sendFinish(true, atoms->xdnd_action_copy);
    QVERIFY(waylandDataSourceDragAndDropFinishedSpy.wait());
}

void XwaylandDndTest::noAcceptedMimeTypeWaylandToX11()
{
    // This test verifies that dnd from a wayland window to an x11 window will be canceled if no mime type has been accepted.

    QVERIFY(Test::waitForWaylandPointer());

    // Initialize the wayland window.
    auto waylandPointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    auto waylandDataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto waylandDataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    waylandDataSource->offer(QStringLiteral("text/plain"));
    waylandDataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    connect(waylandDataSource, &KWayland::Client::DataSource::sendDataRequested, this, [](const QString &mimeType, int fd) {
        if (mimeType == QLatin1String("text/plain")) {
            const auto payload = QByteArrayLiteral("wayland -> x11");
            write(fd, payload.data(), payload.size());
        }
        close(fd);
    });
    QSignalSpy waylandPointerButtonSpy(waylandPointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy waylandDataSourceCancelledSpy(waylandDataSource, &KWayland::Client::DataSource::cancelled);

    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);
    waylandWindow->move(QPointF(0, 0));

    // Initialize the X11 window.
    auto x11Display = X11Display::create();
    QVERIFY(x11Display);

    auto x11Window = createXdndAwareTestWindow(x11Display->connection(), QRect(100, 0, 100, 100));
    QVERIFY(x11Window);

    X11DropHandler x11DropHandler(x11Display.get(), x11Window->window());
    QSignalSpy x11DropHandlerEnteredSpy(&x11DropHandler, &X11DropHandler::entered);
    QSignalSpy x11DropHandlerLeftSpy(&x11DropHandler, &X11DropHandler::left);
    QSignalSpy x11DropHandlerPositionSpy(&x11DropHandler, &X11DropHandler::position);
    QSignalSpy x11DropHandlerDroppedSpy(&x11DropHandler, &X11DropHandler::dropped);

    // Move the pointer to the center of the wayland surface and start a drag-and-drop operation.
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(waylandPointerButtonSpy.wait());

    QSignalSpy dragStartedSpy(waylandServer()->seat(), &SeatInterface::dragStarted);
    waylandDataDevice->startDrag(waylandPointerButtonSpy.last().at(0).value<quint32>(), waylandDataSource, surface.get());
    QVERIFY(dragStartedSpy.wait());

    // Move the pointer to the X11 surface.
    Test::pointerMotion(QPointF(150, 50), timestamp++);
    KWAIT_TWO(x11DropHandlerEnteredSpy, x11DropHandlerPositionSpy);
    x11DropHandler.sendStatus(false, QRect(), XCB_ATOM_NONE);

    // Move the pointer inside the X11 surface a little bit.
    Test::pointerMotion(QPointF(175, 50), timestamp++);
    QVERIFY(x11DropHandlerPositionSpy.wait());
    x11DropHandler.sendStatus(false, QRect(), XCB_ATOM_NONE);

    // Drop.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    KWAIT_TWO(x11DropHandlerLeftSpy, waylandDataSourceCancelledSpy);
    QCOMPARE(x11DropHandlerDroppedSpy.count(), 0);
    QCOMPARE(waylandDataSourceCancelledSpy.count(), 1);
}

void XwaylandDndTest::destroyWaylandToX11Source()
{
    // This test verifies that dnd from a wayland window to an x11 window will be canceled if the data source is destroyed.

    QVERIFY(Test::waitForWaylandPointer());

    // Initialize the wayland window.
    auto waylandPointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    auto waylandDataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto waylandDataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    waylandDataSource->offer(QStringLiteral("text/plain"));
    waylandDataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    connect(waylandDataSource, &KWayland::Client::DataSource::sendDataRequested, this, [](const QString &mimeType, int fd) {
        if (mimeType == QLatin1String("text/plain")) {
            const auto payload = QByteArrayLiteral("wayland -> x11");
            write(fd, payload.data(), payload.size());
        }
        close(fd);
    });
    QSignalSpy waylandPointerButtonSpy(waylandPointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy waylandDataSourceCancelledSpy(waylandDataSource, &KWayland::Client::DataSource::cancelled);

    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);
    waylandWindow->move(QPointF(0, 0));

    // Initialize the X11 window.
    auto x11Display = X11Display::create();
    QVERIFY(x11Display);

    auto x11Window = createXdndAwareTestWindow(x11Display->connection(), QRect(100, 0, 100, 100));
    QVERIFY(x11Window);

    X11DropHandler x11DropHandler(x11Display.get(), x11Window->window());
    QSignalSpy x11DropHandlerEnteredSpy(&x11DropHandler, &X11DropHandler::entered);
    QSignalSpy x11DropHandlerLeftSpy(&x11DropHandler, &X11DropHandler::left);
    QSignalSpy x11DropHandlerPositionSpy(&x11DropHandler, &X11DropHandler::position);
    QSignalSpy x11DropHandlerDroppedSpy(&x11DropHandler, &X11DropHandler::dropped);

    // Move the pointer to the center of the wayland surface and start a drag-and-drop operation.
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(waylandPointerButtonSpy.wait());

    QSignalSpy dragStartedSpy(waylandServer()->seat(), &SeatInterface::dragStarted);
    waylandDataDevice->startDrag(waylandPointerButtonSpy.last().at(0).value<quint32>(), waylandDataSource, surface.get());
    QVERIFY(dragStartedSpy.wait());

    // Move the pointer to the X11 surface.
    Test::pointerMotion(QPointF(150, 50), timestamp++);
    KWAIT_TWO(x11DropHandlerEnteredSpy, x11DropHandlerPositionSpy);
    x11DropHandler.sendStatus(false, QRect(), XCB_ATOM_NONE);

    // Delete the data source.
    delete waylandDataSource;
    waylandDataSource = nullptr;
    QVERIFY(x11DropHandlerLeftSpy.wait());
    QCOMPARE(x11DropHandlerDroppedSpy.count(), 0);

    // Release the LMB.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
}

void XwaylandDndTest::cancelWaylandToX11()
{
    // This test verifies that dnd from a wayland window to an x11 window will be canceled if the user presses the Escape key.

    QVERIFY(Test::waitForWaylandPointer());

    // Initialize the wayland window.
    auto waylandPointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    auto waylandDataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto waylandDataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    waylandDataSource->offer(QStringLiteral("text/plain"));
    waylandDataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    connect(waylandDataSource, &KWayland::Client::DataSource::sendDataRequested, this, [](const QString &mimeType, int fd) {
        if (mimeType == QLatin1String("text/plain")) {
            const auto payload = QByteArrayLiteral("wayland -> x11");
            write(fd, payload.data(), payload.size());
        }
        close(fd);
    });
    QSignalSpy waylandPointerButtonSpy(waylandPointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy waylandDataSourceCancelledSpy(waylandDataSource, &KWayland::Client::DataSource::cancelled);

    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);
    waylandWindow->move(QPointF(0, 0));

    // Initialize the X11 window.
    auto x11Display = X11Display::create();
    QVERIFY(x11Display);

    auto x11Window = createXdndAwareTestWindow(x11Display->connection(), QRect(100, 0, 100, 100));
    QVERIFY(x11Window);

    X11DropHandler x11DropHandler(x11Display.get(), x11Window->window());
    QSignalSpy x11DropHandlerEnteredSpy(&x11DropHandler, &X11DropHandler::entered);
    QSignalSpy x11DropHandlerLeftSpy(&x11DropHandler, &X11DropHandler::left);
    QSignalSpy x11DropHandlerPositionSpy(&x11DropHandler, &X11DropHandler::position);
    QSignalSpy x11DropHandlerDroppedSpy(&x11DropHandler, &X11DropHandler::dropped);

    // Move the pointer to the center of the wayland surface and start a drag-and-drop operation.
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(waylandPointerButtonSpy.wait());

    QSignalSpy dragStartedSpy(waylandServer()->seat(), &SeatInterface::dragStarted);
    waylandDataDevice->startDrag(waylandPointerButtonSpy.last().at(0).value<quint32>(), waylandDataSource, surface.get());
    QVERIFY(dragStartedSpy.wait());

    // Move the pointer to the X11 surface.
    Test::pointerMotion(QPointF(150, 50), timestamp++);
    KWAIT_TWO(x11DropHandlerEnteredSpy, x11DropHandlerPositionSpy);
    x11DropHandler.sendStatus(false, QRect(), XCB_ATOM_NONE);

    // Cancel the drag-and-drop operation by pressing the Escape key.
    Test::keyboardKeyPressed(KEY_ESC, timestamp++);
    Test::keyboardKeyReleased(KEY_ESC, timestamp++);
    KWAIT_TWO(waylandDataSourceCancelledSpy, x11DropHandlerLeftSpy);
    QCOMPARE(x11DropHandlerDroppedSpy.count(), 0);

    // Release the LMB.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
}

void XwaylandDndTest::waylandToXdndUnawareWindow()
{
    // This test verifies that the data bridge doesn't consider XDND unaware windows as drop targets.

    QVERIFY(Test::waitForWaylandPointer());

    // Initialize the wayland window.
    auto waylandPointer = Test::waylandSeat()->createPointer(Test::waylandSeat());
    auto waylandDataDevice = Test::waylandDataDeviceManager()->getDataDevice(Test::waylandSeat(), Test::waylandSeat());
    auto waylandDataSource = Test::waylandDataDeviceManager()->createDataSource(Test::waylandSeat());
    waylandDataSource->offer(QStringLiteral("text/plain"));
    waylandDataSource->setDragAndDropActions(KWayland::Client::DataDeviceManager::DnDAction::Copy | KWayland::Client::DataDeviceManager::DnDAction::Move);
    connect(waylandDataSource, &KWayland::Client::DataSource::sendDataRequested, this, [](const QString &mimeType, int fd) {
        if (mimeType == QLatin1String("text/plain")) {
            const auto payload = QByteArrayLiteral("wayland -> x11");
            write(fd, payload.data(), payload.size());
        }
        close(fd);
    });
    QSignalSpy waylandPointerButtonSpy(waylandPointer, &KWayland::Client::Pointer::buttonStateChanged);
    QSignalSpy waylandDataSourceCancelledSpy(waylandDataSource, &KWayland::Client::DataSource::cancelled);

    auto surface = Test::createSurface();
    auto shellSurface = Test::createXdgToplevelSurface(surface.get());
    auto waylandWindow = Test::renderAndWaitForShown(surface.get(), QSize(100, 100), Qt::red);
    QVERIFY(waylandWindow);
    waylandWindow->move(QPointF(0, 0));

    // Initialize the X11 window.
    auto x11Display = X11Display::create();
    QVERIFY(x11Display);

    auto x11Window = createXdndUnawareTestWindow(x11Display->connection(), QRect(100, 0, 100, 100));
    QVERIFY(x11Window);

    X11DropHandler x11DropHandler(x11Display.get(), x11Window->window());
    QSignalSpy x11DropHandlerEnteredSpy(&x11DropHandler, &X11DropHandler::entered);
    QSignalSpy x11DropHandlerLeftSpy(&x11DropHandler, &X11DropHandler::left);
    QSignalSpy x11DropHandlerPositionSpy(&x11DropHandler, &X11DropHandler::position);
    QSignalSpy x11DropHandlerDroppedSpy(&x11DropHandler, &X11DropHandler::dropped);

    // Move the pointer to the center of the wayland surface and start a drag-and-drop operation.
    quint32 timestamp = 0;
    Test::pointerMotion(QPointF(50, 50), timestamp++);
    Test::pointerButtonPressed(BTN_LEFT, timestamp++);
    QVERIFY(waylandPointerButtonSpy.wait());

    QSignalSpy dragStartedSpy(waylandServer()->seat(), &SeatInterface::dragStarted);
    waylandDataDevice->startDrag(waylandPointerButtonSpy.last().at(0).value<quint32>(), waylandDataSource, surface.get());
    QVERIFY(dragStartedSpy.wait());

    // Move the pointer to the X11 surface.
    Test::pointerMotion(QPointF(150, 50), timestamp++);
    QVERIFY(!x11DropHandlerEnteredSpy.wait(10));

    // Move the pointer a little bit in the X11 surface.
    Test::pointerMotion(QPointF(170, 50), timestamp++);
    QVERIFY(!x11DropHandlerPositionSpy.wait(10));

    // Move the pointer outside the X11 surface.
    Test::pointerMotion(QPointF(170, 50), timestamp++);
    QVERIFY(!x11DropHandlerLeftSpy.wait(10));

    // Move the pointer back to the X11 surface.
    Test::pointerMotion(QPointF(150, 50), timestamp++);
    QVERIFY(!x11DropHandlerEnteredSpy.wait(10));

    // Drop.
    Test::pointerButtonReleased(BTN_LEFT, timestamp++);
    QVERIFY(waylandDataSourceCancelledSpy.wait());
    QCOMPARE(x11DropHandlerDroppedSpy.count(), 0);
}

} // namespace KWin

WAYLANDTEST_MAIN(KWin::XwaylandDndTest)
#include "xwayland_dnd_test.moc"
