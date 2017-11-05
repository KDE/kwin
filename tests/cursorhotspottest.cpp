/*
 * Copyright 2017  Martin Flöser <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <QApplication>
#include <QCursor>
#include <QMouseEvent>
#include <QPainter>
#include <QWidget>

class MouseCursorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MouseCursorWidget();
    ~MouseCursorWidget();

protected:
    void paintEvent(QPaintEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void keyPressEvent(QKeyEvent * event) override;

private:
    QPoint m_cursorPos;
    QCursor m_cursors[5];
    int m_cursorIndex = 0;
};

namespace {
QCursor createCenterHotspotCursor()
{
    QPixmap cursor(64, 64);
    cursor.fill(Qt::transparent);
    QPainter p(&cursor);
    p.setPen(Qt::black);
    const QPoint center = cursor.rect().center();
    p.drawLine(0, center.y(), center.x()-1, center.y());
    p.drawLine(center.x() + 1, center.y(), cursor.width(), center.y());
    p.drawLine(center.x(), 0, center.x(), center.y() -1);
    p.drawLine(center.x(), center.y() + 1, center.x(), cursor.height());
    return QCursor(cursor, 31, 31);
}

QCursor createTopLeftHotspotCursor()
{
    QPixmap cursor(64, 64);
    cursor.fill(Qt::transparent);
    QPainter p(&cursor);
    p.setPen(Qt::black);
    p.drawLine(0, 1, 0, cursor.height());
    p.drawLine(1, 0, cursor.width(), 0);
    return QCursor(cursor, 0, 0);
}

QCursor createTopRightHotspotCursor()
{
    QPixmap cursor(64, 64);
    cursor.fill(Qt::transparent);
    QPainter p(&cursor);
    p.setPen(Qt::black);
    p.drawLine(cursor.width() -1, 1, cursor.width() -1, cursor.height());
    p.drawLine(0, 0, cursor.width() -2, 0);
    return QCursor(cursor, 63, 0);
}

QCursor createButtomRightHotspotCursor()
{
    QPixmap cursor(64, 64);
    cursor.fill(Qt::transparent);
    QPainter p(&cursor);
    p.setPen(Qt::black);
    p.drawLine(cursor.width() -1, 0, cursor.width() -1, cursor.height() - 2);
    p.drawLine(0, cursor.height()-1, cursor.width() -2, cursor.height()-1);
    return QCursor(cursor, 63, 63);
}

QCursor createButtomLeftHotspotCursor()
{
    QPixmap cursor(64, 64);
    cursor.fill(Qt::transparent);
    QPainter p(&cursor);
    p.setPen(Qt::black);
    p.drawLine(0, 0, 0, cursor.height() - 2);
    p.drawLine(1, cursor.height()-1, cursor.width(), cursor.height()-1);
    return QCursor(cursor, 0, 63);
}

}

MouseCursorWidget::MouseCursorWidget()
    : QWidget()
{
    setMouseTracking(true);
    // create cursors
    m_cursors[0] = createCenterHotspotCursor();
    m_cursors[1] = createTopLeftHotspotCursor();
    m_cursors[2] = createTopRightHotspotCursor();
    m_cursors[3] = createButtomRightHotspotCursor();
    m_cursors[4] = createButtomLeftHotspotCursor();

    setCursor(m_cursors[m_cursorIndex]);
}

MouseCursorWidget::~MouseCursorWidget() = default;

void MouseCursorWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter p(this);
    p.fillRect(0, 0, width(), height(), Qt::white);
    if (geometry().contains(m_cursorPos)) {
        p.setPen(Qt::red);
        p.drawPoint(m_cursorPos);
    }
}

void MouseCursorWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_cursorPos = event->pos();
    update();
}

void MouseCursorWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space) {
        m_cursorIndex = (m_cursorIndex + 1) % 5;
        setCursor(m_cursors[m_cursorIndex]);
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MouseCursorWidget widget;
    widget.show();

    return app.exec();
}

#include "cursorhotspottest.moc"
