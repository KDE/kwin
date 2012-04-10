/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

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

// own
#include "tabboxhandler.h"
// tabbox
#include "clientmodel.h"
#include "declarative.h"
#include "desktopmodel.h"
#include "tabboxconfig.h"
// Qt
#include <QKeyEvent>
#include <QModelIndex>
#include <QX11Info>
#include <X11/Xlib.h>
// KDE
#include <KDebug>
#include <KWindowSystem>

namespace KWin
{
namespace TabBox
{

class TabBoxHandlerPrivate
{
public:
    TabBoxHandlerPrivate(TabBoxHandler *q);

    ~TabBoxHandlerPrivate();

    /**
    * Updates the currently shown outline.
    */
    void updateOutline();
    /**
    * Updates the current highlight window state
    */
    void updateHighlightWindows();
    /**
    * Ends window highlighting
    */
    void endHighlightWindows(bool abort = false);

    ClientModel* clientModel() const;
    DesktopModel* desktopModel() const;

    TabBoxHandler *q; // public pointer
    // members
    TabBoxConfig config;
    DeclarativeView *m_declarativeView;
    DeclarativeView *m_declarativeDesktopView;
    ClientModel* m_clientModel;
    DesktopModel* m_desktopModel;
    QModelIndex index;
    /**
    * Indicates if the tabbox is shown.
    * Used to determine if the outline has to be updated, etc.
    */
    bool isShown;
    TabBoxClient *lastRaisedClient, *lastRaisedClientSucc;
    WId m_embedded;
    QPoint m_embeddedOffset;
    QSize m_embeddedSize;
    Qt::Alignment m_embeddedAlignment;
};

TabBoxHandlerPrivate::TabBoxHandlerPrivate(TabBoxHandler *q)
    : m_declarativeView(NULL)
    , m_declarativeDesktopView(NULL)
    , m_embedded(0)
    , m_embeddedOffset(QPoint(0, 0))
    , m_embeddedSize(QSize(0, 0))
{
    this->q = q;
    isShown = false;
    lastRaisedClient = 0;
    lastRaisedClientSucc = 0;
    config = TabBoxConfig();
    m_clientModel = new ClientModel(q);
    m_desktopModel = new DesktopModel(q);
}

TabBoxHandlerPrivate::~TabBoxHandlerPrivate()
{
    delete m_declarativeView;
    delete m_declarativeDesktopView;
}

ClientModel* TabBoxHandlerPrivate::clientModel() const
{
    return m_clientModel;
}

DesktopModel* TabBoxHandlerPrivate::desktopModel() const
{
    return m_desktopModel;
}

void TabBoxHandlerPrivate::updateOutline()
{
    if (config.tabBoxMode() != TabBoxConfig::ClientTabBox)
        return;
//     if ( c == NULL || !m_isShown || !c->isShown( true ) || !c->isOnCurrentDesktop())
    if (!isShown) {
        q->hideOutline();
        return;
    }
    TabBoxClient* c = static_cast< TabBoxClient* >(m_clientModel->data(index, ClientModel::ClientRole).value<void *>());
    q->showOutline(QRect(c->x(), c->y(), c->width(), c->height()));
}

void TabBoxHandlerPrivate::updateHighlightWindows()
{
    if (!isShown || config.tabBoxMode() != TabBoxConfig::ClientTabBox)
        return;

    Display *dpy = QX11Info::display();
    TabBoxClient *currentClient = q->client(index);

    if (KWindowSystem::compositingActive()) {
        if (lastRaisedClient)
            q->elevateClient(lastRaisedClient, false);
        lastRaisedClient = currentClient;
        if (currentClient)
            q->elevateClient(currentClient, true);
    } else {
        if (lastRaisedClient) {
            if (lastRaisedClientSucc)
                q->restack(lastRaisedClient, lastRaisedClientSucc);
            // TODO lastRaisedClient->setMinimized( lastRaisedClientWasMinimized );
        }

        lastRaisedClient = currentClient;
        if (lastRaisedClient) {
            // TODO if ( (lastRaisedClientWasMinimized = lastRaisedClient->isMinimized()) )
            //         lastRaisedClient->setMinimized( false );
            TabBoxClientList order = q->stackingOrder();
            int succIdx = order.indexOf(lastRaisedClient) + 1;   // this is likely related to the index parameter?!
            lastRaisedClientSucc = (succIdx < order.count()) ? order.at(succIdx) : 0;
            q->raiseClient(lastRaisedClient);
        }
    }

    WId wId;
    QVector< WId > data;
    QWidget *w = NULL;
    if (m_declarativeView && m_declarativeView->isVisible()) {
        w = m_declarativeView;
    }
    if (config.isShowTabBox() && w) {
        wId = w->winId();
        data.resize(2);
        data[ 1 ] = wId;
    } else {
        wId = QX11Info::appRootWindow();
        data.resize(1);
    }
    data[ 0 ] = currentClient ? currentClient->window() : 0L;
    if (config.isShowOutline()) {
        QVector<Window> outlineWindows = q->outlineWindowIds();
        data.resize(2+outlineWindows.size());
        for (int i=0; i<outlineWindows.size(); ++i) {
            data[2+i] = outlineWindows[i];
        }
    }
    Atom atom = XInternAtom(dpy, "_KDE_WINDOW_HIGHLIGHT", False);
    XChangeProperty(dpy, wId, atom, atom, 32, PropModeReplace,
                    reinterpret_cast<unsigned char *>(data.data()), data.size());
}

void TabBoxHandlerPrivate::endHighlightWindows(bool abort)
{
    TabBoxClient *currentClient = q->client(index);
    if (currentClient)
        q->elevateClient(currentClient, false);
    if (abort && lastRaisedClient && lastRaisedClientSucc)
        q->restack(lastRaisedClient, lastRaisedClientSucc);
    lastRaisedClient = 0;
    lastRaisedClientSucc = 0;
    // highlight windows
    Display *dpy = QX11Info::display();
    Atom atom = XInternAtom(dpy, "_KDE_WINDOW_HIGHLIGHT", False);
    XDeleteProperty(dpy, config.isShowTabBox() && m_declarativeView ? m_declarativeView->winId() : QX11Info::appRootWindow(), atom);
}

/***********************************************
* TabBoxHandler
***********************************************/

TabBoxHandler::TabBoxHandler()
    : QObject()
{
    KWin::TabBox::tabBox = this;
    d = new TabBoxHandlerPrivate(this);
}

TabBoxHandler::~TabBoxHandler()
{
    delete d;
}

const KWin::TabBox::TabBoxConfig& TabBoxHandler::config() const
{
    return d->config;
}

void TabBoxHandler::setConfig(const TabBoxConfig& config)
{
    d->config = config;
    emit configChanged();
}

void TabBoxHandler::show()
{
    d->isShown = true;
    d->lastRaisedClient = 0;
    d->lastRaisedClientSucc = 0;
    // show the outline
    if (d->config.isShowOutline()) {
        d->updateOutline();
    }
    if (d->config.isShowTabBox()) {
        if (d->config.tabBoxMode() == TabBoxConfig::ClientTabBox) {
            // use declarative view
            if (!d->m_declarativeView) {
                d->m_declarativeView = new DeclarativeView(d->clientModel(), TabBoxConfig::ClientTabBox);
            }
            d->m_declarativeView->show();
            d->m_declarativeView->setCurrentIndex(d->index, true);
        } else {
            if (!d->m_declarativeDesktopView) {
                d->m_declarativeDesktopView = new DeclarativeView(d->desktopModel(), TabBoxConfig::DesktopTabBox);
            }
            d->m_declarativeDesktopView->show();
            d->m_declarativeDesktopView->setCurrentIndex(d->index);
        }
    }
    if (d->config.isHighlightWindows()) {
        d->updateHighlightWindows();
    }
}

void TabBoxHandler::hide(bool abort)
{
    d->isShown = false;
    if (d->config.isHighlightWindows()) {
        d->endHighlightWindows(abort);
    }
    if (d->config.isShowOutline()) {
        hideOutline();
    }
    if (d->m_declarativeView) {
        d->m_declarativeView->hide();
    }
    if (d->m_declarativeDesktopView) {
        d->m_declarativeDesktopView->hide();
    }
}

QModelIndex TabBoxHandler::nextPrev(bool forward) const
{
    QModelIndex ret;
    QAbstractItemModel* model;
    switch(d->config.tabBoxMode()) {
    case TabBoxConfig::ClientTabBox:
        model = d->clientModel();
        break;
    case TabBoxConfig::DesktopTabBox:
        model = d->desktopModel();
        break;
    default:
        return d->index;
    }
    if (forward) {
        int column = d->index.column() + 1;
        int row = d->index.row();
        if (column == model->columnCount()) {
            column = 0;
            row++;
            if (row == model->rowCount())
                row = 0;
        }
        ret = model->index(row, column);
        if (!ret.isValid())
            ret = model->index(0, 0);
    } else {
        int column = d->index.column() - 1;
        int row = d->index.row();
        if (column < 0) {
            column = model->columnCount() - 1;
            row--;
            if (row < 0)
                row = model->rowCount() - 1;
        }
        ret = model->index(row, column);
        if (!ret.isValid()) {
            row = model->rowCount() - 1;
            for (int i = model->columnCount() - 1; i >= 0; i--) {
                ret = model->index(row, i);
                if (ret.isValid())
                    break;
            }
        }
    }
    if (ret.isValid())
        return ret;
    else
        return d->index;
}

QModelIndex TabBoxHandler::desktopIndex(int desktop) const
{
    if (d->config.tabBoxMode() != TabBoxConfig::DesktopTabBox)
        return QModelIndex();
    return d->desktopModel()->desktopIndex(desktop);
}

QList< int > TabBoxHandler::desktopList() const
{
    if (d->config.tabBoxMode() != TabBoxConfig::DesktopTabBox)
        return QList< int >();
    return d->desktopModel()->desktopList();
}

int TabBoxHandler::desktop(const QModelIndex& index) const
{
    if (!index.isValid() || (d->config.tabBoxMode() != TabBoxConfig::DesktopTabBox))
        return -1;
    QVariant ret = d->desktopModel()->data(index, DesktopModel::DesktopRole);
    if (ret.isValid())
        return ret.toInt();
    else
        return -1;
}

void TabBoxHandler::setCurrentIndex(const QModelIndex& index)
{
    if (d->index == index) {
        return;
    }
    if (!index.isValid()) {
        return;
    }
    if (d->m_declarativeView) {
        d->m_declarativeView->setCurrentIndex(index);
    }
    if (d->m_declarativeDesktopView) {
        d->m_declarativeDesktopView->setCurrentIndex(index);
    }
    d->index = index;
    if (d->config.tabBoxMode() == TabBoxConfig::ClientTabBox) {
        if (d->config.isShowOutline()) {
            d->updateOutline();
        }
        if (d->config.isHighlightWindows()) {
            d->updateHighlightWindows();
        }
    }
    emit selectedIndexChanged();
}

const QModelIndex& TabBoxHandler::currentIndex() const
{
    return d->index;
}

void TabBoxHandler::grabbedKeyEvent(QKeyEvent* event) const
{
    if (d->m_declarativeView && d->m_declarativeView->isVisible()) {
        d->m_declarativeView->sendKeyEvent(event);
    } else if (d->m_declarativeDesktopView && d->m_declarativeDesktopView->isVisible()) {
        d->m_declarativeDesktopView->sendKeyEvent(event);
    }
}

bool TabBoxHandler::containsPos(const QPoint& pos) const
{
    QWidget *w = NULL;
    if (d->m_declarativeView && d->m_declarativeView->isVisible()) {
        w = d->m_declarativeView;
    } else if (d->m_declarativeDesktopView && d->m_declarativeDesktopView->isVisible()) {
        w = d->m_declarativeDesktopView;
    } else {
        return false;
    }
    return w->geometry().contains(pos);
}

QModelIndex TabBoxHandler::index(KWin::TabBox::TabBoxClient* client) const
{
    return d->clientModel()->index(client);
}

TabBoxClientList TabBoxHandler::clientList() const
{
    if (d->config.tabBoxMode() != TabBoxConfig::ClientTabBox)
        return TabBoxClientList();
    return d->clientModel()->clientList();
}

TabBoxClient* TabBoxHandler::client(const QModelIndex& index) const
{
    if ((!index.isValid()) ||
            (d->config.tabBoxMode() != TabBoxConfig::ClientTabBox))
        return NULL;
    TabBoxClient* c = static_cast< TabBoxClient* >(
                          d->clientModel()->data(index, ClientModel::ClientRole).value<void *>());
    return c;
}

void TabBoxHandler::createModel(bool partialReset)
{
    switch(d->config.tabBoxMode()) {
    case TabBoxConfig::ClientTabBox:
        d->clientModel()->createClientList(partialReset);
        if (d->lastRaisedClient && !stackingOrder().contains(d->lastRaisedClient))
            d->lastRaisedClient = 0;
        if (d->lastRaisedClientSucc && !stackingOrder().contains(d->lastRaisedClientSucc))
            d->lastRaisedClientSucc = 0;
        break;
    case TabBoxConfig::DesktopTabBox:
        d->desktopModel()->createDesktopList();
        break;
    }
}

QModelIndex TabBoxHandler::first() const
{
    QAbstractItemModel* model;
    switch(d->config.tabBoxMode()) {
    case TabBoxConfig::ClientTabBox:
        model = d->clientModel();
        break;
    case TabBoxConfig::DesktopTabBox:
        model = d->desktopModel();
        break;
    default:
        return QModelIndex();
    }
    return model->index(0, 0);
}

WId TabBoxHandler::embedded() const
{
    return d->m_embedded;
}

void TabBoxHandler::setEmbedded(WId wid)
{
    d->m_embedded = wid;
    emit embeddedChanged(wid != 0);
}

void TabBoxHandler::setEmbeddedOffset(const QPoint &offset)
{
    d->m_embeddedOffset = offset;
}

void TabBoxHandler::setEmbeddedSize(const QSize &size)
{
    d->m_embeddedSize = size;
}

const QPoint &TabBoxHandler::embeddedOffset() const
{
    return d->m_embeddedOffset;
}

const QSize &TabBoxHandler::embeddedSize() const
{
    return d->m_embeddedSize;
}

Qt::Alignment TabBoxHandler::embeddedAlignment() const
{
    return d->m_embeddedAlignment;
}

void TabBoxHandler::setEmbeddedAlignment(Qt::Alignment alignment)
{
    d->m_embeddedAlignment = alignment;
}

void TabBoxHandler::resetEmbedded()
{
    if (d->m_embedded == 0) {
        return;
    }
    d->m_embedded = 0;
    d->m_embeddedOffset = QPoint(0, 0);
    d->m_embeddedSize = QSize(0, 0);
    emit embeddedChanged(false);
}

TabBoxHandler* tabBox = 0;

TabBoxClient::TabBoxClient()
{
}

TabBoxClient::~TabBoxClient()
{
}

} // namespace TabBox
} // namespace KWin
