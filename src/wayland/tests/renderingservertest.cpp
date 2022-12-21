/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include "../compositor_interface.h"
#include "../datadevicemanager_interface.h"
#include "../display.h"
#include "../keyboard_interface.h"
#include "../output_interface.h"
#include "../pointer_interface.h"
#include "../seat_interface.h"
#include "../shmclientbuffer.h"
#include "../xdgshell_interface.h"

#include "fakeoutput.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWidget>
#include <QtConcurrent>

#include <iostream>
#include <unistd.h>

static int startXServer()
{
    const QByteArray process = QByteArrayLiteral("Xwayland");
    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        std::cerr << "FATAL ERROR failed to create pipe to start X Server " << process.constData() << std::endl;
        exit(1);
    }

    pid_t pid = fork();
    if (pid == 0) {
        // child process - should be turned into Xwayland
        // writes to pipe, closes read side
        close(pipeFds[0]);
        char fdbuf[16];
        sprintf(fdbuf, "%d", pipeFds[1]);
        execlp(process.constData(), process.constData(), "-displayfd", fdbuf, "-rootless", (char *)nullptr);
        close(pipeFds[1]);
        exit(20);
    }
    // parent process - this is the wayland server
    // reads from pipe, closes write side
    close(pipeFds[1]);
    return pipeFds[0];
}

static void readDisplayFromPipe(int pipe)
{
    QFile readPipe;
    if (!readPipe.open(pipe, QIODevice::ReadOnly)) {
        std::cerr << "FATAL ERROR failed to open pipe to start X Server XWayland" << std::endl;
        exit(1);
    }
    QByteArray displayNumber = readPipe.readLine();

    displayNumber.prepend(QByteArray(":"));
    displayNumber.remove(displayNumber.size() - 1, 1);
    std::cout << "X-Server started on display " << displayNumber.constData() << std::endl;

    setenv("DISPLAY", displayNumber.constData(), true);

    // close our pipe
    close(pipe);
}

class CompositorWindow : public QWidget
{
    Q_OBJECT
public:
    explicit CompositorWindow(QWidget *parent = nullptr);
    virtual ~CompositorWindow();

    void surfaceCreated(KWaylandServer::XdgToplevelInterface *surface);

    void setSeat(const QPointer<KWaylandServer::SeatInterface> &seat);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void updateFocus();
    QList<KWaylandServer::XdgToplevelInterface *> m_stackingOrder;
    QPointer<KWaylandServer::SeatInterface> m_seat;
};

CompositorWindow::CompositorWindow(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
}

CompositorWindow::~CompositorWindow() = default;

void CompositorWindow::surfaceCreated(KWaylandServer::XdgToplevelInterface *surface)
{
    using namespace KWaylandServer;
    surface->sendConfigure(QSize(), XdgToplevelInterface::States());
    m_stackingOrder << surface;
    connect(surface->surface(), &SurfaceInterface::damaged, this, static_cast<void (CompositorWindow::*)()>(&CompositorWindow::update));
    connect(surface, &XdgToplevelInterface::destroyed, this, [surface, this] {
        m_stackingOrder.removeAll(surface);
        updateFocus();
        update();
    });
    updateFocus();
}

void CompositorWindow::updateFocus()
{
    using namespace KWaylandServer;
    if (!m_seat || m_stackingOrder.isEmpty()) {
        return;
    }
    auto it = std::find_if(m_stackingOrder.constBegin(), m_stackingOrder.constEnd(), [](XdgToplevelInterface *toplevel) {
        return toplevel->surface()->isMapped();
    });
    if (it == m_stackingOrder.constEnd()) {
        return;
    }
    m_seat->notifyPointerEnter((*it)->surface(), m_seat->pointerPos());
    m_seat->setFocusedKeyboardSurface((*it)->surface());
}

void CompositorWindow::setSeat(const QPointer<KWaylandServer::SeatInterface> &seat)
{
    m_seat = seat;
}

void CompositorWindow::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QPainter p(this);
    for (auto window : m_stackingOrder) {
        KWaylandServer::SurfaceInterface *surface = window->surface();
        if (!surface || !surface->isMapped()) {
            continue;
        }
        auto clientBuffer = qobject_cast<KWaylandServer::ShmClientBuffer *>(surface->buffer());
        if (clientBuffer) {
            p.drawImage(QPoint(0, 0), clientBuffer->data());
        }
        surface->frameRendered(QDateTime::currentMSecsSinceEpoch());
    }
}

void CompositorWindow::keyPressEvent(QKeyEvent *event)
{
    QWidget::keyPressEvent(event);
    if (!m_seat) {
        return;
    }
    if (!m_seat->focusedKeyboardSurface()) {
        updateFocus();
    }
    m_seat->setTimestamp(std::chrono::milliseconds(event->timestamp()));
    m_seat->notifyKeyboardKey(event->nativeScanCode() - 8, KWaylandServer::KeyboardKeyState::Pressed);
}

void CompositorWindow::keyReleaseEvent(QKeyEvent *event)
{
    QWidget::keyReleaseEvent(event);
    if (!m_seat) {
        return;
    }
    m_seat->setTimestamp(std::chrono::milliseconds(event->timestamp()));
    m_seat->notifyKeyboardKey(event->nativeScanCode() - 8, KWaylandServer::KeyboardKeyState::Released);
}

void CompositorWindow::mouseMoveEvent(QMouseEvent *event)
{
    QWidget::mouseMoveEvent(event);
    if (!m_seat->focusedPointerSurface()) {
        updateFocus();
    }
    m_seat->setTimestamp(std::chrono::milliseconds(event->timestamp()));
    m_seat->notifyPointerMotion(event->localPos().toPoint());
    m_seat->notifyPointerFrame();
}

void CompositorWindow::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
    if (!m_seat->focusedPointerSurface()) {
        if (!m_stackingOrder.isEmpty()) {
            m_seat->notifyPointerEnter(m_stackingOrder.last()->surface(), event->globalPos());
        }
    }
    m_seat->setTimestamp(std::chrono::milliseconds(event->timestamp()));
    m_seat->notifyPointerButton(event->button(), KWaylandServer::PointerButtonState::Pressed);
    m_seat->notifyPointerFrame();
}

void CompositorWindow::mouseReleaseEvent(QMouseEvent *event)
{
    QWidget::mouseReleaseEvent(event);
    m_seat->setTimestamp(std::chrono::milliseconds(event->timestamp()));
    m_seat->notifyPointerButton(event->button(), KWaylandServer::PointerButtonState::Released);
    m_seat->notifyPointerFrame();
}

void CompositorWindow::wheelEvent(QWheelEvent *event)
{
    QWidget::wheelEvent(event);
    m_seat->setTimestamp(std::chrono::milliseconds(event->timestamp()));
    const QPoint &angle = event->angleDelta() / (8 * 15);
    if (angle.x() != 0) {
        m_seat->notifyPointerAxis(Qt::Horizontal, angle.x(), 1, KWaylandServer::PointerAxisSource::Wheel);
    }
    if (angle.y() != 0) {
        m_seat->notifyPointerAxis(Qt::Vertical, angle.y(), 1, KWaylandServer::PointerAxisSource::Wheel);
    }
    m_seat->notifyPointerFrame();
}

int main(int argc, char **argv)
{
    using namespace KWaylandServer;
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption xwaylandOption(QStringList{QStringLiteral("x"), QStringLiteral("xwayland")}, QStringLiteral("Start a rootless Xwayland server"));
    parser.addOption(xwaylandOption);
    parser.process(app);

    KWaylandServer::Display display;
    display.start();
    new DataDeviceManagerInterface(&display);
    new CompositorInterface(&display, &display);
    XdgShellInterface *shell = new XdgShellInterface(&display);
    display.createShm();

    const QSize windowSize(1024, 768);

    auto outputHandle = std::make_unique<FakeOutput>();
    outputHandle->setPhysicalSize(QSize(269, 202));
    outputHandle->setMode(windowSize, 60000);

    auto outputInterface = std::make_unique<OutputInterface>(&display, outputHandle.get());

    SeatInterface *seat = new SeatInterface(&display);
    seat->setHasKeyboard(true);
    seat->setHasPointer(true);
    seat->setName(QStringLiteral("testSeat0"));

    CompositorWindow compositorWindow;
    compositorWindow.setSeat(seat);
    compositorWindow.setMinimumSize(windowSize);
    compositorWindow.setMaximumSize(windowSize);
    compositorWindow.setGeometry(QRect(QPoint(0, 0), windowSize));
    compositorWindow.show();
    QObject::connect(shell, &XdgShellInterface::toplevelCreated, &compositorWindow, &CompositorWindow::surfaceCreated);

    // start XWayland
    if (parser.isSet(xwaylandOption)) {
        // starts XWayland by forking and opening a pipe
        const int pipe = startXServer();
        if (pipe == -1) {
            exit(1);
        }

        QtConcurrent::run([pipe] {
            readDisplayFromPipe(pipe);
        });
    }

    return app.exec();
}

#include "renderingservertest.moc"
