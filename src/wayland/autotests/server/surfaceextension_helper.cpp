/*
    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#include <QGuiApplication>
#include <QPainter>
#include <QRasterWindow>

class Window : public QRasterWindow
{
    Q_OBJECT
public:
    explicit Window();
    virtual ~Window();

protected:
    void paintEvent(QPaintEvent *event) override;
};

Window::Window()
    : QRasterWindow()
{
    setGeometry(QRect(0, 0, 200, 200));
}

Window::~Window() = default;

void Window::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.fillRect(QRect(QPoint(0, 0), size()), Qt::black);
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("wayland"));
    QGuiApplication app(argc, argv);

    QScopedPointer<Window> w(new Window);
    w->show();

    return app.exec();
}

#include "surfaceextension_helper.moc"
