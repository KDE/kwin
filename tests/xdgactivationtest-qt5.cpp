/*
    SPDX-FileCopyrightText: 2022 Aleix Pol Gonzalez <aleixpol@kde.org>
    SPDX-FileCopyrightText: 2022 Ilya Fedin <fedin-ilja2010@ya.ru>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "qwayland-xdg-activation-v1.h"
#include <QWaylandClientExtensionTemplate>
#include <QtWidgets>
#include <qpa/qplatformnativeinterface.h>

class WaylandXdgActivationTokenV1 : public QObject, public QtWayland::xdg_activation_token_v1
{
    Q_OBJECT
public:
    void xdg_activation_token_v1_done(const QString &token) override
    {
        Q_EMIT done(token);
    }

Q_SIGNALS:
    void done(const QString &token);
};

class WaylandXdgActivationV1 : public QWaylandClientExtensionTemplate<WaylandXdgActivationV1>, public QtWayland::xdg_activation_v1
{
public:
    WaylandXdgActivationV1()
        : QWaylandClientExtensionTemplate<WaylandXdgActivationV1>(1)
    {
        // QWaylandClientExtensionTemplate invokes this with a QueuedConnection but we want shortcuts
        // to be inhibited immediately.
        QMetaObject::invokeMethod(this, "addRegistryListener");
    }

    WaylandXdgActivationTokenV1 *requestXdgActivationToken(wl_seat *seat, struct ::wl_surface *surface, uint32_t serial, const QString &app_id)
    {
        auto wl = get_activation_token();
        auto provider = new WaylandXdgActivationTokenV1;
        provider->init(wl);
        if (surface) {
            provider->set_surface(surface);
        }

        if (!app_id.isEmpty()) {
            provider->set_app_id(app_id);
        }

        if (seat) {
            provider->set_serial(serial, seat);
        }
        provider->commit();
        return provider;
    }
};

int main(int argc, char *argv[])
{

    QApplication app(argc, argv);
    QWidget window1(nullptr, Qt::Window);
    window1.setWindowTitle("Window 1");
    window1.setLayout(new QVBoxLayout);
    QPushButton p("Raise the Window 2");
    window1.layout()->addWidget(&p);
    window1.show();

    QWidget window2(nullptr, Qt::Window);
    window2.setWindowTitle("Window 2");
    window2.show();

    WaylandXdgActivationV1 activation;
    QObject::connect(&p, &QPushButton::clicked, &app, [&] {
        QPlatformNativeInterface *native = qGuiApp->platformNativeInterface();
        auto seat = static_cast<wl_seat *>(native->nativeResourceForIntegration("wl_seat"));
        wl_surface *surface = reinterpret_cast<wl_surface *>(native->nativeResourceForWindow(QByteArrayLiteral("surface"), window1.windowHandle()));
        auto req = activation.requestXdgActivationToken(seat, surface, 0, {});
        QObject::connect(req, &WaylandXdgActivationTokenV1::done, &app, [&activation, &window2](const QString &token) {
            window2.show();
            QPlatformNativeInterface *native = qGuiApp->platformNativeInterface();
            wl_surface *surface = reinterpret_cast<wl_surface *>(native->nativeResourceForWindow(QByteArrayLiteral("surface"), window2.windowHandle()));
            activation.activate(token, surface);
        });
    });

    return app.exec();
}

#include "xdgactivationtest-qt5.moc"
