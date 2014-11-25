/********************************************************************
Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>

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
#include "../src/server/buffer_interface.h"
#include "../src/server/compositor_interface.h"
#include "../src/server/display.h"
#include "../src/server/keyboard_interface.h"
#include "../src/server/output_interface.h"
#include "../src/server/pointer_interface.h"
#include "../src/server/seat_interface.h"
#include "../src/server/shell_interface.h"

#include <QApplication>
#include <QDateTime>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWidget>

class CompositorWindow : public QWidget
{
    Q_OBJECT
public:
    explicit CompositorWindow(QWidget *parent = nullptr);
    virtual ~CompositorWindow();

    void surfaceCreated(KWayland::Server::ShellSurfaceInterface *surface);

    void setSeat(const QPointer<KWayland::Server::SeatInterface> &seat);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QList<KWayland::Server::ShellSurfaceInterface*> m_stackingOrder;
    QPointer<KWayland::Server::SeatInterface> m_seat;
};

CompositorWindow::CompositorWindow(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
}

CompositorWindow::~CompositorWindow() = default;

void CompositorWindow::surfaceCreated(KWayland::Server::ShellSurfaceInterface *surface)
{
    using namespace KWayland::Server;
    m_stackingOrder << surface;
    connect(surface, &ShellSurfaceInterface::fullscreenChanged, this,
        [surface, this](bool fullscreen) {
            if (fullscreen) {
                surface->requestSize(size());
            }
        }
    );
    connect(surface->surface(), &SurfaceInterface::damaged, this, static_cast<void (CompositorWindow::*)()>(&CompositorWindow::update));
    connect(surface, &ShellSurfaceInterface::destroyed, this,
        [surface, this] {
            m_stackingOrder.removeAll(surface);
            update();
        }
    );
}

void CompositorWindow::setSeat(const QPointer< KWayland::Server::SeatInterface > &seat)
{
    m_seat = seat;
}

void CompositorWindow::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QPainter p(this);
    for (auto s : m_stackingOrder) {
        if (auto *b = s->surface()->buffer()) {
            p.drawImage(QPoint(0, 0), b->data());
            s->surface()->frameRendered(QDateTime::currentMSecsSinceEpoch());
        }
    }
}

void CompositorWindow::keyPressEvent(QKeyEvent *event)
{
    QWidget::keyPressEvent(event);
    if (!m_seat) {
        return;
    }
    const auto keyboard = m_seat->keyboard();
    if (!keyboard->focusedSurface()) {
        if (!m_stackingOrder.isEmpty()) {
            keyboard->setFocusedSurface(m_stackingOrder.last()->surface());
        }
    }
    keyboard->updateTimestamp(event->timestamp());
    keyboard->keyPressed(event->nativeScanCode() - 8);
}

void CompositorWindow::keyReleaseEvent(QKeyEvent *event)
{
    QWidget::keyReleaseEvent(event);
    if (!m_seat) {
        return;
    }
    const auto keyboard = m_seat->keyboard();
    keyboard->updateTimestamp(event->timestamp());
    keyboard->keyReleased(event->nativeScanCode() - 8);
}

void CompositorWindow::mouseMoveEvent(QMouseEvent *event)
{
    QWidget::mouseMoveEvent(event);
    const auto pointer = m_seat->pointer();
    if (!m_seat->focusedPointerSurface()) {
        if (!m_stackingOrder.isEmpty()) {
            m_seat->setFocusedPointerSurface(m_stackingOrder.last()->surface());
        }
    }
    m_seat->setTimestamp(event->timestamp());
    pointer->setGlobalPos(event->localPos().toPoint());
}

void CompositorWindow::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
    const auto pointer = m_seat->pointer();
    if (!m_seat->focusedPointerSurface()) {
        if (!m_stackingOrder.isEmpty()) {
            m_seat->setFocusedPointerSurface(m_stackingOrder.last()->surface());
        }
    }
    m_seat->setTimestamp(event->timestamp());
    pointer->buttonPressed(event->button());
}

void CompositorWindow::mouseReleaseEvent(QMouseEvent *event)
{
    QWidget::mouseReleaseEvent(event);
    const auto pointer = m_seat->pointer();
    m_seat->setTimestamp(event->timestamp());
    pointer->buttonReleased(event->button());
}

void CompositorWindow::wheelEvent(QWheelEvent *event)
{
    QWidget::wheelEvent(event);
    const auto pointer = m_seat->pointer();
    m_seat->setTimestamp(event->timestamp());
    const QPoint &angle = event->angleDelta() / (8 * 15);
    if (angle.x() != 0) {
        pointer->axis(Qt::Horizontal, angle.x());
    }
    if (angle.y() != 0) {
        pointer->axis(Qt::Vertical, angle.y());
    }
}

int main(int argc, char **argv)
{
    using namespace KWayland::Server;
    QApplication app(argc, argv);

    Display display;
    display.start();
    CompositorInterface *compositor = display.createCompositor(&display);
    compositor->create();
    ShellInterface *shell = display.createShell();
    shell->create();
    display.createShm();
    OutputInterface *output = display.createOutput(&display);
    output->setPhysicalSize(QSize(400, 300));
    const QSize windowSize(1024, 768);
    output->addMode(windowSize);
    output->create();
    SeatInterface *seat = display.createSeat();
    seat->setHasKeyboard(true);
    seat->setHasPointer(true);
    seat->setName(QStringLiteral("testSeat0"));
    seat->create();

    CompositorWindow compositorWindow;
    compositorWindow.setSeat(seat);
    compositorWindow.setMinimumSize(windowSize);
    compositorWindow.setMaximumSize(windowSize);
    compositorWindow.setGeometry(QRect(QPoint(0, 0), windowSize));
    compositorWindow.show();
    QObject::connect(shell, &ShellInterface::surfaceCreated, &compositorWindow, &CompositorWindow::surfaceCreated);

    return app.exec();
}

#include "renderingservertest.moc"
