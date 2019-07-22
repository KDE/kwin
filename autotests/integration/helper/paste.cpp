/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include <QClipboard>
#include <QGuiApplication>
#include <QPainter>
#include <QRasterWindow>
#include <QTimer>

class Window : public QRasterWindow
{
    Q_OBJECT
public:
    explicit Window();
    ~Window() override;

protected:
    void paintEvent(QPaintEvent *event) override;
};

Window::Window()
    : QRasterWindow()
{
}

Window::~Window() = default;

void Window::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.fillRect(0, 0, width(), height(), Qt::blue);
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QObject::connect(app.clipboard(), &QClipboard::changed, &app,
        [] {
            if (qApp->clipboard()->text() == QLatin1String("test")) {
                QTimer::singleShot(100, qApp, &QCoreApplication::quit);
            }
        }
    );
    QScopedPointer<Window> w(new Window);
    w->setGeometry(QRect(0, 0, 100, 200));
    w->show();

    return app.exec();
}

#include "paste.moc"
