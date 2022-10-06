/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "qwayland-xdg-shell.h"
#include "qwayland-xx-session-management-v1.h"

#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QPainter>
#include <QWidget>
#include <QWindow>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>
#include <qpa/qplatformwindow_p.h>

class XdgSessionManagerV1 : public QWaylandClientExtensionTemplate<XdgSessionManagerV1, &QtWayland::xx_session_manager_v1::destroy>, public QtWayland::xx_session_manager_v1
{
public:
    XdgSessionManagerV1()
        : QWaylandClientExtensionTemplate<XdgSessionManagerV1, &QtWayland::xx_session_manager_v1::destroy>(1)
    {
        initialize();
    }
};

class XdgSessionV1 : public QtWayland::xx_session_v1
{
public:
    ~XdgSessionV1() override
    {
        destroy();
    }

protected:
    void xx_session_v1_created(const QString &id) override
    {
        qDebug() << "xx_session_v1 created" << id;
    }

    void xx_session_v1_restored() override
    {
        qDebug() << "xx_session_v1 restored";
    }
};

class XdgToplevelSessionV1 : public QtWayland::xx_toplevel_session_v1
{
public:
    ~XdgToplevelSessionV1() override
    {
        destroy();
    }

protected:
    void xx_toplevel_session_v1_restored(struct ::xdg_toplevel *surface) override
    {
        qDebug() << "xx_toplevel_session_v1 restored";
    }
};

class XdgSessionDemo : public QObject
{
    Q_OBJECT

public:
    ~XdgSessionDemo();
    void setSessionId(const QString &sessionId);
    void setToplevelId(const QString &toplevelId);
    void start();

private:
    QString m_sessionId;
    QString m_toplevelId;

    XdgSessionManagerV1 m_sessionManager;
    XdgSessionV1 m_session;
    XdgToplevelSessionV1 m_toplevelSession;
};

XdgSessionDemo::~XdgSessionDemo()
{
}

void XdgSessionDemo::setSessionId(const QString &sessionId)
{
    m_sessionId = sessionId;
}

void XdgSessionDemo::setToplevelId(const QString &toplevelId)
{
    m_toplevelId = toplevelId;
}

void XdgSessionDemo::start()
{
    QWidget *window = new QWidget(nullptr);
    if (!m_sessionManager.isActive()) {
        qCritical() << "No session manager available, this test cannot do nothing useful";
    }
    if (m_sessionId.isEmpty()) {
        m_session.init(m_sessionManager.get_session(XdgSessionManagerV1::reason_launch, QString()));
    } else {
        m_session.init(m_sessionManager.get_session(XdgSessionManagerV1::reason_session_restore, m_sessionId));
    }
    window->resize(200, 200);

    Q_ASSERT(!m_toplevelId.isEmpty());

    window->createWinId();
    if (auto waylandWindow = window->windowHandle()->nativeInterface<QNativeInterface::Private::QWaylandWindow>()) {
        connect(waylandWindow, &QNativeInterface::Private::QWaylandWindow::surfaceRoleCreated, this, [this, waylandWindow]() {
            auto topLevelObject = waylandWindow->surfaceRole<xdg_toplevel>();
            Q_ASSERT(topLevelObject);
            m_toplevelSession.init(m_session.restore_toplevel(topLevelObject, m_toplevelId));
        });
    } else {
        qCritical() << "No wayland window interface available";
    }
    window->show();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();

    QCommandLineOption sessionIdOption(QStringLiteral("session-id"),
                                       QStringLiteral("The session id"),
                                       QStringLiteral("session-id"));
    parser.addOption(sessionIdOption);

    QCommandLineOption toplevelIdOption(QStringLiteral("toplevel-id"),
                                        QStringLiteral("The toplevel id"),
                                        QStringLiteral("toplevel-id"));
    toplevelIdOption.setDefaultValue(QStringLiteral("main"));
    parser.addOption(toplevelIdOption);

    parser.process(app);

    XdgSessionDemo client;
    client.setSessionId(parser.value(sessionIdOption));
    client.setToplevelId(parser.value(toplevelIdOption));
    client.start();

    return app.exec();
}

#include "xdgsessiontest.moc"
