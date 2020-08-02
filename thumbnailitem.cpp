/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2011 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "thumbnailitem.h"
// KWin
#include "x11client.h"
#include "composite.h"
#include "effects.h"
#include "workspace.h"
#include "wayland_server.h"
// Qt
#include <QDebug>
#include <QPainter>
#include <QQuickWindow>

namespace KWin
{

AbstractThumbnailItem::AbstractThumbnailItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
    , m_brightness(1.0)
    , m_saturation(1.0)
    , m_clipToItem()
{
    connect(Compositor::self(), SIGNAL(compositingToggled(bool)), SLOT(compositingToggled()));
    compositingToggled();
    QTimer::singleShot(0, this, SLOT(init()));
}

AbstractThumbnailItem::~AbstractThumbnailItem()
{
}

void AbstractThumbnailItem::compositingToggled()
{
    m_parent.clear();
    if (effects) {
        connect(effects, SIGNAL(windowAdded(KWin::EffectWindow*)), SLOT(effectWindowAdded()));
        connect(effects, SIGNAL(windowDamaged(KWin::EffectWindow*,QRect)), SLOT(repaint(KWin::EffectWindow*)));
        effectWindowAdded();
    }
}

void AbstractThumbnailItem::init()
{
    findParentEffectWindow();
    if (m_parent) {
        m_parent->registerThumbnail(this);
    }
}

void AbstractThumbnailItem::findParentEffectWindow()
{
    if (effects) {
        QQuickWindow *qw = window();
        if (!qw) {
            qCDebug(KWIN_CORE) << "No QQuickWindow assigned yet";
            return;
        }
        if (auto *w = static_cast<EffectWindowImpl*>(effects->findWindow(qw))) {
            m_parent = QPointer<EffectWindowImpl>(w);
        }
    }
}

void AbstractThumbnailItem::effectWindowAdded()
{
    // the window might be added before the EffectWindow is created
    // by using this slot we can register the thumbnail when it is finally created
    if (m_parent.isNull()) {
        findParentEffectWindow();
        if (m_parent) {
            m_parent->registerThumbnail(this);
        }
    }
}

void AbstractThumbnailItem::setBrightness(qreal brightness)
{
    if (qFuzzyCompare(brightness, m_brightness)) {
        return;
    }
    m_brightness = brightness;
    update();
    emit brightnessChanged();
}

void AbstractThumbnailItem::setSaturation(qreal saturation)
{
    if (qFuzzyCompare(saturation, m_saturation)) {
        return;
    }
    m_saturation = saturation;
    update();
    emit saturationChanged();
}

void AbstractThumbnailItem::setClipTo(QQuickItem *clip)
{
    m_clipToItem = QPointer<QQuickItem>(clip);
    emit clipToChanged();
}

WindowThumbnailItem::WindowThumbnailItem(QQuickItem* parent)
    : AbstractThumbnailItem(parent)
    , m_wId(nullptr)
    , m_client(nullptr)
{
}

WindowThumbnailItem::~WindowThumbnailItem()
{
}

void WindowThumbnailItem::setWId(const QUuid &wId)
{
    if (m_wId == wId) {
        return;
    }
    m_wId = wId;
    if (m_wId != nullptr) {
        setClient(workspace()->findAbstractClient([this] (const AbstractClient *c) { return c->internalId() == m_wId; }));
    } else if (m_client) {
        m_client = nullptr;
        emit clientChanged();
    }
    emit wIdChanged(wId);
}

void WindowThumbnailItem::setClient(AbstractClient *client)
{
    if (m_client == client) {
        return;
    }
    m_client = client;
    if (m_client) {
        setWId(m_client->internalId());
    } else {
        setWId({});
    }
    emit clientChanged();
}

void WindowThumbnailItem::paint(QPainter *painter)
{
    if (effects) {
        return;
    }
    auto client = workspace()->findAbstractClient([this] (const AbstractClient *c) { return c->internalId() == m_wId; });
    if (!client) {
        return;
    }
    QPixmap pixmap = client->icon().pixmap(boundingRect().size().toSize());
    const QSize size(boundingRect().size().toSize() - pixmap.size());
    painter->drawPixmap(boundingRect().adjusted(size.width()/2.0, size.height()/2.0, -size.width()/2.0, -size.height()/2.0).toRect(),
                        pixmap);
}

void WindowThumbnailItem::repaint(KWin::EffectWindow *w)
{
    if (static_cast<KWin::EffectWindowImpl*>(w)->window()->internalId() == m_wId) {
        update();
    }
}

DesktopThumbnailItem::DesktopThumbnailItem(QQuickItem *parent)
    : AbstractThumbnailItem(parent)
    , m_desktop(0)
{
}

DesktopThumbnailItem::~DesktopThumbnailItem()
{
}

void DesktopThumbnailItem::setDesktop(int desktop)
{
    desktop = qBound<int>(1, desktop, VirtualDesktopManager::self()->count());
    if (desktop == m_desktop) {
        return;
    }
    m_desktop = desktop;
    update();
    emit desktopChanged(m_desktop);
}

void DesktopThumbnailItem::paint(QPainter *painter)
{
    Q_UNUSED(painter)
    if (effects) {
        return;
    }
    // TODO: render icon
}

void DesktopThumbnailItem::repaint(EffectWindow *w)
{
    if (w->isOnDesktop(m_desktop)) {
        update();
    }
}

} // namespace KWin
