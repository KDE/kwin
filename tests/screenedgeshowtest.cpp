/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include <QApplication>
#include <QHBoxLayout>
#include <QMenu>
#include <QPlatformSurfaceEvent>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QToolButton>
#include <QWindow>
#include <QWidget>
#include <QCheckBox>
#include <QX11Info>
#include "../xcbutils.h"

#include <KWindowSystem>

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/plasmashell.h>
#include <KWayland/Client/surface.h>

class ScreenEdgeHelper : public QObject
{
    Q_OBJECT
protected:
    ScreenEdgeHelper(QWidget *widget, QObject *parent = nullptr);

    QWindow *window() const {
        return m_widget->windowHandle();
    }

    virtual void restore() = 0;

public:
    ~ScreenEdgeHelper() override;

    virtual void hide() = 0;
    virtual void raiseOrShow(bool raise) = 0;
    virtual void init() {};

    virtual void moveToTop();
    virtual void moveToRight();
    virtual void moveToBottom();
    virtual void moveToLeft();
    virtual void moveToFloating();

    void hideAndRestore() {
        hide();
        m_timer->start(10000);
    }

private:
    QWidget *m_widget;
    QTimer *m_timer;
};

class ScreenEdgeHelperX11 : public ScreenEdgeHelper
{
    Q_OBJECT
public:
    ScreenEdgeHelperX11(QWidget *widget, QObject *parent = nullptr);
    ~ScreenEdgeHelperX11() override = default;

    void hide() override;
    void raiseOrShow(bool raise) override;

    void moveToTop() override;
    void moveToRight() override;
    void moveToBottom() override;
    void moveToLeft() override;
    void moveToFloating() override;

protected:
    void restore() override;

private:
    uint32_t m_locationValue = 2;
    uint32_t m_actionValue = 0;
    KWin::Xcb::Atom m_atom;
};

class ScreenEdgeHelperWayland : public ScreenEdgeHelper
{
    Q_OBJECT
public:
    ScreenEdgeHelperWayland(QWidget *widget, QObject *parent = nullptr);
    ~ScreenEdgeHelperWayland() override = default;

    void hide() override;
    void raiseOrShow(bool raise) override;
    void init() override;

    bool eventFilter(QObject * watched, QEvent * event) override;

protected:
    void restore() override;

private:
    void setupSurface();
    KWayland::Client::PlasmaShell *m_shell = nullptr;
    KWayland::Client::PlasmaShellSurface *m_shellSurface = nullptr;
    bool m_autoHide = true;
};

ScreenEdgeHelper::ScreenEdgeHelper(QWidget *widget, QObject *parent)
    : QObject(parent)
    , m_widget(widget)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &ScreenEdgeHelper::restore);
}

ScreenEdgeHelper::~ScreenEdgeHelper() = default;

void ScreenEdgeHelper::moveToTop()
{
    const QRect geo = QGuiApplication::primaryScreen()->geometry();
    m_widget->setGeometry(geo.x(), geo.y(), geo.width(), 100);
}

void ScreenEdgeHelper::moveToRight()
{
    const QRect geo = QGuiApplication::primaryScreen()->geometry();
    m_widget->setGeometry(geo.x(), geo.y(), geo.width(), 100);
}

void ScreenEdgeHelper::moveToBottom()
{
    const QRect geo = QGuiApplication::primaryScreen()->geometry();
    m_widget->setGeometry(geo.x(), geo.y() + geo.height() - 100, geo.width(), 100);
}

void ScreenEdgeHelper::moveToLeft()
{
    const QRect geo = QGuiApplication::primaryScreen()->geometry();
    m_widget->setGeometry(geo.x(), geo.y(), 100, geo.height());
}

void ScreenEdgeHelper::moveToFloating()
{
    const QRect geo = QGuiApplication::primaryScreen()->geometry();
    m_widget->setGeometry(QRect(geo.center(), QSize(100, 100)));
}

ScreenEdgeHelperX11::ScreenEdgeHelperX11(QWidget *widget, QObject *parent)
    : ScreenEdgeHelper(widget, parent)
    , m_atom(QByteArrayLiteral("_KDE_NET_WM_SCREEN_EDGE_SHOW"))
{
}

void ScreenEdgeHelperX11::hide()
{
    uint32_t value = m_locationValue | (m_actionValue << 8);
    xcb_change_property(QX11Info::connection(), XCB_PROP_MODE_REPLACE, window()->winId(), m_atom, XCB_ATOM_CARDINAL, 32, 1, &value);
}

void ScreenEdgeHelperX11::restore()
{
    xcb_delete_property(QX11Info::connection(), window()->winId(), m_atom);
}

void ScreenEdgeHelperX11::raiseOrShow(bool raise)
{
    m_actionValue = raise ? 1: 0;
}

void ScreenEdgeHelperX11::moveToBottom()
{
    ScreenEdgeHelper::moveToBottom();
    m_locationValue = 2;
}

void ScreenEdgeHelperX11::moveToFloating()
{
    ScreenEdgeHelper::moveToFloating();
    m_locationValue = 4;
}

void ScreenEdgeHelperX11::moveToLeft()
{
    ScreenEdgeHelper::moveToLeft();
    m_locationValue = 3;
}

void ScreenEdgeHelperX11::moveToRight()
{
    ScreenEdgeHelper::moveToRight();
    m_locationValue = 1;
}

void ScreenEdgeHelperX11::moveToTop()
{
    ScreenEdgeHelper::moveToTop();
    m_locationValue = 0;
}

using namespace KWayland::Client;

ScreenEdgeHelperWayland::ScreenEdgeHelperWayland(QWidget *widget, QObject *parent)
    : ScreenEdgeHelper(widget, parent)
{
    ConnectionThread *connection = ConnectionThread::fromApplication(this);
    Registry *registry = new Registry(connection);
    registry->create(connection);

    connect(registry, &Registry::interfacesAnnounced, this,
        [registry, this] {
            const auto interface = registry->interface(Registry::Interface::PlasmaShell);
            if (interface.name == 0) {
                return;
            }
            m_shell = registry->createPlasmaShell(interface.name, interface.version);
        }
    );

    registry->setup();
    connection->roundtrip();
}

void ScreenEdgeHelperWayland::init()
{
    window()->installEventFilter(this);
    setupSurface();
}

void ScreenEdgeHelperWayland::setupSurface()
{
    if (!m_shell) {
        return;
    }
    if (auto s = Surface::fromWindow(window())) {
        m_shellSurface = m_shell->createSurface(s, window());
        m_shellSurface->setRole(PlasmaShellSurface::Role::Panel);
        m_shellSurface->setPanelBehavior(PlasmaShellSurface::PanelBehavior::AutoHide);
        m_shellSurface->setPosition(window()->position());
    }
}

void ScreenEdgeHelperWayland::hide()
{
    if (m_shellSurface && m_autoHide) {
        m_shellSurface->requestHideAutoHidingPanel();
    }
}

void ScreenEdgeHelperWayland::restore()
{
    if (m_shellSurface && m_autoHide) {
        m_shellSurface->requestShowAutoHidingPanel();
    }
}

void ScreenEdgeHelperWayland::raiseOrShow(bool raise)
{
    m_autoHide = !raise;
    if (m_shellSurface) {
        if (raise) {
            m_shellSurface->setPanelBehavior(PlasmaShellSurface::PanelBehavior::WindowsCanCover);
        } else {
            m_shellSurface->setPanelBehavior(PlasmaShellSurface::PanelBehavior::AutoHide);
        }
    }
}

bool ScreenEdgeHelperWayland::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != window() || !m_shell) {
        return false;
    }
    if (event->type() == QEvent::PlatformSurface) {
        QPlatformSurfaceEvent *pe = static_cast<QPlatformSurfaceEvent*>(event);
        if (pe->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
            setupSurface();
        } else {
            delete m_shellSurface;
            m_shellSurface = nullptr;
        }
    }
    if (event->type() == QEvent::Move) {
        if (m_shellSurface) {
            m_shellSurface->setPosition(window()->position());
        }
    }
    return false;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationDisplayName(QStringLiteral("Screen Edge Show Test App"));
    ScreenEdgeHelper *helper = nullptr;
    QScopedPointer<QWidget> widget(new QWidget(nullptr, Qt::FramelessWindowHint));
    if (KWindowSystem::isPlatformX11()) {
        app.setProperty("x11Connection", QVariant::fromValue<void*>(QX11Info::connection()));
        helper = new ScreenEdgeHelperX11(widget.data(), &app);
    } else if (KWindowSystem::isPlatformWayland()) {
        helper = new ScreenEdgeHelperWayland(widget.data(), &app);
    }

    if (!helper) {
        return 2;
    }

    QPushButton *hideWindowButton = new QPushButton(QStringLiteral("Hide"), widget.data());

    QObject::connect(hideWindowButton, &QPushButton::clicked, helper, &ScreenEdgeHelper::hide);

    QPushButton *hideAndRestoreButton = new QPushButton(QStringLiteral("Hide and Restore after 10 sec"), widget.data());
    QObject::connect(hideAndRestoreButton, &QPushButton::clicked, helper, &ScreenEdgeHelper::hideAndRestore);

    QToolButton *edgeButton = new QToolButton(widget.data());

    QCheckBox *raiseCheckBox = new QCheckBox("Raise:", widget.data());
    QObject::connect(raiseCheckBox, &QCheckBox::toggled, helper, &ScreenEdgeHelper::raiseOrShow);

    edgeButton->setText(QStringLiteral("Edge"));
    edgeButton->setPopupMode(QToolButton::MenuButtonPopup);
    QMenu *edgeButtonMenu = new QMenu(edgeButton);
    QObject::connect(edgeButtonMenu->addAction("Top"), &QAction::triggered, helper, &ScreenEdgeHelper::moveToTop);
    QObject::connect(edgeButtonMenu->addAction("Right"), &QAction::triggered, helper, &ScreenEdgeHelper::moveToRight);
    QObject::connect(edgeButtonMenu->addAction("Bottom"), &QAction::triggered, helper, &ScreenEdgeHelper::moveToBottom);
    QObject::connect(edgeButtonMenu->addAction("Left"), &QAction::triggered, helper, &ScreenEdgeHelper::moveToLeft);
    edgeButtonMenu->addSeparator();
    QObject::connect(edgeButtonMenu->addAction("Floating"), &QAction::triggered, helper, &ScreenEdgeHelper::moveToFloating);
    edgeButton->setMenu(edgeButtonMenu);

    QHBoxLayout *layout = new QHBoxLayout(widget.data());
    layout->addWidget(hideWindowButton);
    layout->addWidget(hideAndRestoreButton);
    layout->addWidget(edgeButton);
    widget->setLayout(layout);

    const QRect geo = QGuiApplication::primaryScreen()->geometry();
    widget->setGeometry(geo.x(), geo.y() + geo.height() - 100, geo.width(), 100);
    widget->show();
    helper->init();

    return app.exec();
}

#include "screenedgeshowtest.moc"
