/*
    SPDX-FileCopyrightText: 2025 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "pip.h"
#include "pipshellsurface.h"

#include <QMouseEvent>
#include <QPainter>

PipPin::PipPin(QWidget *parent)
    : QWidget(parent)
{
    resize(100, 50);
}

bool PipPin::isPinned() const
{
    return m_pinned;
}

void PipPin::setPinned(bool pinned)
{
    if (m_pinned != pinned) {
        m_pinned = pinned;
        update();
    }
}

void PipPin::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setClipRegion(event->region());

    if (m_hovered) {
        painter.setOpacity(1.0);
    } else {
        painter.setOpacity(0.5);
    }

    painter.fillRect(rect(), Qt::black);
    painter.setPen(Qt::white);
    painter.drawText(rect(), Qt::AlignCenter, m_pinned ? QStringLiteral("Unpin") : QStringLiteral("Pin"));
}

void PipPin::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        event->accept();
        Q_EMIT clicked();
    }
}

void PipPin::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
}

void PipPin::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
}

Media::Media(QWidget *parent)
    : QWidget(parent)
{
    m_pip = std::make_unique<Pip>();
    connect(m_pip.get(), &Pip::pinned, this, [this]() {
        m_pin->setPinned(true);
    });
    connect(m_pip.get(), &Pip::unpinned, this, [this]() {
        m_pin->setPinned(false);
    });

    m_pin = new PipPin(this);
    connect(m_pin, &PipPin::clicked, this, [this]() {
        if (m_pin->isPinned()) {
            m_pip->show();
        } else {
            m_pip->hide();
        }
    });
}

void Media::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setClipRegion(event->region());
    painter.fillRect(rect(), QColor(0, 0, 0, 128));
}

void Media::resizeEvent(QResizeEvent *event)
{
    m_pin->move(width() - m_pin->width() - 50, height() - m_pin->height() - 50);
    m_pip->resize(width(), height());
}

PipResizeHandle::PipResizeHandle(Qt::Edges edges, QWidget *parent)
    : QWidget(parent)
    , m_edges(edges)
{
    switch (edges) {
    case Qt::LeftEdge:
    case Qt::RightEdge:
        setCursor(Qt::SizeHorCursor);
        break;
    case Qt::TopEdge:
    case Qt::BottomEdge:
        setCursor(Qt::SizeVerCursor);
        break;
    case Qt::TopEdge | Qt::LeftEdge:
    case Qt::BottomEdge | Qt::RightEdge:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case Qt::TopEdge | Qt::RightEdge:
    case Qt::BottomEdge | Qt::LeftEdge:
        setCursor(Qt::SizeBDiagCursor);
        break;
    default:
        Q_UNREACHABLE();
    }
}

void PipResizeHandle::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
}

void PipResizeHandle::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
}

void PipResizeHandle::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        event->accept();
        window()->windowHandle()->startSystemResize(m_edges);
    }
}

void PipResizeHandle::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setClipRegion(event->region());
    painter.fillRect(rect(), QColor(222, 137, 190, m_hovered ? 128 : 0));
}

Pip::Pip(QWidget *parent)
    : QWidget(parent)
{
    m_closeButton = new QPushButton(this);
    m_closeButton->setIcon(QIcon::fromTheme(QStringLiteral("dialog-close")));
    m_closeButton->setText(QStringLiteral("Close"));
    connect(m_closeButton, &QPushButton::clicked, this, &Pip::hide);

    m_topLeftResizeHandle = new PipResizeHandle(Qt::TopEdge | Qt::LeftEdge, this);
    m_topResizeHandle = new PipResizeHandle(Qt::TopEdge, this);
    m_topRightResizeHandle = new PipResizeHandle(Qt::TopEdge | Qt::RightEdge, this);
    m_rightResizeHandle = new PipResizeHandle(Qt::RightEdge, this);
    m_bottomRightResizeHandle = new PipResizeHandle(Qt::BottomEdge | Qt::BottomEdge, this);
    m_bottomResizeHandle = new PipResizeHandle(Qt::BottomEdge, this);
    m_bottomLeftResizeHandle = new PipResizeHandle(Qt::BottomEdge | Qt::LeftEdge, this);
    m_leftResizeHandle = new PipResizeHandle(Qt::LeftEdge, this);

    winId();
    PipShellIntegration::assignPipRole(windowHandle());
}

void Pip::layout()
{
    const int gridUnit = 5;
    const int resizeZone = 2 * gridUnit;

    m_topLeftResizeHandle->setGeometry(0, 0, resizeZone, resizeZone);
    m_topResizeHandle->setGeometry(resizeZone, 0, width() - 2 * resizeZone, resizeZone);
    m_topRightResizeHandle->setGeometry(width() - resizeZone, 0, resizeZone, resizeZone);
    m_rightResizeHandle->setGeometry(width() - resizeZone, resizeZone, resizeZone, height() - 2 * resizeZone);
    m_bottomRightResizeHandle->setGeometry(width() - resizeZone, height() - resizeZone, resizeZone, resizeZone);
    m_bottomResizeHandle->setGeometry(resizeZone, height() - resizeZone, width() - 2 * resizeZone, resizeZone);
    m_bottomLeftResizeHandle->setGeometry(0, height() - resizeZone, resizeZone, resizeZone);
    m_leftResizeHandle->setGeometry(0, resizeZone, resizeZone, height() - 2 * resizeZone);

    m_closeButton->move(width() - resizeZone - gridUnit - m_closeButton->width(), resizeZone + gridUnit);
}

void Pip::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setClipRegion(event->region());
    painter.fillRect(rect(), QColor(64, 67, 78));
}

void Pip::resizeEvent(QResizeEvent *event)
{
    layout();
}

void Pip::mousePressEvent(QMouseEvent *event)
{
    switch (event->button()) {
    case Qt::LeftButton:
        event->accept();
        windowHandle()->startSystemMove();
        break;
    default:
        break;
    }
}

void Pip::showEvent(QShowEvent *event)
{
    Q_EMIT unpinned();
}

void Pip::hideEvent(QHideEvent *event)
{
    Q_EMIT pinned();
}

#include "moc_pip.cpp"
