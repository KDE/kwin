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
#include "clientitemdelegate.h"
#include "clientmodel.h"
#include "declarative.h"
#include "desktopitemdelegate.h"
#include "desktopmodel.h"
#include "itemlayoutconfig.h"
#include "tabboxconfig.h"
#include "tabboxview.h"
// Qt
#include <qdom.h>
#include <QtCore/QtConcurrentRun>
#include <QtCore/QFutureWatcher>
#include <QFile>
#include <QKeyEvent>
#include <QModelIndex>
#include <QPainter>
#include <QX11Info>
#include <X11/Xlib.h>
// KDE
#include <KDebug>
#include <KStandardDirs>
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
    void createView();
    void parseConfig(const QString& fileName);

    TabBoxHandler *q; // public pointer
    // members
    TabBoxConfig config;
    TabBoxView* view;
    DeclarativeView *m_declarativeView;
    ClientModel* m_clientModel;
    DesktopModel* m_desktopModel;
    QModelIndex index;
    /**
    * Indicates if the tabbox is shown.
    * Used to determine if the outline has to be updated, etc.
    */
    bool isShown;
    QMap< QString, ItemLayoutConfig > tabBoxLayouts;
    TabBoxClient *lastRaisedClient, *lastRaisedClientSucc;
};

TabBoxHandlerPrivate::TabBoxHandlerPrivate(TabBoxHandler *q)
    : view(NULL)
    , m_declarativeView(NULL)
{
    this->q = q;
    isShown = false;
    lastRaisedClient = 0;
    lastRaisedClientSucc = 0;
    config = TabBoxConfig();
    m_clientModel = new ClientModel(q);
    m_desktopModel = new DesktopModel(q);

    // load the layouts
    QFuture< void> future = QtConcurrent::run(this, &TabBoxHandlerPrivate::parseConfig, KStandardDirs::locate("data", "kwin/DefaultTabBoxLayouts.xml"));
    QFutureWatcher< void > *watcher = new QFutureWatcher< void >(q);
    watcher->setFuture(future);
    q->connect(watcher, SIGNAL(finished()), q, SIGNAL(ready()));
}

TabBoxHandlerPrivate::~TabBoxHandlerPrivate()
{
    delete view;
    delete m_declarativeView;
}

void TabBoxHandlerPrivate::createView()
{
    view = new TabBoxView(m_clientModel, m_desktopModel);
    if (tabBoxLayouts.contains(config.layoutName())) {
        view->clientDelegate()->setConfig(tabBoxLayouts.value(config.layoutName()));
        view->desktopDelegate()->setConfig(tabBoxLayouts.value(config.layoutName()));
    } else {
        view->clientDelegate()->setConfig(tabBoxLayouts.value("Default"));
        view->desktopDelegate()->setConfig(tabBoxLayouts.value("Desktop"));
    }
    if (tabBoxLayouts.contains(config.selectedItemLayoutName())) {
        view->additionalClientDelegate()->setConfig(tabBoxLayouts.value(config.selectedItemLayoutName()));
    } else {
        view->additionalClientDelegate()->setConfig(tabBoxLayouts.value("Text"));
    }
    view->desktopDelegate()->setLayouts(tabBoxLayouts);
    emit q->configChanged();
    view->setCurrentIndex(index);
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
    if (!isShown || clientModel()->data(index, ClientModel::EmptyRole).toBool()) {
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

    if (!KWindowSystem::compositingActive()) {
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
    if (view && view->isVisible()) {
        w = view;
    } else if (m_declarativeView && m_declarativeView->isVisible()) {
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
    if (abort && lastRaisedClient && lastRaisedClientSucc)
        q->restack(lastRaisedClient, lastRaisedClientSucc);
    lastRaisedClient = 0;
    lastRaisedClientSucc = 0;
    // highlight windows
    Display *dpy = QX11Info::display();
    Atom atom = XInternAtom(dpy, "_KDE_WINDOW_HIGHLIGHT", False);
    XDeleteProperty(dpy, config.isShowTabBox() && view ? view->winId() : QX11Info::appRootWindow(), atom);
}

/***********************************************************
* Based on the implementation of Kopete's
* contaclistlayoutmanager.cpp by Nikolaj Hald Nielsen and
* Roman Jarosz
***********************************************************/
void TabBoxHandlerPrivate::parseConfig(const QString& fileName)
{
    // open the file
    if (!QFile::exists(fileName)) {
        kDebug(1212) << "File " << fileName << " does not exist";
        return;
    }
    QDomDocument doc("Layouts");
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        kDebug(1212) << "Error reading file " << fileName;
        return;
    }
    if (!doc.setContent(&file)) {
        kDebug(1212) << "Error parsing file " << fileName;
        file.close();
        return;
    }
    file.close();

    QDomElement layouts_element = doc.firstChildElement("tabbox_layouts");
    QDomNodeList layouts = layouts_element.elementsByTagName("layout");

    for (int i = 0; i < layouts.size(); i++) {
        QDomNode layout = layouts.item(i);
        ItemLayoutConfig currentLayout;

        // parse top elements
        QDomElement element = layout.toElement();
        QString layoutName = element.attribute("name", "");

        const bool highlightIcon = (element.attribute("highlight_selected_icon", "true").compare("true", Qt::CaseInsensitive) == 0);
        currentLayout.setHighlightSelectedIcons(highlightIcon);

        const bool grayscaleIcon = (element.attribute("grayscale_deselected_icon", "false").compare("true", Qt::CaseInsensitive) == 0);
        currentLayout.setGrayscaleDeselectedIcons(grayscaleIcon);

        // rows
        QDomNodeList rows = element.elementsByTagName("row");
        for (int j = 0; j < rows.size(); j++) {
            QDomNode rowNode = rows.item(j);

            ItemLayoutConfigRow row;

            QDomNodeList elements = rowNode.toElement().elementsByTagName("element");
            for (int k = 0; k < elements.size(); k++) {
                QDomNode elementNode = elements.item(k);
                QDomElement currentElement = elementNode.toElement();

                ItemLayoutConfigRowElement::ElementType type = ItemLayoutConfigRowElement::ElementType(currentElement.attribute(
                            "type", int(ItemLayoutConfigRowElement::ElementClientName)).toInt());
                ItemLayoutConfigRowElement currentRowElement;
                currentRowElement.setType(type);

                // width - used by all types
                qreal width = currentElement.attribute("width", "0.0").toDouble();
                currentRowElement.setWidth(width);
                switch(type) {
                case ItemLayoutConfigRowElement::ElementEmpty:
                    row.addElement(currentRowElement);
                    break;
                case ItemLayoutConfigRowElement::ElementIcon: {
                    qreal iconWidth = currentElement.attribute("icon_size", "16.0").toDouble();
                    currentRowElement.setIconSize(QSizeF(iconWidth, iconWidth));
                    currentRowElement.setRowSpan(currentElement.attribute("row_span", "true").compare(
                                                     "true", Qt::CaseInsensitive) == 0);
                    row.addElement(currentRowElement);
                    break;
                }
                case ItemLayoutConfigRowElement::ElementClientList: {
                    currentRowElement.setStretch(currentElement.attribute("stretch", "false").compare(
                                                     "true", Qt::CaseInsensitive) == 0);
                    currentRowElement.setClientListLayoutName(currentElement.attribute("layout_name", ""));
                    QString layoutMode = currentElement.attribute("layout_mode", "horizontal");
                    if (layoutMode.compare("horizontal", Qt::CaseInsensitive) == 0)
                        currentRowElement.setClientListLayoutMode(TabBoxConfig::HorizontalLayout);
                    else if (layoutMode.compare("vertical", Qt::CaseInsensitive) == 0)
                        currentRowElement.setClientListLayoutMode(TabBoxConfig::VerticalLayout);
                    else if (layoutMode.compare("tabular", Qt::CaseInsensitive) == 0)
                        currentRowElement.setClientListLayoutMode(TabBoxConfig::HorizontalVerticalLayout);
                    row.addElement(currentRowElement);
                    break;
                }
                default: { // text elements
                    currentRowElement.setStretch(currentElement.attribute("stretch", "false").compare(
                                                     "true", Qt::CaseInsensitive) == 0);
                    currentRowElement.setSmallTextSize(currentElement.attribute("small", "false").compare(
                                                           "true", Qt::CaseInsensitive) == 0);
                    currentRowElement.setBold(currentElement.attribute("bold", "false").compare(
                                                  "true", Qt::CaseInsensitive) == 0);
                    currentRowElement.setItalic(currentElement.attribute("italic", "false").compare(
                                                    "true", Qt::CaseInsensitive) == 0);
                    currentRowElement.setItalicMinimized(currentElement.attribute("italic_minimized", "true").compare(
                            "true", Qt::CaseInsensitive) == 0);

                    currentRowElement.setPrefix(currentElement.attribute("prefix", ""));
                    currentRowElement.setSuffix(currentElement.attribute("suffix", ""));
                    currentRowElement.setPrefixMinimized(currentElement.attribute("prefix_minimized", ""));
                    currentRowElement.setSuffixMinimzed(currentElement.attribute("suffix_minimized", ""));

                    QString halign = currentElement.attribute("horizontal_alignment", "left");
                    Qt::Alignment alignment;
                    if (halign.compare("left", Qt::CaseInsensitive) == 0)
                        alignment = Qt::AlignLeft;
                    else if (halign.compare("right", Qt::CaseInsensitive) == 0)
                        alignment = Qt::AlignRight;
                    else
                        alignment = Qt::AlignCenter;
                    QString valign = currentElement.attribute("vertical_alignment", "center");
                    if (valign.compare("top", Qt::CaseInsensitive) == 0)
                        alignment = alignment | Qt::AlignTop;
                    else if (valign.compare("bottom", Qt::CaseInsensitive) == 0)
                        alignment = alignment | Qt::AlignBottom;
                    else
                        alignment = alignment | Qt::AlignVCenter;
                    currentRowElement.setAlignment(alignment);

                    row.addElement(currentRowElement);
                    break;
                }// case default
                } // switch type
            } // for loop elements

            currentLayout.addRow(row);
        } // for loop rows
        if (!layoutName.isEmpty()) {
            tabBoxLayouts.insert(layoutName, currentLayout);
        }
    } // for loop layouts
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
    if (config.layoutName() != d->config.layoutName()) {
        // new item layout config
        if (d->tabBoxLayouts.contains(config.layoutName()) && d->view) {
            d->view->clientDelegate()->setConfig(d->tabBoxLayouts.value(config.layoutName()));
            d->view->desktopDelegate()->setConfig(d->tabBoxLayouts.value(config.layoutName()));
        }
    }
    if (config.selectedItemLayoutName() != d->config.selectedItemLayoutName()) {
        // TODO: desktop layouts
        if (d->tabBoxLayouts.contains(config.selectedItemLayoutName()) && d->view)
            d->view->additionalClientDelegate()->setConfig(d->tabBoxLayouts.value(config.selectedItemLayoutName()));
    }
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
                d->m_declarativeView = new DeclarativeView(d->clientModel());
            }
            d->m_declarativeView->show();
            d->m_declarativeView->setCurrentIndex(d->index);
        } else {
            if (!d->view) {
                d->createView();
            }
            d->view->show();
            d->view->updateGeometry();
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
    if (d->view) {
        d->view->hide();
    }
    if (d->m_declarativeView) {
        d->m_declarativeView->hide();
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

int TabBoxHandler::currentSelectedDesktop() const
{
    return desktop(d->index);
}

void TabBoxHandler::setCurrentIndex(const QModelIndex& index)
{
    if (d->index == index) {
        return;
    }
    if (!index.isValid()) {
        return;
    }
    if (d->view) {
        d->view->setCurrentIndex(index);
    }
    if (d->m_declarativeView) {
        d->m_declarativeView->setCurrentIndex(index);
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
}

const QModelIndex& TabBoxHandler::currentIndex() const
{
    return d->index;
}

QModelIndex TabBoxHandler::grabbedKeyEvent(QKeyEvent* event) const
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
    int column = d->index.column();
    int row = d->index.row();
    switch(event->key()) {
    case Qt::Key_Left:
        column--;
        if (column < 0)
            column = model->columnCount() - 1;
        break;
    case Qt::Key_Right:
        column++;
        if (column >= model->columnCount())
            column = 0;
        break;
    case Qt::Key_Up:
        row--;
        if (row < 0)
            row = model->rowCount() - 1;
        break;
    case Qt::Key_Down:
        row++;
        if (row >= model->rowCount())
            row = 0;
        break;
    default:
        // do not do anything for any other key
        break;
    }
    ret = model->index(row, column);

    if (ret.isValid())
        return ret;
    else
        return d->index;
}

bool TabBoxHandler::containsPos(const QPoint& pos) const
{
    QWidget *w = NULL;
    if (d->view && d->view->isVisible()) {
        w = d->view;
    } else if (d->m_declarativeView && d->m_declarativeView->isVisible()) {
        w = d->m_declarativeView;
    } else {
        return false;
    }
    return w->geometry().contains(pos);
}

QModelIndex TabBoxHandler::indexAt(const QPoint& pos) const
{
    if (d->view && d->view->isVisible()) {
        QPoint widgetPos = d->view->mapFromGlobal(pos);
        return d->view->indexAt(widgetPos);
    } else if (d->m_declarativeView && d->m_declarativeView->isVisible()) {
        QPoint widgetPos = d->m_declarativeView->mapFromGlobal(pos);
        return d->m_declarativeView->indexAt(widgetPos);
    }
    return QModelIndex();
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
            (d->config.tabBoxMode() != TabBoxConfig::ClientTabBox) ||
            (d->clientModel()->data(index, ClientModel::EmptyRole).toBool()))
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
    if (d->view) {
        d->view->updateGeometry();
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

QWidget* TabBoxHandler::tabBoxView() const
{
    return d->view;
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
