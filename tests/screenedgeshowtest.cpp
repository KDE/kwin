/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
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
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QToolButton>
#include <QWidget>
#include "../xcbutils.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationDisplayName(QStringLiteral("Screen Edge Show Test App"));

    QScopedPointer<QWidget> widget(new QWidget(nullptr, Qt::FramelessWindowHint));

    KWin::Xcb::Atom atom(QByteArrayLiteral("_KDE_NET_WM_SCREEN_EDGE_SHOW"));

    uint32_t value = 2;
    QPushButton *hideWindowButton = new QPushButton(QStringLiteral("Hide"), widget.data());
    QObject::connect(hideWindowButton, &QPushButton::clicked, [&widget, &atom, &value]() {
        xcb_change_property(QX11Info::connection(), XCB_PROP_MODE_REPLACE, widget->winId(), atom, XCB_ATOM_CARDINAL, 32, 1, &value);
    });
    QPushButton *hideAndRestoreButton = new QPushButton(QStringLiteral("Hide and Restore after 10 sec"), widget.data());
    QTimer *restoreTimer = new QTimer(hideAndRestoreButton);
    restoreTimer->setSingleShot(true);
    QObject::connect(hideAndRestoreButton, &QPushButton::clicked, [&widget, &atom, &value, restoreTimer]() {
        xcb_change_property(QX11Info::connection(), XCB_PROP_MODE_REPLACE, widget->winId(), atom, XCB_ATOM_CARDINAL, 32, 1, &value);
        restoreTimer->start(10000);
    });
    QObject::connect(restoreTimer, &QTimer::timeout, [&widget, &atom]() {
        xcb_delete_property(QX11Info::connection(), widget->winId(), atom);
    });

    QToolButton *edgeButton = new QToolButton(widget.data());
    edgeButton->setText(QStringLiteral("Edge"));
    edgeButton->setPopupMode(QToolButton::MenuButtonPopup);
    QMenu *edgeButtonMenu = new QMenu(edgeButton);
    QObject::connect(edgeButtonMenu->addAction("Top"), &QAction::triggered, [&widget, &value]() {
        const QRect geo = QGuiApplication::primaryScreen()->geometry();
        widget->setGeometry(geo.x(), geo.y(), geo.width(), 100);
        value = 0;
    });
    QObject::connect(edgeButtonMenu->addAction("Right"), &QAction::triggered, [&widget, &value]() {
        const QRect geo = QGuiApplication::primaryScreen()->geometry();
        widget->setGeometry(geo.x() + geo.width() - 100, geo.y(), 100, geo.height());
        value = 1;
    });
    QObject::connect(edgeButtonMenu->addAction("Bottom"), &QAction::triggered, [&widget, &value]() {
        const QRect geo = QGuiApplication::primaryScreen()->geometry();
        widget->setGeometry(geo.x(), geo.y() + geo.height() - 100, geo.width(), 100);
        value = 2;
    });
    QObject::connect(edgeButtonMenu->addAction("Left"), &QAction::triggered, [&widget, &value]() {
        const QRect geo = QGuiApplication::primaryScreen()->geometry();
        widget->setGeometry(geo.x(), geo.y(), 100, geo.height());
        value = 3;
    });
    edgeButtonMenu->addSeparator();
    QObject::connect(edgeButtonMenu->addAction("Floating"), &QAction::triggered, [&widget, &value]() {
        const QRect geo = QGuiApplication::primaryScreen()->geometry();
        widget->setGeometry(QRect(geo.center(), QSize(100, 100)));
        value = 4;
    });
    edgeButton->setMenu(edgeButtonMenu);

    QHBoxLayout *layout = new QHBoxLayout(widget.data());
    layout->addWidget(hideWindowButton);
    layout->addWidget(hideAndRestoreButton);
    layout->addWidget(edgeButton);
    widget->setLayout(layout);

    const QRect geo = QGuiApplication::primaryScreen()->geometry();
    widget->setGeometry(geo.x(), geo.y() + geo.height() - 100, geo.width(), 100);
    widget->show();

    return app.exec();
}
