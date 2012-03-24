/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2011 Martin Gräßlin <mgraesslin@kde.org>

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

#include "thumbnailitem.h"
// KWin
#include "client.h"
#include "effects.h"
#include "workspace.h"
// Qt
#include <QtDeclarative/QDeclarativeContext>
#include <QtDeclarative/QDeclarativeEngine>
#include <QtDeclarative/QDeclarativeView>
// KDE
#include <KDE/KDebug>

namespace KWin
{
ThumbnailItem::ThumbnailItem(QDeclarativeItem* parent)
    : QDeclarativeItem(parent)
    , m_wId(0)
    , m_clip(true)
    , m_parent(QWeakPointer<EffectWindowImpl>())
    , m_parentWindow(0)
{
    setFlags(flags() & ~QGraphicsItem::ItemHasNoContents);
    if (effects) {
        connect(effects, SIGNAL(windowAdded(KWin::EffectWindow*)), SLOT(effectWindowAdded()));
        connect(effects, SIGNAL(windowDamaged(KWin::EffectWindow*,QRect)), SLOT(repaint(KWin::EffectWindow*)));
    }
    QTimer::singleShot(0, this, SLOT(init()));
}

ThumbnailItem::~ThumbnailItem()
{
}

void ThumbnailItem::init()
{
    findParentEffectWindow();
    if (!m_parent.isNull()) {
        m_parent.data()->registerThumbnail(this);
    }
}

void ThumbnailItem::setParentWindow(qulonglong parentWindow)
{
    m_parentWindow = parentWindow;
    findParentEffectWindow();
    if (!m_parent.isNull()) {
        m_parent.data()->registerThumbnail(this);
    }
}

void ThumbnailItem::findParentEffectWindow()
{
    if (effects) {
        if (m_parentWindow) {
            if (EffectWindowImpl *w = static_cast<EffectWindowImpl*>(effects->findWindow(m_parentWindow))) {
                m_parent = QWeakPointer<EffectWindowImpl>(w);
                return;
            }
        }
        QDeclarativeContext *ctx = QDeclarativeEngine::contextForObject(this);
        if (!ctx) {
            kDebug(1212) << "No Context";
            return;
        }
        const QVariant variant = ctx->engine()->rootContext()->contextProperty("viewId");
        if (!variant.isValid()) {
            kDebug(1212) << "Required context property 'viewId' not found";
            return;
        }
        if (EffectWindowImpl *w = static_cast<EffectWindowImpl*>(effects->findWindow(variant.value<qulonglong>()))) {
            m_parent = QWeakPointer<EffectWindowImpl>(w);
            m_parentWindow = variant.value<qulonglong>();
        }
    }
}

void ThumbnailItem::setWId(qulonglong wId)
{
    m_wId = wId;
    emit wIdChanged(wId);
}

void ThumbnailItem::setClip(bool clip)
{
    m_clip = clip;
    emit clipChanged(clip);
}

void ThumbnailItem::effectWindowAdded()
{
    // the window might be added before the EffectWindow is created
    // by using this slot we can register the thumbnail when it is finally created
    if (m_parent.isNull()) {
        findParentEffectWindow();
        if (!m_parent.isNull()) {
            m_parent.data()->registerThumbnail(this);
        }
    }
}

void ThumbnailItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    if (effects) {
        QDeclarativeItem::paint(painter, option, widget);
        return;
    }
    Client *client = Workspace::self()->findClient(WindowMatchPredicate(m_wId));
    if (!client) {
        QDeclarativeItem::paint(painter, option, widget);
        return;
    }
    QPixmap pixmap = client->icon(boundingRect().size().toSize());
    const QSize size(boundingRect().size().toSize() - pixmap.size());
    painter->drawPixmap(boundingRect().adjusted(size.width()/2.0, size.height()/2.0, -size.width()/2.0, -size.height()/2.0).toRect(),
                        pixmap);
}

void ThumbnailItem::repaint(KWin::EffectWindow *w)
{
    if (static_cast<KWin::EffectWindowImpl*>(w)->window()->window() == m_wId) {
        update();
    }
}

} // namespace KWin
