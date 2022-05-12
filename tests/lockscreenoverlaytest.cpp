/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "qwayland-kde-lockscreen-overlay-v1.h"
#include <KWindowSystem>
#include <QWaylandClientExtensionTemplate>
#include <QtWidgets>
#include <qpa/qplatformnativeinterface.h>

class WaylandAboveLockscreen : public QWaylandClientExtensionTemplate<WaylandAboveLockscreen>, public QtWayland::kde_lockscreen_overlay_v1
{
public:
    WaylandAboveLockscreen()
        : QWaylandClientExtensionTemplate<WaylandAboveLockscreen>(1)
    {
        QMetaObject::invokeMethod(this, "addRegistryListener");
    }

    void allowWindow(QWindow *window)
    {
        QPlatformNativeInterface *native = qGuiApp->platformNativeInterface();
        wl_surface *surface = reinterpret_cast<wl_surface *>(native->nativeResourceForWindow(QByteArrayLiteral("surface"), window));
        allow(surface);
    }
};

class WidgetAllower : public QObject
{
public:
    WidgetAllower(QWidget *widget)
        : QObject(widget)
        , m_widget(widget)
    {
        widget->installEventFilter(this);
    }

    bool eventFilter(QObject * /*watched*/, QEvent *event) override
    {
        if (auto w = m_widget->windowHandle()) {
            WaylandAboveLockscreen aboveLockscreen;
            Q_ASSERT(aboveLockscreen.isInitialized());
            aboveLockscreen.allowWindow(w);
            deleteLater();
        }
        return false;
    }

    QWidget *const m_widget;
};

int main(int argc, char *argv[])
{

    QApplication app(argc, argv);
    QWidget window1(nullptr, Qt::Window);
    window1.setWindowTitle("Window 1");
    window1.setLayout(new QVBoxLayout);
    QPushButton p("Lock && Raise the Window 2");
    window1.layout()->addWidget(&p);
    window1.show();

    QWidget window2(nullptr, Qt::Window);
    window2.setWindowTitle("Window 2");
    window2.setLayout(new QVBoxLayout);
    QPushButton p2("Close");
    window2.layout()->addWidget(&p2);

    new WidgetAllower(&window2);
    auto raiseWindow2 = [&] {
        KWindowSystem::requestXdgActivationToken(window2.windowHandle(), 0, "lockscreenoverlaytest.desktop");
    };
    QObject::connect(KWindowSystem::self(), &KWindowSystem::xdgActivationTokenArrived, &window2, [&window2](int, const QString &token) {
        KWindowSystem::setCurrentXdgActivationToken(token);
        KWindowSystem::activateWindow(window2.windowHandle());
    });

    QObject::connect(&p, &QPushButton::clicked, &app, [&] {
        QProcess::execute("loginctl", {"lock-session"});
        window2.showFullScreen();
        QTimer::singleShot(3000, &app, raiseWindow2);
    });

    QObject::connect(&p2, &QPushButton::clicked, &window2, &QWidget::close);

    return app.exec();
}
