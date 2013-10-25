/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <mgraesslin@kde.org>

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
#include <kwinglobals.h>
#include "xcbutils.h"
// tabbox
#include "clientmodel.h"
#include "declarative.h"
#include "desktopmodel.h"
#include "tabboxconfig.h"
// Qt
#include <QKeyEvent>
#include <QModelIndex>
#include <QTimer>
#include <QX11Info>
#include <X11/Xlib.h>
// KDE
#include <KProcess>
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
    QScopedPointer<DeclarativeView> m_declarativeView;
    QScopedPointer<DeclarativeView> m_declarativeDesktopView;
    ClientModel* m_clientModel;
    DesktopModel* m_desktopModel;
    QModelIndex index;
    /**
    * Indicates if the tabbox is shown.
    */
    bool isShown;
    TabBoxClient *lastRaisedClient, *lastRaisedClientSucc;
    WId m_embedded;
    QPoint m_embeddedOffset;
    QSize m_embeddedSize;
    Qt::Alignment m_embeddedAlignment;
    Xcb::Atom m_highlightWindowsAtom;
};

TabBoxHandlerPrivate::TabBoxHandlerPrivate(TabBoxHandler *q)
    : m_declarativeView(NULL)
    , m_declarativeDesktopView(NULL)
    , m_embedded(0)
    , m_embeddedOffset(QPoint(0, 0))
    , m_embeddedSize(QSize(0, 0))
    , m_highlightWindowsAtom(QByteArrayLiteral("_KDE_WINDOW_HIGHLIGHT"))
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
}

ClientModel* TabBoxHandlerPrivate::clientModel() const
{
    return m_clientModel;
}

DesktopModel* TabBoxHandlerPrivate::desktopModel() const
{
    return m_desktopModel;
}

void TabBoxHandlerPrivate::updateHighlightWindows()
{
    if (!isShown || config.tabBoxMode() != TabBoxConfig::ClientTabBox)
        return;

    TabBoxClient *currentClient = q->client(index);
    QWindow *w = NULL;
    if (m_declarativeView && m_declarativeView->isVisible()) {
        w = m_declarativeView.data();
    }

    if (q->isKWinCompositing()) {
        if (lastRaisedClient)
            q->elevateClient(lastRaisedClient, m_declarativeView ? m_declarativeView->winId() : 0, false);
        lastRaisedClient = currentClient;
        if (currentClient)
            q->elevateClient(currentClient, m_declarativeView ? m_declarativeView->winId() : 0, true);
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
            int succIdx = order.count() + 1;
            for (int i=0; i<order.count(); ++i) {
                if (order.at(i).data() == lastRaisedClient) {
                    succIdx = i + 1;
                    break;
                }
            }
            lastRaisedClientSucc = (succIdx < order.count()) ? order.at(succIdx).data() : 0;
            q->raiseClient(lastRaisedClient);
        }
    }

    xcb_window_t wId;
    QVector< xcb_window_t > data;
    if (config.isShowTabBox() && w) {
        wId = w->winId();
        data.resize(2);
        data[ 1 ] = wId;
    } else {
        wId = QX11Info::appRootWindow();
        data.resize(1);
    }
    data[ 0 ] = currentClient ? currentClient->window() : 0L;
    xcb_change_property(connection(), XCB_PROP_MODE_REPLACE, wId, m_highlightWindowsAtom,
                        m_highlightWindowsAtom, 32, data.size(), data.constData());
}

void TabBoxHandlerPrivate::endHighlightWindows(bool abort)
{
    TabBoxClient *currentClient = q->client(index);
    if (currentClient)
        q->elevateClient(currentClient, m_declarativeView ? m_declarativeView->winId() : 0, false);
    if (abort && lastRaisedClient && lastRaisedClientSucc)
        q->restack(lastRaisedClient, lastRaisedClientSucc);
    lastRaisedClient = 0;
    lastRaisedClientSucc = 0;
    // highlight windows
    xcb_delete_property(connection(), config.isShowTabBox() && m_declarativeView ? m_declarativeView->winId() : rootWindow(), m_highlightWindowsAtom);
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
    if (d->config.isShowTabBox()) {
        DeclarativeView *dv(NULL);
        if (d->config.tabBoxMode() == TabBoxConfig::ClientTabBox) {
            // use declarative view
            if (!d->m_declarativeView) {
                d->m_declarativeView.reset(new DeclarativeView(d->clientModel(), TabBoxConfig::ClientTabBox));
            }
            dv = d->m_declarativeView.data();
        } else {
            if (!d->m_declarativeDesktopView) {
                d->m_declarativeDesktopView.reset(new DeclarativeView(d->desktopModel(), TabBoxConfig::DesktopTabBox));
            }
            dv = d->m_declarativeDesktopView.data();
        }
        if (dv->status() == QQuickView::Ready && dv->rootObject()) {
            dv->show();
            dv->setCurrentIndex(d->index, d->config.tabBoxMode() == TabBoxConfig::ClientTabBox);
        } else {
            QStringList args;
            args << QStringLiteral("--passivepopup") << /*i18n*/QStringLiteral("The Window Switcher installation is broken, resources are missing.\n"
                                                 "Contact your distribution about this.") << QStringLiteral("20");
            KProcess::startDetached(QStringLiteral("kdialog"), args);
            hide();
            return;
        }
    }
    if (d->config.isHighlightWindows()) {
        Xcb::sync();
        // TODO this should be
        // QMetaObject::invokeMethod(this, "updateHighlightWindows", Qt::QueuedConnection);
        // but we somehow need to cross > 1 event cycle (likely because of queued invocation in the effects)
        // to ensure the EffectWindow is present when updateHighlightWindows, thus elevating the window/tabbox
        QTimer::singleShot(1, this, SLOT(updateHighlightWindows()));
    }
}

void TabBoxHandler::updateHighlightWindows()
{
    d->updateHighlightWindows();
}

void TabBoxHandler::hide(bool abort)
{
    d->isShown = false;
    if (d->config.isHighlightWindows()) {
        d->endHighlightWindows(abort);
    }
    d->m_declarativeView.reset();
    d->m_declarativeDesktopView.reset();
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
    QWindow *w = NULL;
    if (d->m_declarativeView && d->m_declarativeView->isVisible()) {
        w = d->m_declarativeView.data();
    } else if (d->m_declarativeDesktopView && d->m_declarativeDesktopView->isVisible()) {
        w = d->m_declarativeDesktopView.data();
    } else {
        return false;
    }
    return w->geometry().contains(pos);
}

QModelIndex TabBoxHandler::index(QWeakPointer<KWin::TabBox::TabBoxClient> client) const
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
    case TabBoxConfig::ClientTabBox: {
        d->clientModel()->createClientList(partialReset);
        // TODO: C++11 use lambda function
        bool lastRaised = false;
        bool lastRaisedSucc = false;
        foreach (const QWeakPointer<TabBoxClient> &clientPointer, stackingOrder()) {
            QSharedPointer<TabBoxClient> client = clientPointer.toStrongRef();
            if (!client) {
                continue;
            }
            if (client.data() == d->lastRaisedClient) {
                lastRaised = true;
            }
            if (client.data() == d->lastRaisedClientSucc) {
                lastRaisedSucc = true;
            }
        }
        if (d->lastRaisedClient && !lastRaised)
            d->lastRaisedClient = 0;
        if (d->lastRaisedClientSucc && !lastRaisedSucc)
            d->lastRaisedClientSucc = 0;
        break;
    }
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
